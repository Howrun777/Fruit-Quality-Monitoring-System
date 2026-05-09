#pragma once

#include <Arduino.h> // <--- 加上这一行

class FPGAManager {
public:
    static FPGAManager& getInstance();

    void begin();
    
    // 通过 UART 发送采集触发命令给 FPGA (CAM|<Device_ID>|<Token>|<Timestamp>)
    // device_id 和 token 从 config.h 读取，只需传入 timestamp
    void triggerCapture(uint32_t timestamp);

private:
    FPGAManager() {}
    FPGAManager(const FPGAManager&) = delete;
    FPGAManager& operator=(const FPGAManager&) = delete;
};
