#pragma once
#include <Arduino.h>

class BuzzerManager {
public:
    static BuzzerManager& getInstance() {
        static BuzzerManager instance;
        return instance;
    }

    void begin();
    
    // 让蜂鸣器响指定的毫秒数 (异步非阻塞)
    void beep(uint32_t duration_ms);
    
    // 必须放在 loop() 里不断调用以维护状态
    void loop();

private:
    BuzzerManager() = default;
    bool is_beeping = false;
    uint32_t beep_start_time = 0;
    uint32_t beep_duration = 0;
};