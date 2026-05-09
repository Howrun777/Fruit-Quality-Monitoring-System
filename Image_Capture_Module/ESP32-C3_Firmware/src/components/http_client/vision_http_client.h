/**
 * @file vision_http_client.h
 * @brief Vision Data HTTP Upload Client Header
 * 
 * This module handles HTTP POST requests to upload vision data
 * (JPEG images with authentication) to the server.
 */

#ifndef VISION_HTTP_CLIENT_H
#define VISION_HTTP_CLIENT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// ============================================================
// Data Structures
// ============================================================

/**
 * @brief Vision upload request structure
 */
typedef struct {
    const char* device_id;
    const char* token;
    const char* timestamp;
    const uint8_t* jpeg_data;
    size_t jpeg_size;
} vision_upload_request_t;

/**
 * @brief Vision upload response structure
 */
typedef struct {
    int status_code;
    char message[256];
    bool success;
} vision_upload_response_t;

/**
 * @brief Upload status callback
 */
typedef void (*upload_status_cb_t)(bool success, const char* message);

// ============================================================
// Function Declarations
// ============================================================

/**
 * @brief Initialize HTTP client
 * @return ESP_OK on success
 */
esp_err_t vision_http_init(void);

/**
 * @brief Deinitialize HTTP client
 */
void vision_http_deinit(void);

/**
 * @brief Upload vision data to server
 * @param request upload request data
 * @param response response data (can be NULL)
 * @return ESP_OK on success
 */
esp_err_t vision_http_upload(const vision_upload_request_t* request,
                             vision_upload_response_t* response);

/**
 * @brief Upload vision data asynchronously
 * @param request upload request data
 * @param callback status callback (can be NULL)
 */
void vision_http_upload_async(const vision_upload_request_t* request,
                              upload_status_cb_t callback);

/**
 * @brief Check if HTTP client is connected to server
 * @return true if connected
 */
bool vision_http_is_connected(void);

/**
 * @brief Get server URL
 * @return server URL string
 */
const char* vision_http_get_server_url(void);

/**
 * @brief Set server URL
 * @param host server host
 * @param port server port
 */
void vision_http_set_server(const char* host, uint16_t port);

#endif // VISION_HTTP_CLIENT_H
