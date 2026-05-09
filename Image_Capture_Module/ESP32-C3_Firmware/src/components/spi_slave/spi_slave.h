/**
 * @file spi_slave.h
 * @brief ESP32-C3 SPI Slave DMA Driver Header
 * 
 * This module implements SPI Slave mode with DMA for receiving
 * high-speed data from FPGA (e.g., JPEG image data).
 */

#ifndef SPI_SLAVE_H
#define SPI_SLAVE_H

#include <stdint.h>
#include <stdbool.h>
#include <driver/gpio.h>
#include <esp_err.h>

// ============================================================
// Constants
// ============================================================

#define SPI_SLAVE_MODE_DEDI    0

// ============================================================
// Data Structures
// ============================================================

/**
 * @brief SPI transfer result status
 */
typedef enum {
    SPI_SLAVE_OK = 0,
    SPI_SLAVE_TIMEOUT,
    SPI_SLAVE_ERROR,
    SPI_SLAVE_CRC_ERROR
} spi_slave_status_t;

/**
 * @brief SPI slave configuration
 */
typedef struct {
    gpio_num_t sck_io;
    gpio_num_t mosi_io;
    gpio_num_t miso_io;
    gpio_num_t cs_io;
    uint32_t clock_speed_hz;
} spi_slave_config_t;

// ============================================================
// Function Declarations
// ============================================================

/**
 * @brief Initialize SPI slave interface with DMA
 * @param config SPI slave configuration
 * @return true if initialization successful
 */
bool spi_slave_init(const spi_slave_config_t* config);

/**
 * @brief Deinitialize SPI slave interface
 */
void spi_slave_deinit(void);

/**
 * @brief Start listening for SPI transactions
 * @return true if started successfully
 */
bool spi_slave_start(void);

/**
 * @brief Stop SPI slave listening
 */
void spi_slave_stop(void);

/**
 * @brief Check if data is available in RX buffer
 * @return true if data is available
 */
bool spi_slave_data_available(void);

/**
 * @brief Get received data length
 * @return number of bytes received
 */
size_t spi_slave_get_received_len(void);

/**
 * @brief Read data from RX buffer
 * @param buf destination buffer
 * @param len number of bytes to read
 * @return number of bytes actually read
 */
size_t spi_slave_read(uint8_t* buf, size_t len);

/**
 * @brief Clear RX buffer
 */
void spi_slave_clear_buffer(void);

/**
 * @brief Get last transaction status
 * @return status of last transaction
 */
spi_slave_status_t spi_slave_get_status(void);

/**
 * @brief Print SPI configuration for debugging
 */
void spi_slave_print_config(void);

#endif // SPI_SLAVE_H
