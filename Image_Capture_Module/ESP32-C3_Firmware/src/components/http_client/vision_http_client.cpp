/**
 * @file vision_http_client.cpp
 * @brief Vision Data HTTP Upload Client Implementation
 */

#include "vision_http_client.h"
#include "esp_http_client.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "../../include/config.h"

static const char* TAG = "VISION_HTTP";

// Static variables
static char s_server_host[64] = SERVER_HOST;
static uint16_t s_server_port = SERVER_PORT;
static bool s_initialized = false;

// Forward declarations
static esp_err_t http_event_handler(esp_http_client_event_t* evt);
static size_t build_url(char* url_buffer, size_t buffer_size);
static esp_http_client_handle_t create_client(const vision_upload_request_t* request);

// ============================================================
// Public Functions
// ============================================================

esp_err_t vision_http_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "HTTP client already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing HTTP client...");
    ESP_LOGI(TAG, "Server: %s:%d", s_server_host, s_server_port);
    ESP_LOGI(TAG, "Vision API: %s", VISION_API_PATH);

    s_initialized = true;
    ESP_LOGI(TAG, "HTTP client initialized");

    return ESP_OK;
}

void vision_http_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    s_initialized = false;
    ESP_LOGI(TAG, "HTTP client deinitialized");
}

esp_err_t vision_http_upload(const vision_upload_request_t* request,
                             vision_upload_response_t* response)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "HTTP client not initialized");
        return ESP_FAIL;
    }

    if (request == nullptr || request->jpeg_data == nullptr || request->jpeg_size == 0) {
        ESP_LOGE(TAG, "Invalid request parameters");
        return ESP_ERR_INVALID_ARG;
    }

    if (request->device_id == nullptr || request->token == nullptr || 
        request->timestamp == nullptr) {
        ESP_LOGE(TAG, "Missing authentication parameters");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting vision upload...");
    ESP_LOGI(TAG, "  Device ID: %s", request->device_id);
    ESP_LOGI(TAG, "  Timestamp: %s", request->timestamp);
    ESP_LOGI(TAG, "  JPEG size: %d bytes", request->jpeg_size);

    // Create HTTP client
    esp_http_client_handle_t client = create_client(request);
    if (client == nullptr) {
        ESP_LOGE(TAG, "Failed to create HTTP client");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_set_post_field(client, 
                                                   (const char*)request->jpeg_data, 
                                                   request->jpeg_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set post field: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    // ❌ 你的代码 (第 90 行)：esp_err_t err = esp_http_client_perform(client);
    
    // ✅ 修复：去掉 esp_err_t，直接复用刚才的 err 变量
    err = esp_http_client_perform(client);
    int status_code = esp_http_client_get_status_code(client);

    if (response != nullptr) {
        response->status_code = status_code;
        response->success = (err == ESP_OK && status_code == 200);
        
        // Read response body
        char response_buffer[256] = {0};
        int read_len = esp_http_client_read(client, response_buffer, 
                                            sizeof(response_buffer) - 1);
        if (read_len > 0) {
            response_buffer[read_len] = '\0';
            strncpy(response->message, response_buffer, sizeof(response->message) - 1);
        }
    }

    ESP_LOGI(TAG, "HTTP request completed, status: %d", status_code);

    esp_http_client_cleanup(client);

    return (status_code == 200) ? ESP_OK : ESP_FAIL;
}

void vision_http_upload_async(const vision_upload_request_t* request,
                              upload_status_cb_t callback)
{
    // For simplicity, we're doing synchronous upload in a separate task
    // In production, you might want to use FreeRTOS task or event group
    
    if (request == nullptr) {
        if (callback) {
            callback(false, "Invalid request");
        }
        return;
    }

    vision_upload_response_t response;
    esp_err_t err = vision_http_upload(request, &response);

    if (callback) {
        if (err == ESP_OK) {
            callback(true, response.message);
        } else {
            char error_msg[128];
            snprintf(error_msg, sizeof(error_msg), 
                     "Upload failed with status %d: %s", 
                     response.status_code, response.message);
            callback(false, error_msg);
        }
    }
}

bool vision_http_is_connected(void)
{
    // Simple connectivity check - try to connect to server
    // In production, you might want to maintain a connection pool or ping
    
    char url[128];
    snprintf(url, sizeof(url), "http://%s:%d/", s_server_host, s_server_port);
    
    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        return false;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    esp_http_client_cleanup(client);

    return (err == ESP_OK);
}

const char* vision_http_get_server_url(void)
{
    static char url[128];
    snprintf(url, sizeof(url), "http://%s:%d%s", 
             s_server_host, s_server_port, VISION_API_PATH);
    return url;
}

void vision_http_set_server(const char* host, uint16_t port)
{
    if (host != nullptr) {
        strncpy(s_server_host, host, sizeof(s_server_host) - 1);
    }
    if (port > 0) {
        s_server_port = port;
    }
    ESP_LOGI(TAG, "Server URL updated: %s:%d", s_server_host, s_server_port);
}

// ============================================================
// Static Functions
// ============================================================

static esp_err_t http_event_handler(esp_http_client_event_t* evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT, %s", evt->header_key);
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", 
                     evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static size_t build_url(char* url_buffer, size_t buffer_size)
{
    return snprintf(url_buffer, buffer_size, 
                    "http://%s:%d%s", 
                    s_server_host, s_server_port, VISION_API_PATH);
}

static esp_http_client_handle_t create_client(const vision_upload_request_t* request)
{
    char url[256];
    build_url(url, sizeof(url));

    esp_http_client_config_t config = {};
    config.url = url;
    config.method = HTTP_METHOD_POST;
    config.event_handler = http_event_handler;
    config.timeout_ms = 30000;
    config.buffer_size = 4096;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return nullptr;
    }

    // Set headers
    esp_http_client_set_header(client, "X-Device-ID", request->device_id);
    esp_http_client_set_header(client, "X-Device-Token", request->token);
    esp_http_client_set_header(client, "X-Timestamp", request->timestamp);
    esp_http_client_set_header(client, "Content-Type", "application/octet-stream");

    ESP_LOGD(TAG, "HTTP client configured for: %s", url);

    return client;
}
