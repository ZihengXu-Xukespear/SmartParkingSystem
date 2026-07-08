// =============================================================================
//  SmartParkingSystem - json_helpers.cpp
// =============================================================================
#include "json_helpers.h"
#include "string_utils.h"

#include <sstream>
#include <stdexcept>

namespace sps::util {

crow::json::wvalue ok_response(crow::json::wvalue payload) {
    crow::json::wvalue res;
    res["ok"] = true;
    res["data"] = std::move(payload);
    return res;
}

crow::json::wvalue error_response(std::string code,
                                   std::string message,
                                   crow::json::wvalue details) {
    crow::json::wvalue res;
    res["ok"] = false;
    res["code"] = std::move(code);
    res["message"] = std::move(message);
    if (details.dump() != "null") {
        res["details"] = std::move(details);
    }
    return res;
}

crow::json::wvalue pagination(int page, int page_size, std::int64_t total) {
    crow::json::wvalue res;
    res["page"] = page;
    res["page_size"] = page_size;
    res["total"] = total;
    if (page_size > 0) {
        std::int64_t pages = (total + page_size - 1) / page_size;
        res["total_pages"] = pages;
    }
    return res;
}

std::string csv_escape(std::string_view field) {
    bool need_quote = field.find_first_of(",\"\n\r") != std::string_view::npos;
    std::string out = json_escape(field);
    if (need_quote) {
        out.insert(out.begin(), '"');
        out.push_back('"');
    }
    return out;
}

std::string csv_row(const std::vector<std::string>& fields) {
    std::string out;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i) out.push_back(',');
        out += csv_escape(fields[i]);
    }
    out.push_back('\n');
    return out;
}

crow::json::wvalue rvalue_to_wvalue(const crow::json::rvalue& v) {
    crow::json::wvalue out;
    switch (v.t()) {
        case crow::json::type::Null:
            out = nullptr;
            break;
        case crow::json::type::False:
            out = false;
            break;
        case crow::json::type::True:
            out = true;
            break;
        case crow::json::type::Number: {
            std::string num(v.s());
            if (num.find('.') != std::string::npos ||
                num.find('e') != std::string::npos ||
                num.find('E') != std::string::npos) {
                out = v.d();
            } else {
                try { out = std::stoll(num); } catch (...) { out = v.d(); }
            }
            break;
        }
        case crow::json::type::String:
            out = v.s();
            break;
        case crow::json::type::List: {
            std::vector<crow::json::wvalue> arr;
            for (std::size_t i = 0; i < v.size(); ++i) {
                arr.push_back(rvalue_to_wvalue(v[i]));
            }
            out = std::move(arr);
            break;
        }
        case crow::json::type::Object: {
            for (const auto& key : v.keys()) {
                out[key] = rvalue_to_wvalue(v[key]);
            }
            break;
        }
    }
    return out;
}

std::string first_missing_key(const crow::json::rvalue& obj,
                              const std::vector<std::string>& required_keys) {
    if (obj.t() != crow::json::type::Object) return "$";
    for (const auto& k : required_keys) {
        if (!obj.has(k)) return k;
    }
    return std::string();
}

namespace {

void pretty_print_indent(std::ostringstream& os, const std::string& s, int indent) {
    bool in_string = false;
    bool escape = false;
    int depth = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (escape) { os << c; escape = false; continue; }
        if (c == '\\' && in_string) { os << c; escape = true; continue; }
        if (c == '"') { in_string = !in_string; os << c; continue; }
        if (in_string) { os << c; continue; }
        if (c == '{' || c == '[') {
            os << c << '\n';
            depth++;
            for (int j = 0; j < depth * indent; ++j) os << ' ';
            continue;
        }
        if (c == '}' || c == ']') {
            os << '\n';
            depth--;
            for (int j = 0; j < depth * indent; ++j) os << ' ';
            os << c;
            continue;
        }
        if (c == ',') { os << c << '\n';
            for (int j = 0; j < depth * indent; ++j) os << ' ';
            continue;
        }
        if (c == ':') { os << ": "; continue; }
        os << c;
    }
}

}  // namespace

std::string pretty_print(const crow::json::wvalue& v, int indent) {
    std::string raw = v.dump();
    std::ostringstream os;
    pretty_print_indent(os, raw, indent);
    return os.str();
}

}  // namespace sps::util