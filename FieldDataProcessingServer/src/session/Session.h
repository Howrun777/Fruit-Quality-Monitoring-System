#pragma once
#include <string>
#include <unordered_map>
#include <mutex>

class Session {
public:
    static Session& getInstance();
    
    // 创建一个 session，返回 session_id
    std::string createSession(const std::string& username);
    // 检查 session_id 是否有效
    bool isValid(const std::string& session_id);
    // 销毁 session
    void destroySession(const std::string& session_id);

private:
    Session() = default;
    ~Session() = default;

    // 存储 session_id -> 过期时间戳
    std::unordered_map<std::string, long long> session_map;
    std::mutex mtx;
    
    std::string generateUUID();
};