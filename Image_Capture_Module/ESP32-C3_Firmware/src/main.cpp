/**
 * @file main.cpp
 * @brief ESP32-C3 Vision Module Main Entry Point
 */

#include <Arduino.h>
#include <WiFi.h>  
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include "include/config.h"
#include "components/spi_slave/spi_slave.h"
#include "components/protocol/protocol_parser.h"
#include "components/http_client/vision_http_client.h"
#include "components/wifi/wifi_manager.h"

static const char* TAG = "MAIN";

// ============================================================
// Global Variables
// ============================================================

#define RX_BUFFER_SIZE (PROTOCOL_MAX_JPEG_SIZE + 512)
// 动态分配内存，保护 Bootloader
static uint8_t* s_spi_packet_buffer = nullptr;
static uint8_t* s_jpeg_buffer = nullptr;

static protocol_parser_t s_parser;
static TaskHandle_t s_data_processing_task_handle = nullptr;
static TaskHandle_t s_upload_task_handle = nullptr;

static SemaphoreHandle_t s_data_ready_semaphore = nullptr;
static StaticSemaphore_t s_data_ready_semaphore_buffer;
static SemaphoreHandle_t s_upload_done_semaphore = nullptr;
static StaticSemaphore_t s_upload_done_semaphore_buffer;

static QueueHandle_t s_upload_queue = nullptr;
static StaticQueue_t s_upload_queue_buffer;
static uint8_t s_upload_queue_storage[10 * sizeof(parsed_packet_t)];

static void init_all_components(void);
static void spi_data_task(void* pvParameters);
static void data_processing_task(void* pvParameters);
static void upload_task(void* pvParameters);
static void wifi_status_callback(wifi_status_t status, const char* ssid, const char* ip);
static void upload_vision_data(parsed_packet_t* packet);
static void print_system_info(void);
static void blink_led_task(void* pvParameters);

// 🌟 合宙 LuatOS 板子自带的 LED 测试灯通常在 GPIO12 或 GPIO13
#define LED_PIN 12  

// ============================================================
// Setup & Loop
// ============================================================
void setup() {
    Serial.begin(115200);
    delay(1000);
    
    ESP_LOGI(TAG, "==============================================");
    ESP_LOGI(TAG, "LuatOS ESP32-C3 Vision Firmware Started");
    ESP_LOGI(TAG, "==============================================");

    print_system_info();
    
    // 动态分配 100KB 的空间
    s_spi_packet_buffer = (uint8_t*)heap_caps_malloc(RX_BUFFER_SIZE, MALLOC_CAP_DMA);
    s_jpeg_buffer = (uint8_t*)heap_caps_malloc(PROTOCOL_MAX_JPEG_SIZE, MALLOC_CAP_8BIT);
    if (s_spi_packet_buffer == nullptr || s_jpeg_buffer == nullptr) {
        ESP_LOGE(TAG, "Fatal Error: Failed to allocate image receive buffers!");
        while(1) { delay(100); }
    }
    
    init_all_components();

    s_data_ready_semaphore = xSemaphoreCreateBinaryStatic(&s_data_ready_semaphore_buffer);
    s_upload_done_semaphore = xSemaphoreCreateBinaryStatic(&s_upload_done_semaphore_buffer);
    s_upload_queue = xQueueCreateStatic(10, sizeof(parsed_packet_t), s_upload_queue_storage, &s_upload_queue_buffer);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // 全部绑定到单核 (Core 0)
    xTaskCreatePinnedToCore(&spi_data_task, "spi_data", TASK_STACK_SIZE_MEDIUM, NULL, TASK_PRIORITY_HIGH, &s_data_processing_task_handle, 0);
    xTaskCreatePinnedToCore(&data_processing_task, "data_proc", TASK_STACK_SIZE_LARGE, NULL, TASK_PRIORITY_MEDIUM, NULL, 0);
    xTaskCreatePinnedToCore(&upload_task, "upload", TASK_STACK_SIZE_MEDIUM, NULL, TASK_PRIORITY_MEDIUM, &s_upload_task_handle, 0);
    xTaskCreatePinnedToCore(&blink_led_task, "blink", 2048, NULL, TASK_PRIORITY_LOW, NULL, 0);

    ESP_LOGI(TAG, "System initialization complete");
}

void loop() {
    delay(1000);
    static uint32_t last_status_time = 0;
    uint32_t current_time = millis();
    
    // 1. 每 10 秒打印一次系统状态
    if (current_time - last_status_time > 10000) {
        last_status_time = current_time;
        wifi_info_t wifi_info;
        wifi_manager_get_info(&wifi_info);
        ESP_LOGI(TAG, "[Status] WiFi: %s, Queue: %d pending, Free Heap: %d",
                 wifi_info.status == WIFI_STATUS_CONNECTED ? "Connected" : "Disconnected",
                 uxQueueMessagesWaiting(s_upload_queue), ESP.getFreeHeap());
    }
    /*
    // ============================================================
    // 🌟 API 测试：开机连上 WiFi 5秒后，自动伪造一次数据发给服务器
    // ============================================================
    static bool mock_sent = false; 
    
    if (!mock_sent && wifi_manager_is_connected() && current_time > 5000) {
        ESP_LOGW(TAG, "==== TRIGGERING MOCK API UPLOAD ====");

        // 1. 准备一张 158 字节的有效微型纯黑 JPEG 图片（硬编码 16 进制数组）
        static const uint8_t fake_jpeg[] = {
            0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46, 0x00, 0x01, 0x01, 0x01, 0x00, 0x48, 
            0x00, 0x48, 0x00, 0x00, 0xFF, 0xDB, 0x00, 0x43, 0x00, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 
            0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF, 0xC0, 0x00, 0x0B, 0x08, 0x00, 0x01, 
            0x00, 0x01, 0x01, 0x01, 0x11, 0x00, 0xFF, 0xC4, 0x00, 0x14, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xC4, 0x00, 0x14, 
            0x10, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
            0x00, 0x00, 0xFF, 0xDA, 0x00, 0x08, 0x01, 0x01, 0x00, 0x00, 0x3F, 0x00, 0x3F, 0xFF, 0xD9
        };

        // 2. 构造一个完美的伪造业务包
        parsed_packet_t mock_packet;
        memset(&mock_packet, 0, sizeof(parsed_packet_t));
        
        // 填入你指定的测试信息
        strncpy(mock_packet.device_id, DEVICE_ID, sizeof(mock_packet.device_id) - 1);
        strncpy(mock_packet.token, DEVICE_TOKEN, sizeof(mock_packet.token) - 1);
        strncpy(mock_packet.timestamp, "1777887600", sizeof(mock_packet.timestamp) - 1);
        
        // 绑定图像数据
        mock_packet.file_size = sizeof(fake_jpeg);
        mock_packet.jpeg_data = (uint8_t*)fake_jpeg; 
        mock_packet.valid = true;

        // 3. 把这个包直接塞进 HTTP 上传队列（绕过 SPI 解析环节）
        if (xQueueSend(s_upload_queue, &mock_packet, pdMS_TO_TICKS(1000)) == pdTRUE) {
            ESP_LOGI(TAG, "Mock packet successfully queued! HTTP Task will handle it.");
        } else {
            ESP_LOGE(TAG, "Failed to queue mock packet!");
        }

        // 设置标志位，确保整个生命周期内只发这一次，防止刷爆服务器
        mock_sent = true; 
    }
    // ============================================================
    // 🌟 API 测试over：开机连上 WiFi 5秒后，自动伪造一次数据发给服务器
    // ============================================================
    */
}
// ============================================================
// Initialization
// ============================================================
static void init_all_components(void) {
    if (wifi_manager_init() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WiFi manager");
    } else {
        wifi_manager_register_callback(wifi_status_callback);
        wifi_manager_connect(WIFI_SSID, WIFI_PASSWORD, WIFI_CONNECT_TIMEOUT);
    }

    spi_slave_config_t spi_config = {
        .sck_io = (gpio_num_t)PIN_SPI_SCK,
        .mosi_io = (gpio_num_t)PIN_SPI_MOSI,
        .miso_io = (gpio_num_t)PIN_SPI_MISO,
        .cs_io = (gpio_num_t)PIN_SPI_CS,
        .clock_speed_hz = SPI_FREQUENCY
    };
    if (spi_slave_init(&spi_config)) spi_slave_start();

    protocol_parser_init(&s_parser, s_jpeg_buffer, PROTOCOL_MAX_JPEG_SIZE);
    vision_http_init();
}

static void print_system_info(void) {
    ESP_LOGI(TAG, "Chip: %s, CPU: %d MHz, Free Heap: %d bytes", ESP.getChipModel(), ESP.getCpuFreqMHz(), ESP.getFreeHeap());
}

// ============================================================
// Tasks
// ============================================================
static void spi_data_task(void* pvParameters) {
    while (true) {
        if (spi_slave_data_available()) {
            size_t len = spi_slave_get_received_len();
            if (len > 0) {
                size_t read_len = spi_slave_read(s_spi_packet_buffer, RX_BUFFER_SIZE);
                
                if (read_len > 0) {
                    uint8_t state = protocol_parser_feed(&s_parser, s_spi_packet_buffer, read_len);
                    if (protocol_parser_is_complete(&s_parser)) {
                        digitalWrite(LED_PIN, HIGH);
                        xSemaphoreGive(s_data_ready_semaphore);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void data_processing_task(void* pvParameters) {
    parsed_packet_t packet;
    while (true) {
        if (xSemaphoreTake(s_data_ready_semaphore, portMAX_DELAY) == pdTRUE) {
            protocol_parser_get_packet(&s_parser, &packet);
            if (packet.valid) {
                if (xQueueSend(s_upload_queue, &packet, pdMS_TO_TICKS(1000)) == pdTRUE) {
                    ESP_LOGI(TAG, "Packet queued! Waiting for HTTP upload to complete...");
                    xSemaphoreTake(s_upload_done_semaphore, portMAX_DELAY);
                }
            }
            protocol_parser_reset(&s_parser);
            ESP_LOGI(TAG, "Parser reset, ready for next image");
        }
    }
}

static void upload_task(void* pvParameters) {
    parsed_packet_t packet;
    while (true) {
        if (xQueueReceive(s_upload_queue, &packet, portMAX_DELAY) == pdTRUE) {
            if (!wifi_manager_is_connected()) {
                wifi_manager_connect(WIFI_SSID, WIFI_PASSWORD, WIFI_CONNECT_TIMEOUT);
            }
            upload_vision_data(&packet);
            xSemaphoreGive(s_upload_done_semaphore);
        }
    }
}

static void blink_led_task(void* pvParameters) {
    while (true) {
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(500));
        digitalWrite(LED_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

static void wifi_status_callback(wifi_status_t status, const char* ssid, const char* ip) {
    if (status == WIFI_STATUS_CONNECTED) ESP_LOGI(TAG, "WiFi connected! IP: %s", ip);
}

static void upload_vision_data(parsed_packet_t* packet) {
    if (packet == nullptr || !packet->valid) return;
    vision_upload_request_t request = {
        .device_id = packet->device_id,
        .token = packet->token,
        .timestamp = packet->timestamp,
        .jpeg_data = packet->jpeg_data,
        .jpeg_size = packet->file_size
    };
    vision_upload_response_t response;
    esp_err_t err = vision_http_upload(&request, &response);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Upload SUCCESS! Msg: %s", response.message);
    } else {
        ESP_LOGE(TAG, "Upload FAILED! Code: %d", response.status_code);
    }
}
