/**
 * @file wifi_manager.cpp
 * @brief WiFi Manager Implementation
 */
#include <esp_log.h>
#include "wifi_manager.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include <string.h>
#include "esp_netif.h"
#include "../../include/config.h"

static const char* TAG = "WIFI_MGR";

// Static variables
static bool s_initialized = false;
static bool s_auto_reconnect = true;
static bool s_reconnect_running = false;
static wifi_info_t s_wifi_info = {};
static wifi_event_cb_t s_event_callback = nullptr;
static TaskHandle_t s_reconnect_task_handle = nullptr;

// Forward declarations
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data);
static void reconnect_task(void* pvParameters);

// ============================================================
// Public Functions
// ============================================================

esp_err_t wifi_manager_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "WiFi manager already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing WiFi manager safely...");

    // 1. Initialize NVS (允许重复初始化)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 2. Initialize TCP/IP stack (去掉 ERROR_CHECK，因为 Arduino 已经初始化过了)
    esp_netif_init();
    
    // 3. Create default event loop
    // 🚨 拆除炸弹：如果 Arduino 已经创建过了，会返回 INVALID_STATE，我们忽略它即可！
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to create event loop: %d", err);
    }

    // 4. Create default WiFi station
    esp_netif_create_default_wifi_sta();

    // 5. WiFi configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) {
        ESP_LOGE(TAG, "WiFi init failed: %d", err);
    }

    // 6. Register event handlers
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL);

    // 7. Set WiFi mode
    esp_wifi_set_mode(WIFI_MODE_STA);

    // Initialize WiFi info
    memset(&s_wifi_info, 0, sizeof(s_wifi_info));
    s_wifi_info.status = WIFI_STATUS_IDLE;

    s_initialized = true;
    ESP_LOGI(TAG, "WiFi manager initialized SUCCESSFULLY");

    return ESP_OK;
}

void wifi_manager_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    wifi_manager_stop_reconnect();
    esp_wifi_stop();
    esp_wifi_deinit();
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);

    s_initialized = false;
    ESP_LOGI(TAG, "WiFi manager deinitialized");
}

esp_err_t wifi_manager_connect(const char* ssid, const char* password, uint32_t timeout_ms)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "WiFi manager not initialized");
        return ESP_FAIL;
    }

    if (ssid == nullptr || password == nullptr) {
        ESP_LOGE(TAG, "Invalid SSID or password");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);

    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    // 🚨 拆除炸弹：去掉 ESP_ERROR_CHECK
    if (esp_wifi_set_config(WIFI_IF_STA, &wifi_config) != ESP_OK) {
        return ESP_FAIL;
    }
    
    s_wifi_info.status = WIFI_STATUS_CONNECTING;
    strncpy(s_wifi_info.ssid, ssid, sizeof(s_wifi_info.ssid) - 1);

    if (esp_wifi_start() != ESP_OK) {
        return ESP_FAIL;
    }

    // Wait for connection
    uint32_t start_time = xTaskGetTickCount();
    while (s_wifi_info.status == WIFI_STATUS_CONNECTING) {
        vTaskDelay(pdMS_TO_TICKS(100));
        if ((xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS > timeout_ms) {
            ESP_LOGE(TAG, "WiFi connection timeout");
            s_wifi_info.status = WIFI_STATUS_FAILED;
            return ESP_ERR_TIMEOUT;
        }
    }

    return (s_wifi_info.status == WIFI_STATUS_CONNECTED) ? ESP_OK : ESP_FAIL;
}

void wifi_manager_disconnect(void)
{
    if (!s_initialized) return;
    ESP_LOGI(TAG, "Disconnecting from WiFi...");
    s_auto_reconnect = false;
    esp_wifi_disconnect();
    s_wifi_info.status = WIFI_STATUS_DISCONNECTED;
}

bool wifi_manager_is_connected(void) {
    return s_wifi_info.status == WIFI_STATUS_CONNECTED;
}

void wifi_manager_get_info(wifi_info_t* info) {
    if (info != nullptr) memcpy(info, &s_wifi_info, sizeof(wifi_info_t));
}

void wifi_manager_register_callback(wifi_event_cb_t callback) {
    s_event_callback = callback;
}

void wifi_manager_start_reconnect(void) {
    if (s_reconnect_running) return;
    s_auto_reconnect = true;
    s_reconnect_running = true;
    xTaskCreatePinnedToCore(&reconnect_task, "wifi_reconnect", 4096, NULL, 1, &s_reconnect_task_handle, 0);
}

void wifi_manager_stop_reconnect(void) {
    s_auto_reconnect = false;
    s_reconnect_running = false;
    if (s_reconnect_task_handle != nullptr) {
        vTaskDelete(s_reconnect_task_handle);
        s_reconnect_task_handle = nullptr;
    }
}

void wifi_manager_print_config(void) {
    ESP_LOGI(TAG, "=== WiFi Config ===");
    ESP_LOGI(TAG, "SSID: %s", s_wifi_info.ssid);
    ESP_LOGI(TAG, "Status: %d", s_wifi_info.status);
    ESP_LOGI(TAG, "IP: %s", s_wifi_info.ip_addr);
}

// ============================================================
// Static Functions
// ============================================================

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi station started");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Connected to AP");
                break;
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t* disconnected = 
                    (wifi_event_sta_disconnected_t*)event_data;
                ESP_LOGI(TAG, "Disconnected from AP, reason: %d", disconnected->reason);
                
                if (s_wifi_info.status == WIFI_STATUS_CONNECTED) {
                    s_wifi_info.status = WIFI_STATUS_DISCONNECTED;
                    if (s_event_callback) {
                        s_event_callback(s_wifi_info.status, s_wifi_info.ssid, s_wifi_info.ip_addr);
                    }
                }
                if (s_auto_reconnect) {
                    ESP_LOGI(TAG, "Attempting to reconnect...");
                    esp_wifi_connect();
                }
                break;
            }
            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        
        s_wifi_info.status = WIFI_STATUS_CONNECTED;
        snprintf(s_wifi_info.ip_addr, sizeof(s_wifi_info.ip_addr), 
                IPSTR, IP2STR(&event->ip_info.ip));

        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            s_wifi_info.rssi = ap_info.rssi;
            s_wifi_info.channel = ap_info.primary;
        }

        if (s_event_callback) {
            s_event_callback(s_wifi_info.status, s_wifi_info.ssid, s_wifi_info.ip_addr);
        }
    }
}

static void reconnect_task(void* pvParameters)
{
    ESP_LOGI(TAG, "WiFi reconnect task started");
    while (s_reconnect_running && s_auto_reconnect) {
        if (s_wifi_info.status != WIFI_STATUS_CONNECTED) {
            ESP_LOGI(TAG, "Attempting WiFi reconnection...");
            esp_wifi_connect();
        }
        vTaskDelay(pdMS_TO_TICKS(WIFI_RECONNECT_INTERVAL_MS));
    }
    vTaskDelete(NULL);
}