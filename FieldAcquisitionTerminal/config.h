

#pragma once

// ======= 设备与网络配置 =======
#define DEVICE_ID "1001-01-01"
#define FRUIT_VARIETY "Cherry" // 水果品种
#define DEVICE_TOKEN "device-token-001"

#define WIFI_SSID "Howrun777"
#define WIFI_PASS "28983904"
#define SERVER_URL "http://47.107.41.102:9000/api/device/upload"

// ======= I2C 传感器总线引脚 =======
#define I2C_SDA_PIN 27
#define I2C_SCL_PIN 22

// ======= 独立触摸屏 SPI 总线引脚 =======
#define TOUCH_MOSI_PIN 32
#define TOUCH_MISO_PIN 39
#define TOUCH_CLK_PIN  25
#define TOUCH_CS_PIN   33
#define TOUCH_IRQ_PIN  36

// ======= 主控 → FPGA 通信引脚 =======
// 使用无滤波电容的大焊盘 TF 卡槽引脚 IO5
#define FPGA_UART_TX_PIN 5
#define FPGA_UART_BAUD 9600

// ======= 蜂鸣器引脚（已禁用） =======

#define BUZZER_PIN -1 // 已禁用，GPIO 26 用于 FPGA 通信

// 新增：外部气体传感器模块 (Arduino) 串口通信引脚
#define GAS_RX_PIN 35
#define GAS_TX_PIN -1 // 不需要 TX，因为 ESP32 只是接收数据
#define GAS_BAUD_RATE 9600

// ======= 业务定时配置 =======
#define ENV_UPDATE_INTERVAL 2000    // UI环境数据刷新频率: 2秒
#define AUTO_UPLOAD_INTERVAL 300000 // 自动检测上传频率: 5分钟 (300000毫秒)