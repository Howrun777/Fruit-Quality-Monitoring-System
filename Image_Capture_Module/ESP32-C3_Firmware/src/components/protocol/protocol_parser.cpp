/**
 * @file protocol_parser.cpp
 * @brief FPGA Communication Protocol Parser Implementation (最终定长协议规范)
 */

#include "protocol_parser.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"

static const char* TAG = "PROTOCOL";

// ============================================================
// Public Functions
// ============================================================

void protocol_parser_init(protocol_parser_t* parser, uint8_t* buffer, size_t buffer_size)
{
    if (parser == nullptr || buffer == nullptr) return;

    memset(parser, 0, sizeof(protocol_parser_t));
    parser->buffer = buffer;
    parser->buffer_size = buffer_size;
    parser->state = PROTOCOL_STATE_IDLE;
    
    ESP_LOGI(TAG, "Protocol parser initialized (Strict Fixed-Length Format). Buffer size: %d", buffer_size);
}

void protocol_parser_reset(protocol_parser_t* parser)
{
    if (parser == nullptr) return;

    parser->state = PROTOCOL_STATE_IDLE;
    parser->data_index = 0;
    parser->ts_val = 0;
    parser->file_size = 0;
    parser->buffer_pos = 0;
    parser->jpeg_start_offset = 0;
    
    memset(parser->device_id, 0, sizeof(parser->device_id));
    memset(parser->token, 0, sizeof(parser->token));
    memset(parser->timestamp, 0, sizeof(parser->timestamp));
}

uint8_t protocol_parser_feed(protocol_parser_t* parser, const uint8_t* data, size_t len)
{
    if (parser == nullptr || data == nullptr || len == 0) return PROTOCOL_STATE_ERROR;

    if (parser->state == PROTOCOL_STATE_COMPLETE || parser->state == PROTOCOL_STATE_ERROR) {
        return parser->state;
    }

    for (size_t i = 0; i < len; i++) {
        uint8_t byte = data[i];

        switch (parser->state) {
            case PROTOCOL_STATE_IDLE:
                // 1. 寻找帧头第一字节: 0x55
                if (byte == 0x55) {
                    parser->state = PROTOCOL_STATE_HEADER_2;
                }
                break;

            case PROTOCOL_STATE_HEADER_2:
                // 2. 寻找帧头第二字节: 0xAA
                if (byte == 0xAA) {
                    parser->data_index = 0;
                    parser->state = PROTOCOL_STATE_DEVICE_ID;
                } else if (byte != 0x55) {
                    parser->state = PROTOCOL_STATE_IDLE; // 没匹配上，重新寻找 0x55
                }
                break;

            case PROTOCOL_STATE_DEVICE_ID:
                // 3. 接收 32 字节的 Device ID
                parser->device_id[parser->data_index++] = byte;
                if (parser->data_index >= 32) {
                    parser->device_id[32] = '\0';
                    // 自动剥离 FPGA 补足的末尾空格 (Space = 0x20)
                    for (int j = 31; j >= 0; j--) {
                        if (parser->device_id[j] == ' ') {
                            parser->device_id[j] = '\0';
                        } else {
                            break;
                        }
                    }
                    ESP_LOGI(TAG, "Device ID Parsed: '%s'", parser->device_id);
                    parser->data_index = 0;
                    parser->state = PROTOCOL_STATE_TOKEN;
                }
                break;

            case PROTOCOL_STATE_TOKEN:
                // 4. 接收 32 字节的 Token
                parser->token[parser->data_index++] = byte;
                if (parser->data_index >= 32) {
                    parser->token[32] = '\0';
                    // 自动剥离 FPGA 补足的末尾空格
                    for (int j = 31; j >= 0; j--) {
                        if (parser->token[j] == ' ') {
                            parser->token[j] = '\0';
                        } else {
                            break;
                        }
                    }
                    ESP_LOGD(TAG, "Token Parsed: '%s'", parser->token);
                    parser->data_index = 0;
                    parser->ts_val = 0;
                    parser->state = PROTOCOL_STATE_TIMESTAMP;
                }
                break;

            case PROTOCOL_STATE_TIMESTAMP:
                // 5. 接收 4 字节的 Timestamp (大端序)
                parser->ts_val = (parser->ts_val << 8) | byte;
                parser->data_index++;
                if (parser->data_index >= 4) {
                    // 将数值转换为供 HTTP 鉴权使用的 ASCII 字符串
                    snprintf(parser->timestamp, sizeof(parser->timestamp), "%lu", (unsigned long)parser->ts_val);
                    ESP_LOGD(TAG, "Timestamp Parsed: %s", parser->timestamp);
                    
                    parser->data_index = 0;
                    parser->file_size = 0;
                    parser->state = PROTOCOL_STATE_FILE_SIZE;
                }
                break;

            case PROTOCOL_STATE_FILE_SIZE:
                // 6. 接收 4 字节的 JPEG Size (大端序)
                parser->file_size = (parser->file_size << 8) | byte;
                parser->data_index++;
                if (parser->data_index >= 4) {
                    ESP_LOGI(TAG, "JPEG Size Parsed: %lu bytes", (unsigned long)parser->file_size);
                    
                    if (parser->file_size == 0 || parser->file_size > parser->buffer_size) {
                        ESP_LOGE(TAG, "Error: Invalid File Size!");
                        parser->state = PROTOCOL_STATE_ERROR;
                    } else {
                        parser->jpeg_start_offset = parser->buffer_pos;
                        parser->state = PROTOCOL_STATE_JPEG_DATA;
                    }
                }
                break;

            case PROTOCOL_STATE_JPEG_DATA:
                // 7. 接收 JPEG 数据，填入大缓存
                if (parser->buffer_pos < parser->buffer_size) {
                    parser->buffer[parser->buffer_pos++] = byte;
                }
                
                if ((parser->buffer_pos - parser->jpeg_start_offset) >= parser->file_size) {
                    parser->state = PROTOCOL_STATE_TRAILER_1;
                }
                break;

            case PROTOCOL_STATE_TRAILER_1:
                // 8. 校验帧尾第一字节: 0xCC
                if (byte == 0xCC) {
                    parser->state = PROTOCOL_STATE_TRAILER_2;
                } else {
                    ESP_LOGE(TAG, "Error: Invalid Trailer 1 (Expected 0xCC, got 0x%02X)", byte);
                    parser->state = PROTOCOL_STATE_ERROR;
                }
                break;

            case PROTOCOL_STATE_TRAILER_2:
                // 9. 校验帧尾第二字节: 0x33
                if (byte == 0x33) {
                    ESP_LOGI(TAG, "Packet strictly validated and completely received!");
                    parser->state = PROTOCOL_STATE_COMPLETE;
                } else {
                    ESP_LOGE(TAG, "Error: Invalid Trailer 2 (Expected 0x33, got 0x%02X)", byte);
                    parser->state = PROTOCOL_STATE_ERROR;
                }
                break;

            default:
                parser->state = PROTOCOL_STATE_ERROR;
                break;
        }

        // 如果包完整或出错，直接跳出处理循环交由外部任务接管
        if (parser->state == PROTOCOL_STATE_ERROR || parser->state == PROTOCOL_STATE_COMPLETE) {
            break;
        }
    }

    return parser->state;
}

const char* protocol_parser_state_str(uint8_t state)
{
    switch (state) {
        case PROTOCOL_STATE_IDLE:      return "IDLE";
        case PROTOCOL_STATE_HEADER_2:  return "HEADER_2";
        case PROTOCOL_STATE_DEVICE_ID: return "DEVICE_ID";
        case PROTOCOL_STATE_TOKEN:     return "TOKEN";
        case PROTOCOL_STATE_TIMESTAMP: return "TIMESTAMP";
        case PROTOCOL_STATE_FILE_SIZE: return "FILE_SIZE";
        case PROTOCOL_STATE_JPEG_DATA: return "JPEG_DATA";
        case PROTOCOL_STATE_TRAILER_1: return "TRAILER_1";
        case PROTOCOL_STATE_TRAILER_2: return "TRAILER_2";
        case PROTOCOL_STATE_COMPLETE:  return "COMPLETE";
        case PROTOCOL_STATE_ERROR:     return "ERROR";
        default:                       return "UNKNOWN";
    }
}

bool protocol_parser_is_complete(protocol_parser_t* parser) {
    return (parser != nullptr) && (parser->state == PROTOCOL_STATE_COMPLETE);
}

bool protocol_parser_has_error(protocol_parser_t* parser) {
    return (parser != nullptr) && (parser->state == PROTOCOL_STATE_ERROR);
}

void protocol_parser_get_packet(protocol_parser_t* parser, parsed_packet_t* packet)
{
    if (parser == nullptr || packet == nullptr) return;

    memset(packet, 0, sizeof(parsed_packet_t));
    if (parser->state != PROTOCOL_STATE_COMPLETE) return;

    strncpy(packet->device_id, parser->device_id, sizeof(packet->device_id) - 1);
    strncpy(packet->token, parser->token, sizeof(packet->token) - 1);
    strncpy(packet->timestamp, parser->timestamp, sizeof(packet->timestamp) - 1);
    packet->file_size = parser->file_size;
    packet->jpeg_data = parser->buffer + parser->jpeg_start_offset;
    packet->valid = true;
}