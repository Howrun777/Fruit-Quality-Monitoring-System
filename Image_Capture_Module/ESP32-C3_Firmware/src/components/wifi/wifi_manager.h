/**
 * @file wifi_manager.h
 * @brief WiFi Manager Header
 * 
 * This module handles WiFi connection management for the ESP32-C3.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// ============================================================
// Constants
// ============================================================

#define WIFI_RECONNECT_INTERVAL_MS    5000
#define WIFI_MAX_RECONNECT_ATTEMPTS   10

// ============================================================
// Data Structures
// ============================================================

/**
 * @brief WiFi connection status
 */
typedef enum {
    WIFI_STATUS_IDLE = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_FAILED
} wifi_status_t;

/**
 * @brief WiFi connection info
 */
typedef struct {
    wifi_status_t status;
    char ssid[32];
    int8_t rssi;
    uint8_t channel;
    char ip_addr[16];
} wifi_info_t;

/**
 * @brief WiFi event callback
 */
typedef void (*wifi_event_cb_t)(wifi_status_t status, const char* ssid, const char* ip);

// ============================================================
// Function Declarations
// ============================================================

/**
 * @brief Initialize WiFi manager
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_init(void);

/**
 * @brief Deinitialize WiFi manager
 */
void wifi_manager_deinit(void);

/**
 * @brief Connect to WiFi network
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @param timeout_ms connection timeout in milliseconds
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_connect(const char* ssid, const char* password, uint32_t timeout_ms);

/**
 * @brief Disconnect from WiFi
 */
void wifi_manager_disconnect(void);

/**
 * @brief Check if WiFi is connected
 * @return true if connected
 */
bool wifi_manager_is_connected(void);

/**
 * @brief Get current WiFi connection info
 * @param info pointer to wifi_info_t structure
 */
void wifi_manager_get_info(wifi_info_t* info);

/**
 * @brief Register WiFi status callback
 * @param callback callback function
 */
void wifi_manager_register_callback(wifi_event_cb_t callback);

/**
 * @brief Start auto-reconnect
 */
void wifi_manager_start_reconnect(void);

/**
 * @brief Stop auto-reconnect
 */
void wifi_manager_stop_reconnect(void);

/**
 * @brief Print WiFi configuration
 */
void wifi_manager_print_config(void);

#endif // WIFI_MANAGER_H
