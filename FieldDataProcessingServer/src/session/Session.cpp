#include "Session.h"
#include <chrono>
#include <random>

Session& Session::getInstance() {
    static Session instance;
    return instance;
}

std::string Session::generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    const char* v = "0123456789abcdef";
    std::string res;
    for (int i = 0; i < 32; i++) res += v[dis(gen)];
    return res;
}

std::string Session::createSession(const std::string& username) {
    std::lock_guard<std::mutex> lock(mtx);
    std::string sid = generateUUID();
    // 有效期 1 小时 (3600秒)
    auto expire_time = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count() + 3600;
    session_map[sid] = expire_time;
    return sid;
}

bool Session::isValid(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mtx);
    if (session_map.find(session_id) == session_map.end()) return false;
    
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
        
    if (now > session_map[session_id]) {
        session_map.erase(session_id); // 过期删除
        return false;
    }
    return true;
}

void Session::destroySession(const std::string& session_id) {
    std::lock_guard<std::mutex> lock(mtx);
    session_map.erase(session_id);
}