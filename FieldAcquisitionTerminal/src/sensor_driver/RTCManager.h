#pragma once
#include <Arduino.h>
#include <RTClib.h>

class RTCManager {
public:
    static RTCManager& getInstance() {
        static RTCManager instance;
        return instance;
    }

    void begin();
    uint32_t getTimestamp();              // 获取当前秒级时间戳
    void syncTime(uint32_t server_time);  // 从服务器获取时间后，同步给本地
    String formatTime(uint32_t timestamp);// 将时间戳转为 "2026/04/10 12:00:00"

private:
    RTCManager() = default;
    RTC_DS3231 rtc;
    bool has_ds3231 = false;
};