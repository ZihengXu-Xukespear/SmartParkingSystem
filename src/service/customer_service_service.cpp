#include "customer_service_service.h"
#include "llm_client.h"
#include "../config.h"
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <iostream>

// ---------- helpers ----------
static std::string scalar(MYSQL* mysql, const std::string& sql) {
    if (mysql_query(mysql, sql.c_str()) != 0) return "";
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return "";
    MYSQL_ROW row = mysql_fetch_row(res);
    std::string v = (row && row[0]) ? row[0] : "";
    mysql_free_result(res);
    return v;
}

static std::string trim(std::string s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static crow::json::wvalue msgSystem(const std::string& c) {
    crow::json::wvalue m; m["role"] = "system"; m["content"] = c; return m;
}
static crow::json::wvalue msgUser(const std::string& c) {
    crow::json::wvalue m; m["role"] = "user"; m["content"] = c; return m;
}
static crow::json::wvalue msgAssistantText(const std::string& c) {
    crow::json::wvalue m; m["role"] = "assistant"; m["content"] = c; return m;
}
static crow::json::wvalue msgAssistantTools(const std::string& c, const std::vector<LlmToolCall>& calls) {
    crow::json::wvalue m;
    m["role"] = "assistant";
    m["content"] = c;
    std::vector<crow::json::wvalue> tcs;
    for (const auto& tc : calls) {
        crow::json::wvalue t;
        t["id"] = tc.id;
        t["type"] = "function";
        t["function"]["name"] = tc.name;
        t["function"]["arguments"] = tc.arguments;
        tcs.push_back(std::move(t));
    }
    m["tool_calls"] = std::move(tcs);
    return m;
}
static crow::json::wvalue msgTool(const std::string& toolCallId, const std::string& content) {
    crow::json::wvalue m;
    m["role"] = "tool";
    m["tool_call_id"] = toolCallId;
    m["content"] = content;
    return m;
}

// ---------- billing estimate (mirrors BillingService per-day cap logic) ----------
struct BillingRuleLite {
    bool valid = false;
    std::string rule_type;
    int free_minutes = 30;
    double hourly_rate = 5.0;
    double max_daily_fee = 0.0;
    std::string tier_config;
};

static BillingRuleLite loadRule(MYSQL* mysql, const std::string& ruleType) {
    BillingRuleLite r;
    r.rule_type = ruleType.empty() ? "standard" : ruleType;
    std::string sql = "SELECT free_minutes,hourly_rate,IFNULL(max_daily_fee,0),IFNULL(tier_config,'') "
                      "FROM BILLING_RULE WHERE rule_type='" + r.rule_type + "' AND is_active=1 ORDER BY id LIMIT 1";
    if (mysql_query(mysql, sql.c_str()) != 0) return r;
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) return r;
    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        r.valid = true;
        r.free_minutes = row[0] ? std::stoi(row[0]) : 30;
        r.hourly_rate = row[1] ? std::stod(row[1]) : 5.0;
        r.max_daily_fee = row[2] ? std::stod(row[2]) : 0.0;
        r.tier_config = row[3] ? row[3] : "";
    }
    mysql_free_result(res);
    return r;
}

static double computeFee(const BillingRuleLite& r, int totalMinutes) {
    if (totalMinutes <= r.free_minutes) return 0.0;
    double chargeable = (double)(totalMinutes - r.free_minutes);
    double hours = std::ceil(chargeable / 60.0);
    if (hours < 1.0) hours = 1.0;

    double fee = 0.0;
    if (r.rule_type == "tiered" && !r.tier_config.empty()) {
        auto tiers = crow::json::load(r.tier_config);
        double remaining = hours;
        double prev = 0.0;
        if (tiers) {
            int n = (int)tiers.size();
            for (int i = 0; i < n && remaining > 0.0001; ++i) {
                double thr = tiers[i]["hours"].d();
                double rate = tiers[i]["rate"].d();
                double span = std::max(0.0, thr - prev);
                prev = thr;
                double take = std::min(remaining, span);
                fee += take * rate;
                remaining -= take;
            }
        } else {
            fee = hours * r.hourly_rate;
        }
    } else {
        fee = hours * r.hourly_rate;
    }

    if (r.max_daily_fee > 0) {
        double days = std::ceil(hours / 24.0);
        if (days < 1.0) days = 1.0;
        double cap = days * r.max_daily_fee;
        if (fee > cap) fee = cap;
    }
    fee = std::round(fee * 100.0) / 100.0;
    return fee;
}

// ---------- row mapping ----------
CSMessage CustomerServiceService::mapRow(MYSQL_ROW row) {
    CSMessage m;
    m.id = row[0] ? std::stoi(row[0]) : 0;
    m.session_id = row[1] ? std::stoi(row[1]) : 0;
    m.user_id = row[2] ? std::stoi(row[2]) : 0;
    m.sender_type = row[3] ? row[3] : "user";
    m.sender_id = row[4] ? std::stoi(row[4]) : 0;
    m.content = row[5] ? row[5] : "";
    m.created_at = row[6] ? row[6] : "";
    m.is_read_by_admin = row[7] && std::stoi(row[7]) != 0;
    m.is_read_by_user = row[8] && std::stoi(row[8]) != 0;
    return m;
}

// ---------- session helpers ----------
int CustomerServiceService::getActiveSessionId(int userId) {
    auto conn = getConnection();
    if (!conn) return 0;
    std::string sql = "SELECT id FROM CS_SESSION WHERE user_id=" + std::to_string(userId) +
        " AND status<>'closed' ORDER BY id DESC LIMIT 1";
    std::string v = scalar(conn->get(), sql);
    return v.empty() ? 0 : std::stoi(v);
}

int CustomerServiceService::getOrCreateSession(int userId) {
    int sid = getActiveSessionId(userId);
    if (sid > 0) return sid;
    auto conn = getConnection();
    if (!conn) return 0;
    MYSQL* mysql = conn->get();
    std::string sql = "INSERT INTO CS_SESSION (user_id,status) VALUES (" +
        std::to_string(userId) + ",'ai')";
    if (!executeQuery(mysql, sql)) return 0;
    return (int)mysql_insert_id(mysql);
}

std::string CustomerServiceService::getSessionStatus(int sessionId) {
    auto conn = getConnection();
    if (!conn) return "";
    return scalar(conn->get(), "SELECT status FROM CS_SESSION WHERE id=" + std::to_string(sessionId));
}

bool CustomerServiceService::setSessionStatus(int sessionId, const std::string& status, int handledBy) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();
    std::string sql = "UPDATE CS_SESSION SET status='" + status + "'";
    if (status == "escalated") sql += ", escalated_at=NOW()";
    if (handledBy > 0) sql += ", handled_by=" + std::to_string(handledBy);
    sql += " WHERE id=" + std::to_string(sessionId);
    return executeQuery(mysql, sql);
}

bool CustomerServiceService::appendMessage(int sessionId, int userId, const std::string& senderType,
                                           int senderId, const std::string& content, bool unreadByAdmin) {
    auto conn = getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();
    std::string sql = "INSERT INTO CS_MESSAGE (session_id,user_id,sender_type,sender_id,content,is_read_by_admin) VALUES (" +
        std::to_string(sessionId) + "," + std::to_string(userId) + ",'" + senderType + "'," +
        std::to_string(senderId) + ",'" + escape(mysql, content) + "'," + (unreadByAdmin ? "0" : "1") + ")";
    if (!executeQuery(mysql, sql)) return false;
    executeQuery(mysql, "UPDATE CS_SESSION SET last_message_at=NOW() WHERE id=" + std::to_string(sessionId));
    return true;
}

void CustomerServiceService::touchSession(int sessionId) {
    auto conn = getConnection();
    if (!conn) return;
    executeQuery(conn->get(), "UPDATE CS_SESSION SET last_message_at=NOW() WHERE id=" + std::to_string(sessionId));
}

// ---------- user-facing ----------
AskResult CustomerServiceService::ask(int userId, const std::string& textRaw) {
    AskResult ar;
    std::string text = trim(textRaw);
    if (text.empty()) { ar.error = "消息内容不能为空"; return ar; }

    int sid = getOrCreateSession(userId);
    if (sid <= 0) { ar.error = "会话创建失败"; return ar; }
    std::string status = getSessionStatus(sid);

    // user messages in human mode are unread by the admin; in AI mode they are self-read
    appendMessage(sid, userId, "user", userId, text, status != "ai");

    // set a title from the first message
    {
        auto conn = getConnection();
        if (conn) {
            std::string t = text.substr(0, 30);
            std::string sql = "UPDATE CS_SESSION SET title='" + escape(conn->get(), t) +
                "' WHERE id=" + std::to_string(sid) + " AND (title IS NULL OR title='')";
            executeQuery(conn->get(), sql);
        }
    }

    ar.session_id = sid;

    if (status == "ai") {
        std::string reply = runAgent(sid, userId);
        bool aiFailed = reply.empty();
        if (aiFailed) {
            // Ephemeral fallback shown to the user only; NOT persisted as a turn,
            // so the next attempt has clean context and we avoid a retry flood.
            reply = "抱歉，AI 客服暂时开小差了，请稍后重试，或点击下方「转接人工客服」由人工为您处理。";
        } else {
            appendMessage(sid, userId, "assistant", 0, reply, true);
            auto conn = getConnection();
            if (conn) executeQuery(conn->get(),
                "UPDATE CS_SESSION SET ai_turn_count=ai_turn_count+1 WHERE id=" + std::to_string(sid));
        }
        ar.mode = "ai";
        ar.reply = reply;
        ar.show_actions = true;
        ar.status = "ai";
        ar.ok = true;
    } else {
        // human mode: message is queued for an admin
        ar.mode = "human";
        ar.reply = "";
        ar.show_actions = false;
        ar.status = status;
        ar.ok = true;
    }
    return ar;
}

bool CustomerServiceService::escalate(int userId, std::string& error) {
    int sid = getActiveSessionId(userId);
    if (sid <= 0) { error = "没有进行中的会话"; return false; }
    std::string status = getSessionStatus(sid);
    if (status == "closed") { error = "会话已关闭"; return false; }
    if (status != "escalated" && status != "handled") {
        setSessionStatus(sid, "escalated", 0);
        appendMessage(sid, userId, "system", 0,
            "已为您转接人工客服，客服人员将尽快与您联系，请稍候…", true);
    }
    return true;
}

crow::json::wvalue CustomerServiceService::getSession(int userId) {
    crow::json::wvalue res;
    int sid = getActiveSessionId(userId);
    res["session_id"] = sid;
    res["status"] = sid > 0 ? getSessionStatus(sid) : "ai";
    res["ai_turn_count"] = 0;

    std::vector<CSMessage> msgs;
    if (sid > 0) {
        auto conn = getConnection();
        if (conn) {
            std::string s = "SELECT COUNT(*) FROM CS_SESSION WHERE id=" + std::to_string(sid);
            std::string ac = scalar(conn->get(), "SELECT ai_turn_count FROM CS_SESSION WHERE id=" + std::to_string(sid));
            res["ai_turn_count"] = ac.empty() ? 0 : std::stoi(ac);
        }
        msgs = list("SELECT id,session_id,user_id,sender_type,sender_id,content,created_at,is_read_by_admin,is_read_by_user "
                    "FROM CS_MESSAGE WHERE session_id=" + std::to_string(sid) + " ORDER BY id ASC LIMIT 200");
        markReadByUser(userId);
    }

    std::vector<crow::json::wvalue> arr;
    for (auto& m : msgs) arr.push_back(m.serialize());
    res["messages"] = std::move(arr);
    return res;
}

bool CustomerServiceService::markReadByUser(int userId) {
    int sid = getActiveSessionId(userId);
    if (sid <= 0) return false;
    auto conn = getConnection();
    if (!conn) return false;
    return executeQuery(conn->get(),
        "UPDATE CS_MESSAGE SET is_read_by_user=1 WHERE session_id=" + std::to_string(sid) +
        " AND sender_type IN ('admin','system') AND is_read_by_user=0");
}

// ---------- agent ----------
static const char* TOOLS_JSON = R"JSON([
 {"type":"function","function":{"name":"get_my_balance","description":"查询当前登录用户的钱包余额","parameters":{"type":"object","properties":{}}}},
 {"type":"function","function":{"name":"get_my_vehicles","description":"查询当前登录用户名下绑定的车牌列表","parameters":{"type":"object","properties":{}}}},
 {"type":"function","function":{"name":"get_my_parking_status","description":"查询用户车辆当前是否在停车场内、入场时间、已停时长和预估费用。可选传入plate指定某辆车。","parameters":{"type":"object","properties":{"plate":{"type":"string","description":"车牌号(可选)"}}}}},
 {"type":"function","function":{"name":"estimate_parking_fee","description":"按指定计费规则预估某一时长的停车费用","parameters":{"type":"object","properties":{"minutes":{"type":"integer","description":"停车分钟数"},"rule":{"type":"string","description":"计费规则类型","enum":["standard","tiered","member"]}},"required":["minutes"]}}},
 {"type":"function","function":{"name":"get_my_monthly_pass","description":"查询用户当前有效的月卡/季卡/年卡","parameters":{"type":"object","properties":{}}}},
 {"type":"function","function":{"name":"check_blacklist","description":"查询某车牌是否在黑名单中","parameters":{"type":"object","properties":{"plate":{"type":"string","description":"车牌号"}},"required":["plate"]}}},
 {"type":"function","function":{"name":"get_pricing","description":"查询停车场当前的全部计费规则和套餐价格","parameters":{"type":"object","properties":{}}}},
 {"type":"function","function":{"name":"get_occupancy","description":"查询停车场车位占用情况(总车位/已停/预约/空闲)","parameters":{"type":"object","properties":{}}}}
])JSON";

std::string CustomerServiceService::buildSystemPrompt(int userId) {
    std::string lot = AppConfig::instance().parking_name;
    if (lot.empty()) lot = "智慧停车场";
    std::string p =
        u8"你是「小智」，" + lot + u8"的AI智能客服。你熟悉本停车场的全部业务，语气亲和、专业、简洁，"
        u8"像一位热心的真人客服，称呼用户为「您」，可适当使用emoji但不过度。\n\n"
        u8"【你的能力】你可以调用工具查询用户的实时信息。涉及用户个人数据时必须先调用工具，绝不能凭空编造数字：\n"
        u8"- get_my_balance：钱包余额\n- get_my_vehicles：名下车牌\n"
        u8"- get_my_parking_status：车辆是否在场、入场时间、已停时长、预估费用\n"
        u8"- estimate_parking_fee：按规则预估某时长费用\n"
        u8"- get_my_monthly_pass：有效的月卡/季卡/年卡\n"
        u8"- check_blacklist：黑名单查询\n- get_pricing：当前计费规则与套餐价格\n"
        u8"- get_occupancy：车位余量\n"
        u8"用户问到这些时，先调用工具拿到真实数据，再据此回答。\n\n"
        u8"【本停车场常识(具体以 get_pricing 工具返回为准)】\n"
        u8"- 计费：普通计费 30分钟免费，之后5元/小时，每日封顶50元；另有阶梯计费、会员计费。\n"
        u8"- 套餐：月卡300元(30天)、季卡800元(90天)、年卡2880元(365天)。\n"
        u8"- 车位总量约100个；预约车位30分钟内未入场会自动过期。\n"
        u8"- 入场：识别车牌后自动抬杆入场；出场：识别后自动结算扣费抬杆出场。\n"
        u8"- 充值：在「个人中心-钱包」充值，出场时自动从余额扣费。\n"
        u8"- 营业时间：24小时营业。\n"
        u8"- 车牌格式：省份汉字+字母+5位(如 鲁B12345)；新能源为6位。\n\n"
        u8"【边界与转人工】你不能直接办卡/充值/开闸/退款，这些需用户在系统操作或由人工处理。"
        u8"遇到退款、投诉、纠纷、设备/道闸故障、紧急情况、辱骂，或你两次仍无法解决的问题，"
        u8"主动建议用户点击「转接人工客服」。最终是否转人工由用户决定。\n\n"
        u8"【格式】回答简短(一般2-4句或少量分点)，先直接回答再补充，不要长篇大论。\n\n"
        u8"【当前用户】user_id=" + std::to_string(userId) + u8"。";
    return p;
}

std::vector<crow::json::wvalue> CustomerServiceService::buildContextMessages(int sessionId, int userId) {
    std::vector<crow::json::wvalue> msgs;
    msgs.push_back(msgSystem(buildSystemPrompt(userId)));

    // last 10 user/assistant turns (newest first, then reverse) for continuity
    auto recent = list("SELECT id,session_id,user_id,sender_type,sender_id,content,created_at,is_read_by_admin,is_read_by_user "
                       "FROM CS_MESSAGE WHERE session_id=" + std::to_string(sessionId) +
                       " AND sender_type IN ('user','assistant') ORDER BY id DESC LIMIT 10");
    std::reverse(recent.begin(), recent.end());
    for (auto& m : recent) {
        if (m.sender_type == "user") msgs.push_back(msgUser(m.content));
        else msgs.push_back(msgAssistantText(m.content));
    }
    return msgs;
}

std::string CustomerServiceService::runAgent(int sessionId, int userId) {
    std::vector<crow::json::wvalue> msgs = buildContextMessages(sessionId, userId);
    std::string reply;
    std::string lastError;

    for (int iter = 0; iter < 5; ++iter) {
        LlmResponse r = LlmClient::complete(msgs, TOOLS_JSON);
        if (!r.ok) { lastError = r.error; break; }

        if (r.tool_calls.empty()) {
            reply = r.content;
            break;
        }
        // append the assistant's tool-calling message, then execute each tool
        msgs.push_back(msgAssistantTools(r.content, r.tool_calls));
        for (auto& tc : r.tool_calls) {
            std::string result = dispatchTool(tc.name, tc.arguments, userId);
            msgs.push_back(msgTool(tc.id, result));
        }
    }

    if (reply.empty()) {
        // Log the real reason server-side; return empty so the caller shows a
        // clean fallback instead of storing a raw HTTP error as an assistant turn.
        if (!lastError.empty()) std::cerr << "[CS] agent error: " << lastError << "\n";
    }
    return reply;
}

std::string CustomerServiceService::dispatchTool(const std::string& name, const std::string& argsJson, int userId) {
    auto args = crow::json::load(argsJson);

    if (name == "get_my_balance") return toolGetBalance(userId);
    if (name == "get_my_vehicles") return toolGetVehicles(userId);
    if (name == "get_my_parking_status") {
        std::string plate = (args && args.has("plate")) ? std::string(args["plate"].s()) : "";
        return toolGetParkingStatus(userId, plate);
    }
    if (name == "estimate_parking_fee") {
        std::string rule = (args && args.has("rule")) ? std::string(args["rule"].s()) : "standard";
        int minutes = (args && args.has("minutes")) ? args["minutes"].i() : 60;
        return toolEstimateFee(rule, minutes);
    }
    if (name == "get_my_monthly_pass") return toolGetMonthlyPass(userId);
    if (name == "check_blacklist") {
        std::string plate = (args && args.has("plate")) ? std::string(args["plate"].s()) : "";
        return toolCheckBlacklist(plate);
    }
    if (name == "get_pricing") return toolGetPricing();
    if (name == "get_occupancy") return toolGetOccupancy();
    return "{\"error\":\"unknown tool\"}";
}

// ---------- read-only tools ----------
std::string CustomerServiceService::toolGetBalance(int userId) {
    auto conn = getConnection();
    crow::json::wvalue j;
    j["user_id"] = userId;
    if (!conn) { j["error"] = "db"; return j.dump(); }
    std::string v = scalar(conn->get(), "SELECT IFNULL(balance,0) FROM USER WHERE id=" + std::to_string(userId));
    j["balance"] = v.empty() ? 0.0 : std::stod(v);
    return j.dump();
}

std::string CustomerServiceService::toolGetVehicles(int userId) {
    auto conn = getConnection();
    crow::json::wvalue j;
    std::vector<crow::json::wvalue> plates;
    if (conn) {
        std::string sql = "SELECT license_plate FROM USER_PLATE WHERE user_id=" + std::to_string(userId);
        if (mysql_query(conn->get(), sql.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(conn->get());
            if (res) {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res))) {
                    crow::json::wvalue p;
                    p = row[0] ? row[0] : "";
                    plates.push_back(std::move(p));
                }
                mysql_free_result(res);
            }
        }
    }
    j["vehicles"] = std::move(plates);
    j["count"] = (int)plates.size();
    return j.dump();
}

std::string CustomerServiceService::toolGetParkingStatus(int userId, const std::string& plate) {
    auto conn = getConnection();
    crow::json::wvalue j;
    std::vector<crow::json::wvalue> list;
    if (conn) {
        MYSQL* mysql = conn->get();
        std::string sql;
        if (!plate.empty()) {
            sql = "SELECT license_plate,check_in_time,TIMESTAMPDIFF(MINUTE,check_in_time,NOW()) "
                  "FROM CAR_RECORD WHERE license_plate='" + escape(mysql, plate) + "' AND check_out_time IS NULL "
                  "ORDER BY id DESC LIMIT 5";
        } else {
            sql = "SELECT license_plate,check_in_time,TIMESTAMPDIFF(MINUTE,check_in_time,NOW()) "
                  "FROM CAR_RECORD WHERE check_out_time IS NULL AND license_plate IN "
                  "(SELECT license_plate FROM USER_PLATE WHERE user_id=" + std::to_string(userId) + ") "
                  "ORDER BY id DESC LIMIT 5";
        }
        if (mysql_query(mysql, sql.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(mysql);
            if (res) {
                BillingRuleLite rule = loadRule(mysql, "standard");
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res))) {
                    crow::json::wvalue c;
                    c["plate"] = row[0] ? row[0] : "";
                    c["check_in_time"] = row[1] ? row[1] : "";
                    int mins = row[2] ? std::stoi(row[2]) : 0;
                    c["minutes"] = mins;
                    c["estimated_fee"] = computeFee(rule, mins);
                    list.push_back(std::move(c));
                }
                mysql_free_result(res);
            }
        }
    }
    int n = (int)list.size();
    j["in_parking"] = std::move(list);
    j["count"] = n;
    return j.dump();
}

std::string CustomerServiceService::toolEstimateFee(const std::string& ruleType, int minutes) {
    auto conn = getConnection();
    crow::json::wvalue j;
    j["rule"] = ruleType.empty() ? "standard" : ruleType;
    j["minutes"] = minutes;
    if (!conn) { j["error"] = "db"; return j.dump(); }
    BillingRuleLite r = loadRule(conn->get(), ruleType);
    if (!r.valid) { j["error"] = "rule not found"; return j.dump(); }
    j["estimated_fee"] = computeFee(r, minutes);
    j["free_minutes"] = r.free_minutes;
    j["hourly_rate"] = r.hourly_rate;
    j["max_daily_fee"] = r.max_daily_fee;
    return j.dump();
}

std::string CustomerServiceService::toolGetMonthlyPass(int userId) {
    auto conn = getConnection();
    crow::json::wvalue j;
    std::vector<crow::json::wvalue> passes;
    if (conn) {
        std::string sql = "SELECT license_plate,pass_type,start_date,end_date,fee FROM MONTHLY_PASS "
                          "WHERE user_id=" + std::to_string(userId) +
                          " AND is_active=1 AND end_date>=CURDATE() ORDER BY end_date DESC LIMIT 10";
        if (mysql_query(conn->get(), sql.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(conn->get());
            if (res) {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res))) {
                    crow::json::wvalue p;
                    p["plate"] = row[0] ? row[0] : "";
                    p["type"] = row[1] ? row[1] : "";
                    p["start_date"] = row[2] ? row[2] : "";
                    p["end_date"] = row[3] ? row[3] : "";
                    p["fee"] = row[4] ? std::stod(row[4]) : 0.0;
                    passes.push_back(std::move(p));
                }
                mysql_free_result(res);
            }
        }
    }
    j["passes"] = std::move(passes);
    j["count"] = (int)passes.size();
    return j.dump();
}

std::string CustomerServiceService::toolCheckBlacklist(const std::string& plate) {
    auto conn = getConnection();
    crow::json::wvalue j;
    j["plate"] = plate;
    if (!conn) { j["blacklisted"] = false; return j.dump(); }
    MYSQL* mysql = conn->get();
    std::string sql = "SELECT reason FROM VEHICLE_BLACKLIST WHERE license_plate='" + escape(mysql, plate) + "' LIMIT 1";
    std::string reason;
    if (mysql_query(mysql, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(mysql);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row) reason = row[0] ? row[0] : "";
            mysql_free_result(res);
        }
    }
    j["blacklisted"] = !reason.empty() || scalar(mysql,
        "SELECT COUNT(*) FROM VEHICLE_BLACKLIST WHERE license_plate='" + escape(mysql, plate) + "'") != "0";
    j["reason"] = reason;
    return j.dump();
}

std::string CustomerServiceService::toolGetPricing() {
    auto conn = getConnection();
    crow::json::wvalue j;
    std::vector<crow::json::wvalue> rules;
    std::vector<crow::json::wvalue> plans;
    if (conn) {
        MYSQL* mysql = conn->get();
        if (mysql_query(mysql, "SELECT rule_name,rule_type,free_minutes,hourly_rate,IFNULL(max_daily_fee,0),description FROM BILLING_RULE WHERE is_active=1 ORDER BY id") == 0) {
            MYSQL_RES* res = mysql_store_result(mysql);
            if (res) {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res))) {
                    crow::json::wvalue r;
                    r["name"] = row[0] ? row[0] : "";
                    r["type"] = row[1] ? row[1] : "";
                    r["free_minutes"] = row[2] ? std::stoi(row[2]) : 0;
                    r["hourly_rate"] = row[3] ? std::stod(row[3]) : 0.0;
                    r["max_daily_fee"] = row[4] ? std::stod(row[4]) : 0.0;
                    r["description"] = row[5] ? row[5] : "";
                    rules.push_back(std::move(r));
                }
                mysql_free_result(res);
            }
        }
        if (mysql_query(mysql, "SELECT plan_name,duration_days,price,description FROM PASS_PLAN WHERE is_active=1 ORDER BY price") == 0) {
            MYSQL_RES* res = mysql_store_result(mysql);
            if (res) {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res))) {
                    crow::json::wvalue p;
                    p["name"] = row[0] ? row[0] : "";
                    p["days"] = row[1] ? std::stoi(row[1]) : 0;
                    p["price"] = row[2] ? std::stod(row[2]) : 0.0;
                    p["description"] = row[3] ? row[3] : "";
                    plans.push_back(std::move(p));
                }
                mysql_free_result(res);
            }
        }
    }
    j["billing_rules"] = std::move(rules);
    j["pass_plans"] = std::move(plans);
    return j.dump();
}

std::string CustomerServiceService::toolGetOccupancy() {
    auto conn = getConnection();
    crow::json::wvalue j;
    std::vector<crow::json::wvalue> lots;
    if (conn) {
        if (mysql_query(conn->get(), "SELECT P_name,P_total_count,P_current_count,P_reserve_count FROM PARKING_LOT ORDER BY P_id") == 0) {
            MYSQL_RES* res = mysql_store_result(conn->get());
            if (res) {
                MYSQL_ROW row;
                while ((row = mysql_fetch_row(res))) {
                    int total = row[1] ? std::stoi(row[1]) : 0;
                    int cur = row[2] ? std::stoi(row[2]) : 0;
                    int resv = row[3] ? std::stoi(row[3]) : 0;
                    crow::json::wvalue l;
                    l["name"] = row[0] ? row[0] : "";
                    l["total"] = total;
                    l["occupied"] = cur;
                    l["reserved"] = resv;
                    l["free"] = total - cur - resv;
                    lots.push_back(std::move(l));
                }
                mysql_free_result(res);
            }
        }
    }
    j["lots"] = std::move(lots);
    return j.dump();
}

// ---------- admin-facing ----------
crow::json::wvalue CustomerServiceService::adminListSessions(const std::string& statusFilter) {
    crow::json::wvalue result;
    std::vector<crow::json::wvalue> rows;
    auto conn = getConnection();
    if (!conn) { result["sessions"] = std::move(rows); return result; }
    MYSQL* mysql = conn->get();

    std::string where = "WHERE s.status<>'closed' ";
    if (statusFilter == "escalated") where = "WHERE s.status='escalated' ";
    else if (statusFilter == "active") where = "WHERE s.status<>'closed' ";

    std::string sql =
        "SELECT s.id,s.user_id,u.username,u.truename,u.telephone,u.role,IFNULL(u.balance,0),"
        "s.status,s.ai_turn_count,IFNULL(s.handled_by,0),IFNULL(s.title,''),"
        "s.created_at,s.last_message_at,IFNULL(s.escalated_at,'') "
        "FROM CS_SESSION s JOIN USER u ON u.id=s.user_id " + where +
        "ORDER BY (s.status='escalated') DESC, s.last_message_at DESC LIMIT 200";

    if (mysql_query(mysql, sql.c_str()) != 0) { result["sessions"] = std::move(rows); return result; }
    MYSQL_RES* res = mysql_store_result(mysql);
    if (!res) { result["sessions"] = std::move(rows); return result; }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        int sid = row[0] ? std::stoi(row[0]) : 0;
        crow::json::wvalue c;
        c["id"] = sid;
        c["user_id"] = row[1] ? std::stoi(row[1]) : 0;
        c["username"] = row[2] ? row[2] : "";
        c["truename"] = row[3] ? row[3] : "";
        c["telephone"] = row[4] ? row[4] : "";
        c["role"] = row[5] ? row[5] : "";
        c["balance"] = row[6] ? std::stod(row[6]) : 0.0;
        c["status"] = row[7] ? row[7] : "";
        c["ai_turn_count"] = row[8] ? std::stoi(row[8]) : 0;
        c["handled_by"] = row[9] ? std::stoi(row[9]) : 0;
        c["title"] = row[10] ? row[10] : "";
        c["created_at"] = row[11] ? row[11] : "";
        c["last_message_at"] = row[12] ? row[12] : "";
        c["escalated_at"] = row[13] ? row[13] : "";

        std::string lastMsg = scalar(mysql, "SELECT content FROM CS_MESSAGE WHERE session_id=" + std::to_string(sid) + " ORDER BY id DESC LIMIT 1");
        std::string lastTime = scalar(mysql, "SELECT created_at FROM CS_MESSAGE WHERE session_id=" + std::to_string(sid) + " ORDER BY id DESC LIMIT 1");
        std::string unread = scalar(mysql, "SELECT COUNT(*) FROM CS_MESSAGE WHERE session_id=" + std::to_string(sid) + " AND sender_type='user' AND is_read_by_admin=0");
        c["last_message"] = lastMsg;
        c["last_time"] = lastTime;
        c["unread"] = unread.empty() ? 0 : std::stoi(unread);
        rows.push_back(std::move(c));
    }
    mysql_free_result(res);

    result["sessions"] = std::move(rows);
    return result;
}

crow::json::wvalue CustomerServiceService::adminGetSession(int sessionId) {
    crow::json::wvalue result;
    auto conn = getConnection();
    result["found"] = false;
    if (!conn) return result;
    MYSQL* mysql = conn->get();

    std::string sql =
        "SELECT s.id,s.user_id,u.username,u.truename,u.telephone,u.role,IFNULL(u.balance,0),"
        "s.status,s.ai_turn_count,IFNULL(s.handled_by,0),IFNULL(s.title,''),"
        "s.created_at,s.last_message_at,IFNULL(s.escalated_at,'') "
        "FROM CS_SESSION s JOIN USER u ON u.id=s.user_id WHERE s.id=" + std::to_string(sessionId);
    if (mysql_query(mysql, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(mysql);
        if (res) {
            if (MYSQL_ROW row = mysql_fetch_row(res)) {
                result["found"] = true;
                result["id"] = row[0] ? std::stoi(row[0]) : 0;
                result["user_id"] = row[1] ? std::stoi(row[1]) : 0;
                result["username"] = row[2] ? row[2] : "";
                result["truename"] = row[3] ? row[3] : "";
                result["telephone"] = row[4] ? row[4] : "";
                result["role"] = row[5] ? row[5] : "";
                result["balance"] = row[6] ? std::stod(row[6]) : 0.0;
                result["status"] = row[7] ? row[7] : "";
                result["ai_turn_count"] = row[8] ? std::stoi(row[8]) : 0;
                result["handled_by"] = row[9] ? std::stoi(row[9]) : 0;
                result["title"] = row[10] ? row[10] : "";
                result["created_at"] = row[11] ? row[11] : "";
                result["last_message_at"] = row[12] ? row[12] : "";
                result["escalated_at"] = row[13] ? row[13] : "";
            }
            mysql_free_result(res);
        }
    }

    auto msgs = list("SELECT id,session_id,user_id,sender_type,sender_id,content,created_at,is_read_by_admin,is_read_by_user "
                     "FROM CS_MESSAGE WHERE session_id=" + std::to_string(sessionId) + " ORDER BY id ASC LIMIT 500");
    std::vector<crow::json::wvalue> arr;
    for (auto& m : msgs) arr.push_back(m.serialize());
    result["messages"] = std::move(arr);

    adminMarkRead(sessionId);
    return result;
}

bool CustomerServiceService::adminReply(int adminId, int sessionId, const std::string& content, std::string& error) {
    int userId = 0;
    {
        auto conn = getConnection();
        if (!conn) { error = "数据库连接失败"; return false; }
        std::string v = scalar(conn->get(), "SELECT user_id FROM CS_SESSION WHERE id=" + std::to_string(sessionId));
        if (v.empty()) { error = "会话不存在"; return false; }
        userId = std::stoi(v);
    }
    appendMessage(sessionId, userId, "admin", adminId, content, false);
    setSessionStatus(sessionId, "handled", adminId);
    return true;
}

bool CustomerServiceService::adminClose(int sessionId, std::string& error) {
    if (!setSessionStatus(sessionId, "closed", 0)) { error = "关闭失败"; return false; }
    return true;
}

int CustomerServiceService::adminPendingCount() {
    auto conn = getConnection();
    if (!conn) return 0;
    std::string v = scalar(conn->get(), "SELECT COUNT(*) FROM CS_SESSION WHERE status='escalated'");
    return v.empty() ? 0 : std::stoi(v);
}

int CustomerServiceService::adminUnreadCount() {
    auto conn = getConnection();
    if (!conn) return 0;
    std::string v = scalar(conn->get(),
        "SELECT COUNT(*) FROM CS_MESSAGE m JOIN CS_SESSION s ON s.id=m.session_id "
        "WHERE m.sender_type='user' AND m.is_read_by_admin=0 AND s.status<>'closed'");
    return v.empty() ? 0 : std::stoi(v);
}

bool CustomerServiceService::adminMarkRead(int sessionId) {
    auto conn = getConnection();
    if (!conn) return false;
    return executeQuery(conn->get(),
        "UPDATE CS_MESSAGE SET is_read_by_admin=1 WHERE session_id=" + std::to_string(sessionId) +
        " AND sender_type='user'");
}
