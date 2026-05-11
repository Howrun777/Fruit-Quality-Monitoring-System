/**
 * @file protocol_parser.h
 * @brief FPGA Communication Protocol Parser Header (最终定长协议规范)
 * 
 * 协议格式 (总控制字段 76 字节 + JPEG 可变数据):
 *[0x55 0xAA] (2字节)
 * [Device_ID] (32字节 ASCII，不足补空格)
 * [Token]     (32字节 ASCII，不足补空格)
 *[Timestamp] (4字节 uint32_t 大端序)
 * [JPEG_Size] (4字节 uint32_t 大端序)
 * [JPEG_Data] (可变长度，二进制流)
 *[0xCC 0x33] (2字节，帧尾)
 */

#ifndef PROTOCOL_PARSER_H
#define PROTOCOL_PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <sys/time.h>

// ============================================================
// Constants 状态机枚举
// ============================================================
#define PROTOCOL_STATE_IDLE         0
#define PROTOCOL_STATE_HEADER_2     1
#define PROTOCOL_STATE_DEVICE_ID    2
#define PROTOCOL_STATE_TOKEN        3
#define PROTOCOL_STATE_TIMESTAMP    4
#define PROTOCOL_STATE_FILE_SIZE    5
#define PROTOCOL_STATE_JPEG_DATA    6
#define PROTOCOL_STATE_TRAILER_1    7
#define PROTOCOL_STATE_TRAILER_2    8
#define PROTOCOL_STATE_COMPLETE     9
#define PROTOCOL_STATE_ERROR        10

// ============================================================
// Data Structures
// ============================================================

/**
 * @brief 最终供业务层使用的解析结果
 */
typedef struct {
    char device_id[33];   // 最长 32 字节 + 结束符
    char token[33];       // 最长 32 字节 + 结束符
    char timestamp[32];   // 转换后的字符串格式时间戳
    uint32_t file_size;   // JPEG 图像大小
    uint8_t* jpeg_data;   // 指向图像数据的指针
    bool valid;           // 数据包是否有效
} parsed_packet_t;

/**
 * @brief 协议解析器内部状态机
 */
typedef struct {
    uint8_t state;
    uint32_t data_index;
    
    // 临时存储区
    uint32_t ts_val;
    uint32_t file_size;
    char device_id[33];
    char token[33];
    char timestamp[32];
    
    // 缓冲区控制
    uint8_t* buffer;
    size_t buffer_size;
    size_t buffer_pos;
    size_t jpeg_start_offset;
    
    // 超时控制 (微秒)
    int64_t last_byte_time;
    int64_t timeout_us;
} protocol_parser_t;

// ============================================================
// Function Declarations
// ============================================================
void protocol_parser_init(protocol_parser_t* parser, uint8_t* buffer, size_t buffer_size);
void protocol_parser_reset(protocol_parser_t* parser);
uint8_t protocol_parser_feed(protocol_parser_t* parser, const uint8_t* data, size_t len);
const char* protocol_parser_state_str(uint8_t state);
bool protocol_parser_is_complete(protocol_parser_t* parser);
bool protocol_parser_has_error(protocol_parser_t* parser);
void protocol_parser_get_packet(protocol_parser_t* parser, parsed_packet_t* packet);

#endif // PROTOCOL_PARSER_H