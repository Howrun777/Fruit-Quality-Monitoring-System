/**
 * @file config.h
 * @brief ESP32-C3 Vision Module Configuration (合宙 LuatOS 定制版)
 */

#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// SPI Configuration 
// ============================================================
#define SPI_HOST_ID            SPI2_HOST
#define SPI_DMA_CH             SPI_DMA_CH_AUTO  

// 🌟 根据合宙板子引脚图，挑选的最安全的纯净 GPIO
#define PIN_SPI_SCK  4   // GPIO4
#define PIN_SPI_MOSI 5   // GPIO5
#define PIN_SPI_MISO 6   // GPIO6
#define PIN_SPI_CS   7   // GPIO7

// 🌟 修复：改为 SPI Mode 3 (CPOL=1, CPHA=1) -> 空闲时钟为高电平
#define SPI_MODE 3             
#define SPI_FREQUENCY 10000000 
#define SPI_BIT_ORDER MSBFIRST
#define SPI_RX_BUFFER_SIZE (100 * 1024 + 512)

// ============================================================
// Protocol Configuration (🌟 已适配最终定长规范)
// ============================================================
#define PROTOCOL_HEADER_0       0x55  // 修复：新帧头1
#define PROTOCOL_HEADER_1       0xAA  // 修复：新帧头2
#define PROTOCOL_TRAILER_0      0xCC  // 新增：帧尾1
#define PROTOCOL_TRAILER_1      0x33  // 新增：帧尾2
#define PROTOCOL_HEADER_SIZE    2
#define PROTOCOL_MAX_ID_LEN     32
#define PROTOCOL_MAX_TOKEN_LEN  32    // 修复：新协议Token为32字节
#define PROTOCOL_MAX_JPEG_SIZE  (100 * 1024) 

// ============================================================
// Device Configuration (设备标识)
// ============================================================
#define DEVICE_ID "1001-01-01"
#define DEVICE_TOKEN "device-token-001"

// ============================================================
// WiFi & Server Configuration
// ============================================================
#define WIFI_SSID "potato"
#define WIFI_PASSWORD "696374hu"
#define WIFI_CONNECT_TIMEOUT 30000 

#define SERVER_HOST "47.107.41.102"
#define SERVER_PORT 9000
#define VISION_API_PATH "/api/device/vision"
#define TIME_API_PATH "/api/device/time"

// ============================================================
// Task Configuration
// ============================================================
#define TASK_STACK_SIZE_SMALL 4096
#define TASK_STACK_SIZE_MEDIUM 8192
#define TASK_STACK_SIZE_LARGE 16384
#define TASK_PRIORITY_HIGH 10
#define TASK_PRIORITY_MEDIUM 5
#define TASK_PRIORITY_LOW 1

#define DEBUG_ENABLED 1
#define DEBUG_PRINTF(...) if (DEBUG_ENABLED) Serial.printf(__VA_ARGS__)
#define DEBUG_SPI_DUMP_BYTES 128

#endif // CONFIG_H
