#pragma once
#include <string>

class PasswordHasher {
public:
    // 将明文密码哈希化
    static std::string hash(const std::string& password);
    // 验证密码是否匹配
    static bool verify(const std::string& hash, const std::string& password);
};