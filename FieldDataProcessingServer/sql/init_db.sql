-- ============================================
-- 智能水果（樱桃）无损检测系统 数据库初始化脚本
-- 数据库名：FruitDataBase
-- 版本：v3.0 (新增多模态AI视觉扩展模块)
-- ============================================

-- 创建全新的水果数据库
CREATE DATABASE IF NOT EXISTS FruitDataBase
    DEFAULT CHARSET=utf8mb4
    COLLATE=utf8mb4_unicode_ci;

USE FruitDataBase;


-- ============================================-
-- 1. 管理员信息表
-- ============================================-
CREATE TABLE IF NOT EXISTS admin_users (
    id INT PRIMARY KEY AUTO_INCREMENT,
    username VARCHAR(64) NOT NULL UNIQUE COMMENT '管理员账号（唯一）',
    password_hash VARCHAR(255) NOT NULL COMMENT 'Argon2 哈希后的密码',
    role TINYINT NOT NULL DEFAULT 1 COMMENT '角色：0=超级管理员，1=普通管理员',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================
-- 2. 设备校验表
-- ============================================
CREATE TABLE IF NOT EXISTS device_auth (
    device_id VARCHAR(32) PRIMARY KEY COMMENT '设备编号：产区号-组号-设备号',
    token VARCHAR(64) NOT NULL UNIQUE COMMENT '设备唯一认证Token',
    status TINYINT NOT NULL DEFAULT 1 COMMENT '状态：0=禁用，1=启用',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================
-- 3. 果园生产信息表
-- ============================================
CREATE TABLE IF NOT EXISTS field_production (
    field_id VARCHAR(16) PRIMARY KEY COMMENT '产区号',
    fruit_variety VARCHAR(64) NOT NULL COMMENT '水果品种',
    mature_sugar_threshold DECIMAL(5,2) NOT NULL COMMENT '成熟糖度阈值',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP COMMENT '创建时间',
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP COMMENT '更新时间'
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================
-- 4. 水果果实信息表 (保持原表名避免大修代码)
-- ============================================
CREATE TABLE IF NOT EXISTS fruit_data (
    device_id VARCHAR(32) NOT NULL COMMENT '设备编号',
    collected_at INT NOT NULL COMMENT '数据采集时间戳（秒级）',
    sugar_brix DECIMAL(5,2) NOT NULL COMMENT '当前糖度值（MLR算法）',
    maturity_score DECIMAL(5,3) NOT NULL COMMENT '成熟度评分',
    spectrum_json JSON NOT NULL COMMENT 'AS7341光谱数据',
    PRIMARY KEY (device_id, collected_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================
-- 5. 环境信息表 (✅ 核心升级：新增腐败气体字段)
-- ============================================
CREATE TABLE IF NOT EXISTS field_environment (
    field_id VARCHAR(16) NOT NULL COMMENT '产区号',
    collected_at INT NOT NULL COMMENT '数据采集时间戳',
    temperature_c DECIMAL(5,2) NOT NULL COMMENT '温度（℃）',
    humidity_rh DECIMAL(5,2) NOT NULL COMMENT '湿度（%RH）',
    light_lux INT NOT NULL COMMENT '光照强度（Lux）',
    gas135_ppm DECIMAL(6,2) NOT NULL DEFAULT 99.0 COMMENT '苯类等空气质量浓度', -- ✅ 新增
    gas137_ppm DECIMAL(6,2) NOT NULL DEFAULT 99.0 COMMENT '乙醇浓度',           -- ✅ 新增
    PRIMARY KEY (field_id, collected_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================
-- 6. 樱桃果实视觉与AI分析表 (✅ 多模态AI视觉扩展模块新增)
-- ============================================
CREATE TABLE IF NOT EXISTS fruit_vision_data (
    device_id VARCHAR(32) NOT NULL COMMENT '设备编号',
    collected_at INT NOT NULL COMMENT '采集时间戳（与fruit_data绝对对齐）',
    quality_level TINYINT NOT NULL DEFAULT 0 COMMENT 'AI评估品质梯度(1~10级)，-1表示ONNX不可用/推理失败',
    image_url VARCHAR(255) NOT NULL COMMENT '服务器本地图片相对路径',
    PRIMARY KEY (device_id, collected_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- ============================================
-- 初始测试数据注入
-- ============================================

-- 初始管理员 admin/admin123 (C++代码会自动将其刷为Argon2)
INSERT INTO admin_users (username, password_hash, role) VALUES
('admin', '$2b$12$LQv3c1yqBWVHxkd0LHAkCOYz6TtxMQJqhN8/X4bAq/xxx-placeholder', 0);

-- 注入樱桃产区信息 (假设樱桃成熟糖度在 15.0 ~ 18.0 左右)
INSERT INTO field_production (field_id, fruit_variety, mature_sugar_threshold) VALUES
('1001', 'Bing Cherry (冰樱桃)', 15.00),
('1002', 'Rainier Cherry (雷尼尔)', 16.50),
('1003', 'Brooks Cherry (布鲁克斯)', 14.50);

-- 注入设备权限
INSERT INTO device_auth (device_id, token, status) VALUES
('1001-01-01', 'device-token-001', 1),
('1001-01-02', 'device-token-002', 1),
('1002-01-01', 'device-token-003', 1);