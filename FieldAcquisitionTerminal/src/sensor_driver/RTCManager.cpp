#include "RTCManager.h"
#include "../config.h" // 确保包含 I2C 引脚定义
#include <sys/time.h>

void RTCManager::begin() {
    // 强制将硬件 I2C 映射到 27(SDA) 和 22(SCL)
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    
    if (rtc.begin(&Wire)) {
        has_ds3231 = true;
        Serial.println("DS3231 RTC Init Success!");
        if (rtc.lostPower()) {
            Serial.println("RTC lost power, waiting for NTP/Server sync...");
        } else {
            // 开机时，把 RTC 的时间同步给系统
            syncTime(rtc.now().unixtime());
        }
    } else {
        has_ds3231 = false;
        Serial.println("DS3231 RTC Not Found! Using internal timer.");
    }
}

uint32_t RTCManager::getTimestamp() {
    if (has_ds3231) {
        return rtc.now().unixtime();
    } else {
        // 如果没有外部时钟，使用 ESP32 内部时钟 (断电会归零)
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec;
    }
}

void RTCManager::syncTime(uint32_t server_time) {
    if (server_time < 1000000000) return; // 忽略无效的时间戳

    // 1. 更新 ESP32 内部系统时钟
    struct timeval tv;
    tv.tv_sec = server_time;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    // 2. 如果有 DS3231，同时更新外部硬件时钟
    if (has_ds3231) {
        rtc.adjust(DateTime(server_time));
        Serial.println("DS3231 & System time synced to Server Time!");
    } else {
        Serial.println("System time synced to Server Time!");
    }
}

String RTCManager::formatTime(uint32_t timestamp) {
    if (timestamp < 1000000000) return "Waiting Sync...";
    
    // ✅ 核心修复：手动增加 8 小时的偏移量 (8 * 3600 秒)
    // 这样底层保存的是绝对 UTC 时间戳，但在屏幕上展示时是北京时间
    uint32_t beijing_timestamp = timestamp + (8 * 3600);
    
    DateTime dt(beijing_timestamp);
    char buf[32];
    sprintf(buf, "%04d/%02d/%02d %02d:%02d:%02d", 
            dt.year(), dt.month(), dt.day(), 
            dt.hour(), dt.minute(), dt.second());
    return String(buf);
}