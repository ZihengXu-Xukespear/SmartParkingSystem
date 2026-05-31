-- Smart Parking Database Schema
-- Run this manually or via the init wizard

CREATE DATABASE IF NOT EXISTS smart_parking
    DEFAULT CHARACTER SET utf8mb4
    DEFAULT COLLATE utf8mb4_unicode_ci;

USE smart_parking;

-- 用户表
CREATE TABLE IF NOT EXISTS USER (
    id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(255) UNIQUE NOT NULL,
    password VARCHAR(64) NOT NULL COMMENT 'SHA-256 hash',
    telephone VARCHAR(11) NOT NULL,
    truename VARCHAR(255) NOT NULL,
    role VARCHAR(20) DEFAULT 'user' COMMENT 'user or admin',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB;

-- 停车场表
CREATE TABLE IF NOT EXISTS PARKING_LOT (
    P_id INT PRIMARY KEY AUTO_INCREMENT,
    P_name VARCHAR(255) UNIQUE NOT NULL,
    P_total_count INT NOT NULL DEFAULT 0 COMMENT '总车位数',
    P_current_count INT NOT NULL DEFAULT 0 COMMENT '当前占用数',
    P_reserve_count INT NOT NULL DEFAULT 0 COMMENT '当前预约数',
    P_fee DECIMAL(10,2) NOT NULL DEFAULT 5.00 COMMENT '每小时费用'
) ENGINE=InnoDB;

-- 车辆记录表
CREATE TABLE IF NOT EXISTS CAR_RECORD (
    id INT PRIMARY KEY AUTO_INCREMENT,
    license_plate VARCHAR(20) NOT NULL,
    check_in_time DATETIME NOT NULL,
    check_out_time DATETIME DEFAULT NULL,
    fee DECIMAL(10,2) DEFAULT NULL,
    location VARCHAR(255) NOT NULL,
    billing_type VARCHAR(20) DEFAULT 'standard',
    spot_number INT DEFAULT 0 COMMENT '车位号',
    reservation_id INT DEFAULT 0,
    exit_deadline DATETIME DEFAULT NULL,
    INDEX idx_plate (license_plate),
    INDEX idx_checkin (check_in_time)
) ENGINE=InnoDB;

-- 预约表
CREATE TABLE IF NOT EXISTS RESERVATION (
    id INT PRIMARY KEY AUTO_INCREMENT,
    license_plate VARCHAR(20) NOT NULL,
    P_name VARCHAR(255) NOT NULL,
    prepaid DECIMAL(10,2) DEFAULT 0,
    status VARCHAR(20) DEFAULT 'active' COMMENT 'active/completed/cancelled/expired',
    spot_number INT DEFAULT 0 COMMENT '预约车位号',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_plate_status (license_plate, status)
) ENGINE=InnoDB;

-- 计费规则表
CREATE TABLE IF NOT EXISTS BILLING_RULE (
    id INT PRIMARY KEY AUTO_INCREMENT,
    rule_name VARCHAR(100) NOT NULL,
    rule_type VARCHAR(20) NOT NULL COMMENT 'standard/tiered/member/special',
    free_minutes INT DEFAULT 30,
    hourly_rate DECIMAL(10,2) DEFAULT 5.00,
    max_daily_fee DECIMAL(10,2) DEFAULT NULL,
    tier_config TEXT DEFAULT NULL COMMENT 'JSON: [{"hours":2,"rate":5},{"hours":4,"rate":3}]',
    description TEXT DEFAULT NULL,
    is_active TINYINT DEFAULT 1
) ENGINE=InnoDB;

-- 月卡表
CREATE TABLE IF NOT EXISTS MONTHLY_PASS (
    id INT PRIMARY KEY AUTO_INCREMENT,
    license_plate VARCHAR(20) NOT NULL,
    pass_type VARCHAR(50) NOT NULL,
    start_date DATE NOT NULL,
    end_date DATE NOT NULL,
    fee DECIMAL(10,2) NOT NULL,
    is_active TINYINT DEFAULT 1,
    INDEX idx_plate (license_plate)
) ENGINE=InnoDB;

-- 插入默认计费规则
INSERT IGNORE INTO BILLING_RULE (rule_name, rule_type, free_minutes, hourly_rate, max_daily_fee, description, is_active) VALUES
('标准计费', 'standard', 30, 5.00, 50.00, '30分钟内免费，之后每小时5元，每日封顶50元', 1),
('阶梯计费', 'tiered', 30, 5.00, 60.00, '前2小时每小时5元，2-4小时每小时3元，4小时以上每小时2元', 0),
('会员计费', 'member', 60, 3.00, 30.00, '会员享受60分钟免费，之后每小时3元，每日封顶30元', 0),
('特殊车辆', 'special', 1440, 0.00, 0.00, '军车、警车、消防车等特殊车辆免费', 0);

-- 预约触发器：插入时增加预约数
DELIMITER //
CREATE TRIGGER IF NOT EXISTS after_reservation_insert
AFTER INSERT ON RESERVATION
FOR EACH ROW
BEGIN
    UPDATE PARKING_LOT SET P_reserve_count = P_reserve_count + 1 WHERE P_name = NEW.P_name;
END//
DELIMITER ;

-- 预约触发器：删除时减少预约数
DELIMITER //
CREATE TRIGGER IF NOT EXISTS after_reservation_delete
AFTER DELETE ON RESERVATION
FOR EACH ROW
BEGIN
    UPDATE PARKING_LOT SET P_reserve_count = GREATEST(P_reserve_count - 1, 0) WHERE P_name = OLD.P_name;
END//
DELIMITER ;

-- 开启事件调度器
SET GLOBAL event_scheduler = ON;

-- 定时清理过期预约（每1分钟清理30分钟前的预约）
DROP EVENT IF EXISTS clean_expired_reservations;
DELIMITER //
CREATE EVENT clean_expired_reservations
ON SCHEDULE EVERY 1 MINUTE
DO
BEGIN
    DELETE FROM RESERVATION WHERE created_at < DATE_SUB(NOW(), INTERVAL 30 MINUTE);
END//
DELIMITER ;
