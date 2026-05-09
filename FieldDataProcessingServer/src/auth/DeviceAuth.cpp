#include "DeviceAuth.h"
#include "../db/MySQLDriver.h"

bool DeviceAuth::authenticate(const std::string& device_id, const std::string& token) {
    std::string safe_id = MySQLDriver::getInstance().escapeString(device_id);
    std::string safe_token = MySQLDriver::getInstance().escapeString(token);
    std::string sql = "SELECT status FROM device_auth WHERE device_id = '" + safe_id + "' AND token = '" + safe_token + "'";
    auto result = MySQLDriver::getInstance().query(sql);
    
    if (result.empty()) return false; // 找不到设备或Token不匹配
    return result[0]["status"] == "1"; // 检查是否启用
}