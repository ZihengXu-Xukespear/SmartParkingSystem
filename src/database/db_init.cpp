#include "db_init.h"
#include "../config.h"

bool DBInit::createDatabase(const AppConfig& cfg) {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) return false;
    mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");
    mysql_options(conn, MYSQL_INIT_COMMAND, "SET NAMES utf8mb4");
    if (!mysql_real_connect(conn, cfg.host.c_str(), cfg.user.c_str(),
        cfg.password.c_str(), nullptr, cfg.port, nullptr, 0)) {
        mysql_close(conn);
        return false;
    }
    // Drop existing database to fully reset
    std::string dropSql = "DROP DATABASE IF EXISTS `" + cfg.database + "`";
    mysql_query(conn, dropSql.c_str());

    std::string sql = "CREATE DATABASE `" + cfg.database +
        "` DEFAULT CHARACTER SET utf8mb4 DEFAULT COLLATE utf8mb4_unicode_ci";
    bool ok = mysql_query(conn, sql.c_str()) == 0;
    mysql_close(conn);
    return ok;
}

bool DBInit::createTables(const AppConfig& cfg) {
    auto conn = MySQLPool::instance().getConnection();
    if (!conn) return false;
    MYSQL* mysql = conn->get();

    const char* tables[] = {
        "CREATE TABLE IF NOT EXISTS USER ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  username VARCHAR(255) UNIQUE NOT NULL,"
        "  password VARCHAR(64) NOT NULL,"
        "  telephone VARCHAR(11) NOT NULL,"
        "  truename VARCHAR(255) NOT NULL,"
        "  role VARCHAR(20) DEFAULT 'user',"
        "  balance DECIMAL(10,2) DEFAULT 0.00,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS PARKING_LOT ("
        "  P_id INT PRIMARY KEY AUTO_INCREMENT,"
        "  P_name VARCHAR(255) UNIQUE NOT NULL,"
        "  P_total_count INT NOT NULL DEFAULT 0,"
        "  P_current_count INT NOT NULL DEFAULT 0,"
        "  P_reserve_count INT NOT NULL DEFAULT 0,"
        "  P_fee DECIMAL(10,2) NOT NULL DEFAULT 5.00"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS CAR_RECORD ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  license_plate VARCHAR(20) NOT NULL,"
        "  check_in_time DATETIME NOT NULL,"
        "  check_out_time DATETIME DEFAULT NULL,"
        "  fee DECIMAL(10,2) DEFAULT NULL,"
        "  location VARCHAR(255) NOT NULL,"
        "  billing_type VARCHAR(20) DEFAULT 'standard',"
        "  exit_deadline DATETIME DEFAULT NULL,"
        "  reservation_id INT DEFAULT NULL,"
        "  spot_number INT DEFAULT 0,"
        "  charging_plan VARCHAR(20) DEFAULT '',"
        "  charging_fee DECIMAL(10,2) DEFAULT 0.00,"
        "  INDEX idx_plate (license_plate),"
        "  INDEX idx_checkin (check_in_time)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS RESERVATION ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  license_plate VARCHAR(20) NOT NULL,"
        "  P_name VARCHAR(255) NOT NULL,"
        "  prepaid DECIMAL(10,2) DEFAULT 0.00,"
        "  status VARCHAR(20) DEFAULT 'active',"
        "  spot_number INT DEFAULT 0,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  INDEX idx_plate (license_plate)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS BILLING_RULE ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  rule_name VARCHAR(100) NOT NULL,"
        "  rule_type VARCHAR(20) NOT NULL,"
        "  free_minutes INT DEFAULT 30,"
        "  hourly_rate DECIMAL(10,2) DEFAULT 5.00,"
        "  max_daily_fee DECIMAL(10,2) DEFAULT NULL,"
        "  tier_config TEXT DEFAULT NULL,"
        "  description TEXT DEFAULT NULL,"
        "  P_name VARCHAR(255) DEFAULT '',"
        "  is_active TINYINT DEFAULT 1,"
        "  UNIQUE KEY idx_rule_type (rule_type, P_name)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS PASS_PLAN ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  plan_name VARCHAR(100) NOT NULL,"
        "  duration_days INT NOT NULL DEFAULT 30,"
        "  price DECIMAL(10,2) NOT NULL DEFAULT 300.00,"
        "  description TEXT DEFAULT NULL,"
        "  P_name VARCHAR(255) DEFAULT '',"
        "  is_active TINYINT DEFAULT 1,"
        "  UNIQUE KEY idx_plan_name (plan_name, P_name)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS BALANCE_RECORD ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  user_id INT NOT NULL,"
        "  amount DECIMAL(10,2) NOT NULL,"
        "  type VARCHAR(20) NOT NULL,"
        "  description VARCHAR(255) DEFAULT '',"
        "  balance_after DECIMAL(10,2) NOT NULL,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  INDEX idx_user (user_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS VEHICLE_BLACKLIST ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  license_plate VARCHAR(20) NOT NULL UNIQUE,"
        "  reason VARCHAR(255) DEFAULT '',"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS INTERCEPTION_LOG ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  license_plate VARCHAR(20) NOT NULL,"
        "  reason VARCHAR(255) DEFAULT '',"
        "  intercepted_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  INDEX idx_intercepted_at (intercepted_at)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS BULLETIN ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  content TEXT NOT NULL,"
        "  is_pinned TINYINT DEFAULT 0,"
        "  valid_from DATETIME DEFAULT NULL,"
        "  valid_until DATETIME DEFAULT NULL,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS MONTHLY_PASS ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  license_plate VARCHAR(20) NOT NULL,"
        "  pass_type VARCHAR(50) NOT NULL,"
        "  start_date DATE NOT NULL,"
        "  end_date DATE NOT NULL,"
        "  fee DECIMAL(10,2) NOT NULL,"
        "  is_active TINYINT DEFAULT 1,"
        "  user_id INT DEFAULT 0,"
        "  plan_id INT DEFAULT 0,"
        "  P_name VARCHAR(255) DEFAULT '',"
        "  INDEX idx_plate (license_plate)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS MESSAGE ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  sender_id INT NOT NULL,"
        "  receiver_id INT NOT NULL DEFAULT 0,"
        "  content TEXT NOT NULL,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  is_read TINYINT DEFAULT 0,"
        "  INDEX idx_sender (sender_id),"
        "  INDEX idx_receiver (receiver_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        // AI + human customer-service sessions
        "CREATE TABLE IF NOT EXISTS CS_SESSION ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  user_id INT NOT NULL,"
        "  status VARCHAR(20) DEFAULT 'ai',"
        "  ai_turn_count INT DEFAULT 0,"
        "  handled_by INT DEFAULT NULL,"
        "  title VARCHAR(255) DEFAULT '',"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,"
        "  last_message_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  escalated_at DATETIME DEFAULT NULL,"
        "  INDEX idx_cs_user (user_id),"
        "  INDEX idx_cs_last (last_message_at),"
        "  INDEX idx_cs_status (status)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4",

        "CREATE TABLE IF NOT EXISTS CS_MESSAGE ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  session_id INT NOT NULL,"
        "  user_id INT NOT NULL,"
        "  sender_type VARCHAR(20) NOT NULL,"
        "  sender_id INT DEFAULT 0,"
        "  content TEXT NOT NULL,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  is_read_by_admin TINYINT DEFAULT 0,"
        "  is_read_by_user TINYINT DEFAULT 0,"
        "  INDEX idx_cs_session (session_id),"
        "  INDEX idx_cs_msg_user (user_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4"
    };

    for (const char* sql : tables) {
        if (mysql_query(mysql, sql) != 0) return false;
    }

    mysql_query(mysql, "SELECT COUNT(*) FROM BILLING_RULE");
    MYSQL_RES* brRes = mysql_store_result(mysql);
    bool hasBillingRules = false;
    if (brRes) {
        MYSQL_ROW brRow = mysql_fetch_row(brRes);
        if (brRow && std::stoi(brRow[0]) > 0) hasBillingRules = true;
        mysql_free_result(brRes);
    }

    // Ensure parking lots exist BEFORE seeding billing rules and pass plans
    std::string parking_sql = "INSERT IGNORE INTO PARKING_LOT (P_name,P_total_count,P_current_count,P_reserve_count,P_fee) VALUES ('" +
        cfg.parking_name + "'," + std::to_string(cfg.capacity) + ",0,0," + std::to_string(cfg.fee) + ")";
    mysql_query(mysql, parking_sql.c_str());
    mysql_query(mysql,
        "INSERT IGNORE INTO PARKING_LOT (P_name,P_total_count,P_current_count,P_reserve_count,P_fee) VALUES "
        "('停车场2',20,0,0,5.00)");

    // Clear old duplicated billing rules and re-seed with unified global rules
    {
        // Delete all existing rules and re-seed clean
        mysql_query(mysql, "DELETE FROM BILLING_RULE");
        mysql_query(mysql,
            "INSERT INTO BILLING_RULE (rule_name,rule_type,free_minutes,hourly_rate,max_daily_fee,tier_config,description,is_active,P_name) VALUES "
            "('普通计费','standard',30,5.00,50.00,'','30分钟内免费，之后每小时5元，每日封顶50元',1,''),"
            "('阶梯计费','tiered',30,5.00,60.00,'[{\\\"hours\\\":2,\\\"rate\\\":5.00},{\\\"hours\\\":4,\\\"rate\\\":3.00},{\\\"hours\\\":99,\\\"rate\\\":2.00}]','前2小时每小时5元，2-4小时每小时3元，4小时以上每小时2元',1,''),"
            "('会员计费','member',60,3.00,30.00,'','会员享受60分钟免费，之后每小时3元，每日封顶30元',1,''),"
            "('电动汽车充电计费','electric_charging',0,5.00,0.00,'','电动汽车充电每小时额外收费 5 元，按已完成小时数计算（最少 1 小时）',1,'')");
    }

    // Clear old duplicated pass plans and re-seed with unified global plans
    {
        mysql_query(mysql, "DELETE FROM PASS_PLAN");
        mysql_query(mysql,
            "INSERT INTO PASS_PLAN (plan_name,duration_days,price,description,is_active,P_name) VALUES "
            "('月卡',30,300.00,'30天畅停',1,''),"
            "('季卡',90,800.00,'90天优惠',1,''),"
            "('年卡',365,2880.00,'全年无忧',1,'')");
    }

    mysql_query(mysql, "DROP TRIGGER IF EXISTS after_reservation_insert");
    mysql_query(mysql,
        "CREATE TRIGGER after_reservation_insert "
        "AFTER INSERT ON RESERVATION FOR EACH ROW "
        "BEGIN UPDATE PARKING_LOT SET P_reserve_count = P_reserve_count + 1 WHERE P_name = NEW.P_name; END"
    );

    mysql_query(mysql, "DROP TRIGGER IF EXISTS after_reservation_delete");
    mysql_query(mysql,
        "CREATE TRIGGER after_reservation_delete "
        "AFTER DELETE ON RESERVATION FOR EACH ROW "
        "BEGIN UPDATE PARKING_LOT SET P_reserve_count = GREATEST(P_reserve_count - 1, 0) WHERE P_name = OLD.P_name; END"
    );

    mysql_query(mysql, "DROP TRIGGER IF EXISTS after_reservation_status_change");
    mysql_query(mysql,
        "CREATE TRIGGER after_reservation_status_change "
        "AFTER UPDATE ON RESERVATION FOR EACH ROW "
        "BEGIN "
        "  IF OLD.status = 'active' AND NEW.status != 'active' THEN "
        "    UPDATE PARKING_LOT SET P_reserve_count = GREATEST(P_reserve_count - 1, 0) WHERE P_name = NEW.P_name; "
        "  END IF; "
        "END"
    );

    mysql_query(mysql, "SELECT COUNT(*) FROM USER WHERE role='admin'");
    MYSQL_RES* adminRes = mysql_store_result(mysql);
    bool hasAdmin = false;
    if (adminRes) {
        MYSQL_ROW adminRow = mysql_fetch_row(adminRes);
        if (adminRow && std::stoi(adminRow[0]) > 0) hasAdmin = true;
        mysql_free_result(adminRes);
    }
    if (!hasAdmin) {
        std::string hashed = sha256::hash("admin123");
        std::string adminSql = "INSERT INTO USER (username,password,telephone,truename,role) VALUES "
            "('admin','" + hashed + "','00000000000','系统管理员','admin')";
        mysql_query(mysql, adminSql.c_str());
    }

    // Migrate old notice to BULLETIN table if empty
    mysql_query(mysql, "SELECT COUNT(*) FROM BULLETIN");
    MYSQL_RES* blres = mysql_store_result(mysql);
    bool hasBulletin = false;
    if (blres) {
        MYSQL_ROW blrow = mysql_fetch_row(blres);
        if (blrow && std::stoi(blrow[0]) > 0) hasBulletin = true;
        mysql_free_result(blres);
    }
    if (!hasBulletin && !AppConfig::instance().notice.empty()) {
        std::string oldNotice = AppConfig::instance().notice;
        char* buf = new char[oldNotice.size() * 2 + 1];
        mysql_real_escape_string(mysql, buf, oldNotice.c_str(), (unsigned long)oldNotice.size());
        std::string escaped(buf);
        delete[] buf;
        std::string noticeSql = "INSERT INTO BULLETIN (content,is_pinned) VALUES ('" + escaped + "',1)";
        mysql_query(mysql, noticeSql.c_str());
    }

    // Migrations: add columns missing from older schemas (ignore errors if already exist)
    mysql_query(mysql, "ALTER TABLE USER ADD COLUMN balance DECIMAL(10,2) DEFAULT 0.00");
    mysql_query(mysql, "ALTER TABLE CAR_RECORD ADD COLUMN exit_deadline DATETIME DEFAULT NULL");
    mysql_query(mysql, "ALTER TABLE CAR_RECORD ADD COLUMN reservation_id INT DEFAULT NULL");
    mysql_query(mysql, "ALTER TABLE CAR_RECORD ADD COLUMN spot_number INT DEFAULT 0");
    mysql_query(mysql, "ALTER TABLE CAR_RECORD ADD COLUMN P_name VARCHAR(255) DEFAULT '停车场1'");
    mysql_query(mysql, "ALTER TABLE RESERVATION ADD COLUMN prepaid DECIMAL(10,2) DEFAULT 0.00");
    mysql_query(mysql, "ALTER TABLE RESERVATION ADD COLUMN status VARCHAR(20) DEFAULT 'active'");
    mysql_query(mysql, "ALTER TABLE RESERVATION ADD COLUMN spot_number INT DEFAULT 0");
    // RESERVATION.user_id is required by VehicleService::queryRecords / getParkedVehicles
    // to filter records for regular users. Without it the entire OR clause in
    // the SQL fails (Unknown column 'user_id') and crud_service::list() silently
    // returns empty results → 用户查询自己刚入库的车返回 0 条 (bug).
    mysql_query(mysql, "ALTER TABLE RESERVATION ADD COLUMN user_id INT DEFAULT 0");
    mysql_query(mysql, "ALTER TABLE RESERVATION ADD INDEX idx_reservation_user (user_id)");
    mysql_query(mysql, "ALTER TABLE BILLING_RULE ADD COLUMN P_name VARCHAR(255) DEFAULT ''");
    mysql_query(mysql, "ALTER TABLE PASS_PLAN ADD COLUMN P_name VARCHAR(255) DEFAULT ''");
    mysql_query(mysql, "ALTER TABLE MONTHLY_PASS ADD COLUMN P_name VARCHAR(255) DEFAULT ''");
    // MONTHLY_PASS was created before user_id/plan_id were added to the schema;
    // CREATE TABLE IF NOT EXISTS won't add them to an existing table, so without
    // these ALTERs the INSERT in PassPlanService::purchase fails with
    // "Unknown column 'user_id'" (=> 月卡购买失败退款). Fire-and-forget like the rest.
    mysql_query(mysql, "ALTER TABLE MONTHLY_PASS ADD COLUMN user_id INT DEFAULT 0");
    mysql_query(mysql, "ALTER TABLE MONTHLY_PASS ADD COLUMN plan_id INT DEFAULT 0");
    // Backfill NULL P_name values on existing rows
    mysql_query(mysql, "UPDATE BILLING_RULE SET P_name='' WHERE P_name IS NULL");
    mysql_query(mysql, "UPDATE PASS_PLAN SET P_name='' WHERE P_name IS NULL");

    // Deduplicate BILLING_RULE: keep only the row with the lowest id per (rule_type, P_name)
    mysql_query(mysql,
        "DELETE br1 FROM BILLING_RULE br1 "
        "INNER JOIN BILLING_RULE br2 "
        "ON br1.rule_type = br2.rule_type AND br1.P_name = br2.P_name AND br1.id > br2.id");

    // Deduplicate PASS_PLAN: keep only the row with the lowest id per (plan_name, P_name)
    mysql_query(mysql,
        "DELETE pp1 FROM PASS_PLAN pp1 "
        "INNER JOIN PASS_PLAN pp2 "
        "ON pp1.plan_name = pp2.plan_name AND pp1.P_name = pp2.P_name AND pp1.id > pp2.id");

    // Add UNIQUE constraints to prevent duplicate inserts on restart (ignore if already exists)
    mysql_query(mysql, "ALTER TABLE BILLING_RULE ADD UNIQUE INDEX idx_rule_type (rule_type, P_name)");
    mysql_query(mysql, "ALTER TABLE PASS_PLAN ADD UNIQUE INDEX idx_plan_name (plan_name, P_name)");

    // Remove old parking lot name
    mysql_query(mysql, "DELETE FROM PARKING_LOT WHERE P_name='智慧停车场'");
    mysql_query(mysql, "DELETE FROM PARKING_LOT WHERE P_name='SmartParking'");

    // Fix old UNIQUE constraint on license_plate -> regular index
    mysql_query(mysql, "ALTER TABLE RESERVATION DROP INDEX idx_plate");
    mysql_query(mysql, "ALTER TABLE RESERVATION ADD INDEX idx_plate (license_plate)");

    // Add operator_id to CAR_RECORD for tracking who performed check-in
    mysql_query(mysql, "ALTER TABLE CAR_RECORD ADD COLUMN operator_id INT DEFAULT 0");
    mysql_query(mysql, "ALTER TABLE CAR_RECORD ADD INDEX idx_operator (operator_id)");
    mysql_query(mysql, "ALTER TABLE CAR_RECORD ADD COLUMN charging_plan VARCHAR(20) DEFAULT ''");
    mysql_query(mysql, "ALTER TABLE CAR_RECORD ADD COLUMN charging_fee DECIMAL(10,2) DEFAULT 0.00");

    // User-vehicle binding table
    mysql_query(mysql,
        "CREATE TABLE IF NOT EXISTS USER_PLATE ("
        "  id INT PRIMARY KEY AUTO_INCREMENT,"
        "  user_id INT NOT NULL,"
        "  license_plate VARCHAR(20) NOT NULL,"
        "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "  UNIQUE KEY idx_user_plate (user_id, license_plate),"
        "  INDEX idx_user_id (user_id)"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4");

    // Clean expired reservations synchronously instead of relying on MySQL event scheduler
    // (event scheduler requires SUPER privilege which may not be available)
    {
        int expire_min = AppConfig::instance().notice_expire_minutes;
        std::string cleanSql = "UPDATE RESERVATION SET status='expired' WHERE status='active' AND created_at < DATE_SUB(NOW(), INTERVAL " +
            std::to_string(expire_min) + " MINUTE)";
        mysql_query(mysql, cleanSql.c_str());
    }

    return true;
}
