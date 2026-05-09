#include "FPGAManager.h"
#include "../../config.h"
#include "driver/gpio.h" // 🌟 引入底层驱动增强信号

FPGAManager& FPGAManager::getInstance() {
    static FPGAManager instance;
    return instance;
}

void FPGAManager::begin() {
    Serial2.end();
    delay(50);
    
    // 强制使用配置里的 115200 波特率！
    Serial2.begin(FPGA_UART_BAUD, SERIAL_8N1, -1, FPGA_UART_TX_PIN);
    
    // 注入满级驱动电流（黑科技）
    gpio_set_drive_capability((gpio_num_t)FPGA_UART_TX_PIN, GPIO_DRIVE_CAP_3);

    Serial.printf("[FPGA] UART2 Initialized: TX=GPIO%d, Baud=%d, Drive=MAX\n", FPGA_UART_TX_PIN, FPGA_UART_BAUD);
}

void FPGAManager::triggerCapture(uint32_t timestamp) {
    // 4. 发送正式的业务命令，严格遵守 CAM|<Device_ID>|<Token>|<Timestamp>\r\n 格式
    Serial2.printf("CAM|%s|%s|%u\n", DEVICE_ID, DEVICE_TOKEN, timestamp);
    
    // 终端打印日志，方便调试监控
    Serial.printf("[FPGA] Capture triggered: CAM|%s|%s|%u\n", DEVICE_ID, DEVICE_TOKEN, timestamp);
}