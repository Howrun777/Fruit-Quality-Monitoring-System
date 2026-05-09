#include "AdminAuth.h"
#include "../db/MySQLDriver.h"
#include "../utils/PasswordHasher.h"
#include "../session/Session.h"
#include <iostream>

std::string AdminAuth::login(const std::string& username, const std::string& password) {
    std::string safe_user = MySQLDriver::getInstance().escapeString(username); // 防注入
    std::string sql = "SELECT password_hash FROM admin_users WHERE username = '" + safe_user + "'";
    auto result = MySQLDriver::getInstance().query(sql);
    
    if (result.empty()) return ""; // 账号不存在
    
    std::string hash_in_db = result[0]["password_hash"];
    
    if (PasswordHasher::verify(hash_in_db, password)) {
        return Session::getInstance().createSession(username); // 密码正确，签发 Session
    }
    return ""; // 密码错误
}