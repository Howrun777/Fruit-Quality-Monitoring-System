#include "BuzzerManager.h"
#include "../config.h"

void BuzzerManager::begin() {
    // ✅ 核心拦截：如果是 -1 或 255，就不执行底层硬件命令
    if (BUZZER_PIN == -1 || BUZZER_PIN == 255) return; 
    
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW); // 默认关闭
}

void BuzzerManager::beep(uint32_t duration_ms) {
    if (BUZZER_PIN == -1 || BUZZER_PIN == 255) return; // ✅ 拦截
    
    digitalWrite(BUZZER_PIN, HIGH);
    is_beeping = true;
    beep_start_time = millis();
    beep_duration = duration_ms;
}

void BuzzerManager::loop() {
    if (BUZZER_PIN == -1 || BUZZER_PIN == 255) return; // ✅ 拦截
    
    if (is_beeping) {
        if (millis() - beep_start_time >= beep_duration) {
            digitalWrite(BUZZER_PIN, LOW); // 时间到了，关掉蜂鸣器
            is_beeping = false;
        }
    }
}