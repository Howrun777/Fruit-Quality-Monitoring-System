#pragma once
#include <ArduinoJson.h>

class NetworkManager {
public:
    // 初始化并尝试连接WiFi (非阻塞，最多等10秒，连不上就放弃进入离线模式)
    static bool connectWiFi();
    
    // 后台维护WiFi状态：如果断开，每30秒尝试重连一次
    static void maintainWiFi();
    
    // 向服务器请求最新时间 (返回 0 表示失败)
    static uint32_t fetchServerTime();
    
    // 上传数据，如果成功且服务器返回了 server_time，将通过指针传回以供校时
    static bool uploadData(const JsonDocument& payload, String& outMsg, uint32_t* outServerTime = nullptr);

private:
    static unsigned long last_reconnect_attempt;
};