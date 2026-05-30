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
    if (!hasBillingRules) {
        const char* billingSQL =
            "INSERT INTO BILLING_RULE (rule_name,rule_type,free_minutes,hourly_rate,max_daily_fee,description,is_active,P_name) VALUES "
            "('标准计费','standard',30,5.00,50.00,'30分钟内免费，之后每小时5元，每日封顶50元',1,'%s'),"
            "('阶梯计费','tiered',30,5.00,60.00,'前2小时每小时5元，2-4小时每小时3元，4小时以上每小时2元',0,'%s'),"
            "('会员计费','member',60,3.00,30.00,'会员享受60分钟免费，之后每小时3元，每日封顶30元',0,'%s'),"
            "('特殊车辆','special',1440,0.00,0.00,'军车、警车、消防车等特殊车辆免费',0,'%s')";
        // Get all parking lot names
        MYSQL_RES* lotRes = nullptr;
        if (mysql_query(mysql, "SELECT P_name FROM PARKING_LOT") == 0)
            lotRes = mysql_store_result(mysql);
        if (lotRes) {
            MYSQL_ROW lrow;
            while ((lrow = mysql_fetch_row(lotRes))) {
                std::string pn = lrow[0] ? lrow[0] : "停车场1";
                char buf[2048];
                snprintf(buf, sizeof(buf), billingSQL, pn.c_str(), pn.c_str(), pn.c_str(), pn.c_str());
                mysql_query(mysql, buf);
            }
            mysql_free_result(lotRes);
        }
    }

    mysql_query(mysql, "SELECT COUNT(*) FROM PASS_PLAN");
    MYSQL_RES* ppRes = mysql_store_result(mysql);
    bool hasPassPlans = false;
    if (ppRes) {
        MYSQL_ROW ppRow = mysql_fetch_row(ppRes);
        if (ppRow && std::stoi(ppRow[0]) > 0) hasPassPlans = true;
        mysql_free_result(ppRes);
    }
    if (!hasPassPlans) {
        const char* planSQL =
            "INSERT INTO PASS_PLAN (plan_name,duration_days,price,description,is_active,P_name) VALUES "
            "('月卡',30,300.00,'30天畅停',1,'%s'),"
            "('季卡',90,800.00,'90天优惠',1,'%s'),"
            "('年卡',365,2880.00,'全年无忧',1,'%s')";
        MYSQL_RES* lotRes2 = nullptr;
        if (mysql_query(mysql, "SELECT P_name FROM PARKING_LOT") == 0)
            lotRes2 = mysql_store_result(mysql);
        if (lotRes2) {
            MYSQL_ROW lrow;
            while ((lrow = mysql_fetch_row(lotRes2))) {
                std::string pn = lrow[0] ? lrow[0] : "停车场1";
                char buf[1024];
                snprintf(buf, sizeof(buf), planSQL, pn.c_str(), pn.c_str(), pn.c_str());
                mysql_query(mysql, buf);
            }
            mysql_free_result(lotRes2);
        }
    }

    std::string parking_sql = "INSERT IGNORE INTO PARKING_LOT (P_name,P_total_count,P_current_count,P_reserve_count,P_fee) VALUES ('" +
        cfg.parking_name + "'," + std::to_string(cfg.capacity) + ",0,0," + std::to_string(cfg.fee) + ")";
    mysql_query(mysql, parking_sql.c_str());

    // Second parking lot (20-spot ring layout)
    mysql_query(mysql,
        "INSERT IGNORE INTO PARKING_LOT (P_name,P_total_count,P_current_count,P_reserve_count,P_fee) VALUES "
        "('停车场2',20,0,0,5.00)");

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
    mysql_query(mysql, "ALTER TABLE BILLING_RULE ADD COLUMN P_name VARCHAR(255) DEFAULT ''");
    mysql_query(mysql, "ALTER TABLE PASS_PLAN ADD COLUMN P_name VARCHAR(255) DEFAULT ''");
    // Backfill NULL P_name values on existing rows
    mysql_query(mysql, "UPDATE BILLING_RULE SET P_name='' WHERE P_name IS NULL");
    mysql_query(mysql, "UPDATE PASS_PLAN SET P_name='' WHERE P_name IS NULL");
    // Add UNIQUE constraints to prevent duplicate inserts on restart
    mysql_query(mysql, "ALTER TABLE BILLING_RULE ADD UNIQUE INDEX idx_rule_type (rule_type, P_name)");
    mysql_query(mysql, "ALTER TABLE PASS_PLAN ADD UNIQUE INDEX idx_plan_name (plan_name, P_name)");

    // Remove old parking lot name
    mysql_query(mysql, "DELETE FROM PARKING_LOT WHERE P_name='智慧停车场'");
    mysql_query(mysql, "DELETE FROM PARKING_LOT WHERE P_name='SmartParking'");

    // Fix old UNIQUE constraint on license_plate -> regular index
    mysql_query(mysql, "ALTER TABLE RESERVATION DROP INDEX idx_plate");
    mysql_query(mysql, "ALTER TABLE RESERVATION ADD INDEX idx_plate (license_plate)");

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
