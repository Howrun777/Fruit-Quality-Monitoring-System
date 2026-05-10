/**
 * @file spi_slave.cpp
 * @brief ESP32-C3 SPI Slave DMA Driver Implementation
 * 
 * This module implements SPI Slave mode with DMA for receiving
 * high-speed data from FPGA (e.g., JPEG image data).
 */

#include "spi_slave.h"
#include "../../include/config.h" 
#include <string.h>
#include <Arduino.h>
#include <esp_log.h>
#include <driver/spi_slave.h>

static const char* TAG = "SPI_SLAVE";

// ============================================================
// Static Variables
// ============================================================

static spi_bus_config_t s_bus_config;
static spi_slave_interface_config_t s_slave_config;
static bool s_initialized = false;
static bool s_running = false;

static spi_slave_status_t s_last_status = SPI_SLAVE_OK;

// Receive buffer (ping-pong for DMA)
static uint8_t s_rx_buffer_0[SPI_RX_BUFFER_SIZE] __attribute__((aligned(4)));
static uint8_t* s_current_rx_buffer = s_rx_buffer_0;
static uint8_t* s_processing_buffer = s_rx_buffer_0;
static size_t s_received_len = 0;
static volatile bool s_data_ready = false;
static spi_slave_transaction_t s_transaction = {};

// ============================================================
// Static Function Declarations
// ============================================================

static void IRAM_ATTR spi_slave_transaction_cb(spi_slave_transaction_t* trans);
static bool queue_receive_transaction(void);

// ============================================================
// Public Functions
// ============================================================

bool spi_slave_init(const spi_slave_config_t* config)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "SPI slave already initialized");
        return true;
    }

    if (config == nullptr) {
        ESP_LOGE(TAG, "Invalid configuration");
        return false;
    }

    ESP_LOGI(TAG, "Initializing SPI Slave (HSPI)...");
    ESP_LOGI(TAG, "  SCK: GPIO%d", config->sck_io);
    ESP_LOGI(TAG, "  MOSI: GPIO%d", config->mosi_io);
    ESP_LOGI(TAG, "  MISO: GPIO%d", config->miso_io);
    ESP_LOGI(TAG, "  CS: GPIO%d", config->cs_io);
    ESP_LOGI(TAG, "  Clock: %d Hz", config->clock_speed_hz);

    // Initialize GPIO pins
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->sck_io) | 
                        (1ULL << config->mosi_io) | 
                        (1ULL << config->cs_io),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    gpio_config(&io_conf);

    // MISO as output
    io_conf.pin_bit_mask = (1ULL << config->miso_io);
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);

    // SPI bus configuration
    s_bus_config = (spi_bus_config_t){
        .mosi_io_num = config->mosi_io,
        .miso_io_num = config->miso_io,
        .sclk_io_num = config->sck_io,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SPI_RX_BUFFER_SIZE,
    };

    // Configure slave interface
    s_slave_config = (spi_slave_interface_config_t){
        .spics_io_num = config->cs_io,
        .flags = 0,
        .queue_size = 3,
        .mode = SPI_MODE,
        .post_setup_cb = nullptr,
        .post_trans_cb = spi_slave_transaction_cb,
    };
    // Initialize SPI bus as slave
    esp_err_t ret = spi_slave_initialize(SPI_HOST_ID, &s_bus_config, &s_slave_config, SPI_DMA_CH);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI slave: %s", esp_err_to_name(ret));
        return false;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "SPI slave initialized successfully");

    return true;
}

void spi_slave_deinit(void)
{
    if (!s_initialized) {
        return;
    }

    spi_slave_stop();
    
    // ✅ 直接释放总线即可，不再需要判断那个不存在的 handle 了
    spi_slave_free(SPI_HOST_ID);
    
    s_initialized = false;
    ESP_LOGI(TAG, "SPI slave deinitialized");
}

bool spi_slave_start(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "SPI slave not initialized");
        return false;
    }

    if (s_running) {
        ESP_LOGW(TAG, "SPI slave already running");
        return true;
    }

    s_running = true;
    s_last_status = SPI_SLAVE_OK;

    if (!queue_receive_transaction()) {
        s_running = false;
        s_last_status = SPI_SLAVE_ERROR;
        return false;
    }

    ESP_LOGI(TAG, "SPI slave started, waiting for data...");

    return true;
}

void spi_slave_stop(void)
{
    if (!s_running) {
        return;
    }

    s_running = false;
    ESP_LOGI(TAG, "SPI slave stopped");
}

bool spi_slave_data_available(void)
{
    return s_data_ready;
}

size_t spi_slave_get_received_len(void)
{
    return s_received_len;
}

size_t spi_slave_read(uint8_t* buf, size_t len)
{
    if (!s_data_ready || buf == nullptr) {
        return 0;
    }

    size_t copy_len = (len < s_received_len) ? len : s_received_len;
    memcpy(buf, s_processing_buffer, copy_len);
    spi_slave_transaction_t* completed_transaction = nullptr;
    spi_slave_get_trans_result(SPI_HOST_ID, &completed_transaction, 0);
    
    // Clear ready flag after reading
    s_data_ready = false;
    queue_receive_transaction();
    
    return copy_len;
}

void spi_slave_clear_buffer(void)
{
    s_data_ready = false;
    s_received_len = 0;
    memset(s_rx_buffer_0, 0, SPI_RX_BUFFER_SIZE);
}

spi_slave_status_t spi_slave_get_status(void)
{
    return s_last_status;
}

void spi_slave_print_config(void)
{
    ESP_LOGI(TAG, "=== SPI Slave Configuration ===");
    ESP_LOGI(TAG, "Initialized: %s", s_initialized ? "Yes" : "No");
    ESP_LOGI(TAG, "Running: %s", s_running ? "Yes" : "No");
    ESP_LOGI(TAG, "RX Buffer Size: %d bytes", SPI_RX_BUFFER_SIZE);
    ESP_LOGI(TAG, "Last Status: %d", s_last_status);
}

// ============================================================
// Static Functions
// ============================================================

static void IRAM_ATTR spi_slave_transaction_cb(spi_slave_transaction_t* trans)
{
    if (trans == nullptr || trans->trans_len == 0) {
        return;
    }

    // Calculate received bytes (trans_len is in bits)
    size_t received_bytes = trans->trans_len / 8;
    
    if (received_bytes > 0 && received_bytes <= SPI_RX_BUFFER_SIZE) {
        // Update received data info
        s_processing_buffer = (uint8_t*)trans->rx_buffer;
        s_received_len = received_bytes;
        s_data_ready = true;
        s_last_status = SPI_SLAVE_OK;
    } else {
        s_last_status = SPI_SLAVE_ERROR;
    }
}

static bool queue_receive_transaction(void)
{
    if (!s_running || s_data_ready) {
        return false;
    }

    memset(&s_transaction, 0, sizeof(s_transaction));
    memset(s_current_rx_buffer, 0, SPI_RX_BUFFER_SIZE);
    s_transaction.length = SPI_RX_BUFFER_SIZE * 8;
    s_transaction.rx_buffer = s_current_rx_buffer;

    esp_err_t ret = spi_slave_queue_trans(SPI_HOST_ID, &s_transaction, portMAX_DELAY);
    if (ret != ESP_OK) {
        s_last_status = SPI_SLAVE_ERROR;
        ESP_LOGE(TAG, "Failed to queue SPI transaction: %s", esp_err_to_name(ret));
        return false;
    }

    return true;
}
