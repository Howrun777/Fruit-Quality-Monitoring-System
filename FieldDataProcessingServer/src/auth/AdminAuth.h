#pragma once
#include <string>

class AdminAuth {
public:
    // 登录验证，成功返回 session_id，失败返回空字符串
    static std::string login(const std::string& username, const std::string& password);
};
