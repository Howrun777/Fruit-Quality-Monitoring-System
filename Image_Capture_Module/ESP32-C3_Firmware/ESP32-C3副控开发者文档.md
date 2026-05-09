# ESP32-C3 视觉副控模块开发者文档

> **版本**: v2.1 (更新HTTP请求头字段名)
> **更新时间**: 2026-05-04
> **适配硬件**: 合宙 LuatOS ESP32-C3 开发板

---

## 1. 模块概述

### 1.1 模块定位

ESP32-C3 作为视觉副控，负责接收 FPGA 摄像头数据并上传至云端服务器。

### 1.2 数据流向

```
┌─────────────┐     SPI (10MHz)      ┌─────────────┐     WiFi      ┌──────────────────┐
│  FPGA       │ ──────────────────►  │  ESP32-C3   │ ────────────► │  服务器          │
│ (OV5640)    │   从机接收DMA模式    │ (副控)      │   STA模式     │  47.107.41.102   │
└─────────────┘                      └─────────────┘               │  :9000          │
                                                                       └──────────────────┘
```

### 1.3 核心功能

| 功能 | 说明 |
|------|------|
| SPI从机接收 | 以10MHz DMA方式高速接收FPGA图像数据 |
| 协议解析 | 解析定长协议包(帧头+设备ID+Token+时间戳+JPEG+帧尾) |
| WiFi连接 | STA模式连接路由器，支持自动重连 |
| HTTP上传 | POST图像数据至服务器进行识别 |

---

## 2. 目录结构

```
ESP32-C3_Firmware/
├── platformio.ini                              # PlatformIO 项目配置
│
├── .gitignore                                  # Git 忽略配置
│
├── .vscode/
│   └── extensions.json                         # VSCode 推荐插件
│
├── src/
│   ├── main.cpp                               # 主程序入口
│   │
│   ├── include/
│   │   └── config.h                           # 全局配置参数
│   │
│   └── components/
│       ├── spi_slave/                         # SPI从机驱动模块
│       │   ├── spi_slave.h                    #   驱动头文件
│       │   └── spi_slave.cpp                  #   DMA双缓冲实现
│       │
│       ├── protocol/                           # 协议解析模块
│       │   ├── protocol_parser.h              #   解析器头文件
│       │   └── protocol_parser.cpp           #   状态机实现
│       │
│       ├── wifi/                               # WiFi管理模块
│       │   ├── wifi_manager.h                #   管理器头文件
│       │   └── wifi_manager.cpp              #   连接与重连实现
│       │
│       └── http_client/                        # HTTP客户端模块
│           ├── vision_http_client.h           #   客户端头文件
│           └── vision_http_client.cpp         #   上传实现
│
└── .pio/
    └── build/                                  # 编译输出目录(自动生成)
```

---

## 3. 引脚约束

### 3.1 引脚分配

| GPIO | 功能 | 方向 | 说明 |
|------|------|------|------|
| GPIO4 | SPI_SCK | 输入 | SPI时钟线，由FPGA驱动 |
| GPIO5 | SPI_MOSI | 输入 | 主出从入，数据输入 |
| GPIO6 | SPI_MISO | 输出 | 主入从出（本系统未使用回传） |
| GPIO7 | SPI_CS | 输入 | 片选信号，低电平有效 |
| GPIO12 | LED_STATUS | 输出 | 板载LED状态指示 |

### 3.2 禁用/谨慎使用引脚

| GPIO | 原因 | 建议 |
|------|------|------|
| GPIO2 | 启动模式选择(Strapping) | 避免使用 |
| GPIO8 | Flash电压控制(Strapping) | 避免使用 |
| GPIO9 | 芯片使能(Strapping) | 避免使用 |

> **注意**：这些引脚在启动时有特定功能，配置错误会导致无法启动。

### 3.3 FPGA物理接线表

| ESP32-C3 开发板引脚 | FPGA 开发板 | 信号说明 |
| :--- | :--- | :--- |
| **GND** | **GND** | 信号地 (必须相连) |
| **GPIO 4** | **SPI_SCK** | SPI 时钟线 (FPGA输出) |
| **GPIO 5** | **SPI_MOSI** | 主出从入 (FPGA输出图像数据) |
| **GPIO 6** | **SPI_MISO** | 主入从出 (建议接上) |
| **GPIO 7** | **SPI_CS** | 片选信号 (FPGA输出，低电平有效) |

---

## 4. FPGA通信协议 (定长格式)

### 4.1 数据包格式

```
┌────────────────────────────────────────────────────────────────────────────────────┐
│                                    数据包结构                                         │
├─────────┬────────────┬────────────┬────────────┬────────────┬────────────┬─────────┤
│ 帧头    │ Device_ID  │   Token    │ Timestamp  │ JPEG_Size  │ JPEG_Data  │  帧尾   │
│ 2字节   │  32字节    │   32字节   │   4字节    │   4字节    │  可变长度  │  2字节  │
├─────────┼────────────┼────────────┼────────────┼────────────┼────────────┼─────────┤
│ 0x55AA  │ ASCII不足  │ ASCII不足  │ uint32_t   │ uint32_t   │ 二进制流   │ 0xCC33  │
│         │ 补空格     │ 补空格     │ 大端序     │ 大端序     │            │         │
└─────────┴────────────┴────────────┴────────────┴────────────┴────────────┴─────────┘
```

### 4.2 字段详细说明

| 字段 | 长度 | 类型 | 说明 |
|------|------|------|------|
| Header | 2字节 | 固定值 | `0x55 0xAA` (标识数据包开始) |
| Device_ID | 32字节 | ASCII字符串 | 设备标识，不足32字节补空格(0x20) |
| Token | 32字节 | ASCII字符串 | 认证令牌，不足32字节补空格(0x20) |
| Timestamp | 4字节 | uint32_t | Unix时间戳，大端序(Big-endian) |
| JPEG_Size | 4字节 | uint32_t | JPEG图像大小，大端序(Big-endian) |
| JPEG_Data | 可变 | 二进制流 | 实际的JPEG图像数据 |
| Trailer | 2字节 | 固定值 | `0xCC 0x33` (标识数据包结束) |

**协议总长度**：`78字节固定头 (帧头2 + ID32 + Token32 + 时间戳4 + 文件大小4) + JPEG_Data长度`

### 4.3 FPGA发送时序要求

#### 4.3.1 片选(CS)时序
- **低电平有效**：CS为低时开始传输，高时结束
- **传输期间必须保持低**：整个数据包传输期间(可能长达数十毫秒)CS不能拉高
- **结束间隔**：传输完成后至少10μs高电平才能开始下一帧

#### 4.3.2 SPI时钟参数
| 参数 | 值 |
|------|-----|
| SPI模式 | **Mode 3 (CPOL=1, CPHA=1)** |
| 时钟空闲电平 | 高电平 |
| 数据采样沿 | 上升沿 |
| 时钟频率 | 10 MHz (可降至5MHz调试) |
| 位序 | MSB First |

> **重要**：Mode 3 与旧版文档的Mode 1不同，必须严格匹配！
> - CPOL=1：空闲时钟为**高电平**
> - CPHA=1：数据在**上升沿**被采样

#### 4.3.3 字节序
- Timestamp和JPEG_Size字段使用**Big-endian**(大端序)编码
- 示例：时间戳`0x5F8E4D2A`传输顺序为`5F 8E 4D 2A`

#### 4.3.4 可靠性
- 不支持丢包重传，需确保FPGA数据完整发送
- 建议先用1MHz时钟验证基本通信

---

## 5. 任务架构

### 5.1 任务列表

| 任务名 | 核心 | 优先级 | 栈大小 | 职责 |
|--------|------|--------|--------|------|
| spi_data | Core 0 | 10 (HIGH) | 8KB | SPI数据接收与协议解析 |
| data_proc | Core 0 | 5 (MEDIUM) | 16KB | 协议完成判断与队列入队 |
| upload | Core 0 | 5 (MEDIUM) | 8KB | HTTP上传任务 |
| blink | Core 0 | 1 (LOW) | 2KB | LED状态指示(快闪) |

> **注意**：ESP32-C3为单核处理器(RISC-V)，所有任务必须绑定Core 0。

### 5.2 同步机制

```
┌──────────────────┐      信号量触发       ┌──────────────────┐      队列入队      ┌──────────────────┐
│   spi_data_task  │ ─────────────────►   │ data_proc_task   │ ────────────────► │   upload_task    │
│                  │                       │                  │                   │                  │
│ 接收SPI数据       │                       │ 判断协议完成      │                   │ HTTP POST上传    │
│ 喂给解析器        │                       │ 等待上传完成信号量 │                   │ 发送完成信号量    │
└──────────────────┘                       └──────────────────┘                   └──────────────────┘
```

**同步原语**：

| 名称 | 类型 | 用途 |
|------|------|------|
| `s_data_ready_semaphore` | 二值信号量 | SPI接收完整包后触发解析任务 |
| `s_upload_done_semaphore` | 二值信号量 | 上传完成后解锁，允许解析器接收下一帧 |
| `s_upload_queue` | 队列(容量10) | 解耦解析与上传，存储parsed_packet_t |

**流程说明**：
1. `spi_data_task` 接收SPI数据，喂给协议解析器
2. 协议解析完成后，给` s_data_ready_semaphore`信号
3. `data_proc_task` 收到信号，提取packet并入队到 `s_upload_queue`
4. `upload_task` 从队列取数据，执行HTTP上传
5. 上传完成后，给` s_upload_done_semaphore`信号
6. `data_proc_task` 收到信号后重置解析器，准备接收下一帧

---

## 6. 配置参数

### 6.1 SPI配置

| 参数 | 值 | 说明 |
|------|-----|------|
| SPI主机 | SPI2_HOST | 使用HSPI接口 |
| DMA通道 | SPI_DMA_CH_AUTO | 自动选择 |
| SCK引脚 | GPIO4 | |
| MOSI引脚 | GPIO5 | |
| MISO引脚 | GPIO6 | |
| CS引脚 | GPIO7 | |
| SPI模式 | Mode 3 (CPOL=1, CPHA=1) | **空闲时钟为高** |
| 时钟频率 | 10 MHz | 可降至5MHz调试 |
| 位序 | MSBFIRST | |
| DMA缓冲 | 4096字节 × 2 | Ping-pong双缓冲 |

### 6.2 WiFi配置

| 参数 | 值 |
|------|-----|
| SSID | potato |
| 密码 | 696374hu |
| 连接超时 | 30秒 |
| 安全模式 | WPA2-PSK |
| 重连间隔 | 5秒 |
| 最大重连次数 | 10次 |

### 6.3 服务器配置

| 参数 | 值 |
|------|-----|
| 地址 | 47.107.41.102 |
| 端口 | 9000 |
| 上传路径 | /api/device/vision |
| 请求方法 | POST |
| Content-Type | application/octet-stream |
| **认证头** | **X-Device-ID, X-Device-Token, X-Timestamp** |
| HTTP超时 | 30秒 |

> **注意**：HTTP请求头使用 `X-` 前缀作为自定义认证字段，值从协议包的 Device_ID、Token、Timestamp 字段解析获取。

### 6.4 协议配置

| 参数 | 值 |
|------|-----|
| 帧头 | 0x55 0xAA |
| 帧尾 | 0xCC 0x33 |
| Device_ID长度 | 32字节 |
| Token长度 | 32字节 |
| Timestamp长度 | 4字节 (uint32_t大端序) |
| JPEG_Size长度 | 4字节 (uint32_t大端序) |
| 最大JPEG大小 | 100KB |

---

## 7. 模块API

### 7.1 SPI从机模块 (spi_slave)

```c
// 初始化
bool spi_slave_init(const spi_slave_config_t* config);

// 启动/停止
bool spi_slave_start(void);
void spi_slave_stop(void);

// 数据读取
bool spi_slave_data_available(void);     // 检查数据是否就绪
size_t spi_slave_get_received_len(void); // 获取接收长度
size_t spi_slave_read(uint8_t* buf, size_t len); // 读取数据
void spi_slave_clear_buffer(void);       // 清空缓冲区

// 配置打印
void spi_slave_print_config(void);
```

### 7.2 协议解析模块 (protocol_parser)

```c
// 初始化与重置
void protocol_parser_init(protocol_parser_t* parser, uint8_t* buffer, size_t buffer_size);
void protocol_parser_reset(protocol_parser_t* parser);

// 数据处理
uint8_t protocol_parser_feed(protocol_parser_t* parser, const uint8_t* data, size_t len);

// 状态查询
bool protocol_parser_is_complete(protocol_parser_t* parser);
bool protocol_parser_has_error(protocol_parser_t* parser);
const char* protocol_parser_state_str(uint8_t state);

// 数据提取
void protocol_parser_get_packet(protocol_parser_t* parser, parsed_packet_t* packet);
```

**状态机状态枚举**：

```c
#define PROTOCOL_STATE_IDLE         0   // 空闲，等待帧头
#define PROTOCOL_STATE_HEADER_2     1   // 已收到0x55，等待0xAA
#define PROTOCOL_STATE_DEVICE_ID    2   // 接收Device_ID (32字节)
#define PROTOCOL_STATE_TOKEN        3   // 接收Token (32字节)
#define PROTOCOL_STATE_TIMESTAMP    4   // 接收时间戳 (4字节)
#define PROTOCOL_STATE_FILE_SIZE    5   // 接收文件大小 (4字节)
#define PROTOCOL_STATE_JPEG_DATA    6   // 接收JPEG数据
#define PROTOCOL_STATE_TRAILER_1    7   // 校验帧尾0xCC
#define PROTOCOL_STATE_TRAILER_2    8   // 校验帧尾0x33
#define PROTOCOL_STATE_COMPLETE     9   // 解析完成
#define PROTOCOL_STATE_ERROR        10  // 解析错误
```

### 7.3 WiFi管理模块 (wifi_manager)

```c
// 初始化
esp_err_t wifi_manager_init(void);
void wifi_manager_deinit(void);

// 连接管理
esp_err_t wifi_manager_connect(const char* ssid, const char* password, uint32_t timeout_ms);
void wifi_manager_disconnect(void);
bool wifi_manager_is_connected(void);

// 信息获取
void wifi_manager_get_info(wifi_info_t* info);

// 回调与重连
void wifi_manager_register_callback(wifi_event_cb_t callback);
void wifi_manager_start_reconnect(void);
void wifi_manager_stop_reconnect(void);
```

**WiFi状态枚举**：

```c
typedef enum {
    WIFI_STATUS_IDLE = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_DISCONNECTED,
    WIFI_STATUS_FAILED
} wifi_status_t;
```

### 7.4 HTTP客户端模块 (vision_http_client)

```c
// 初始化
esp_err_t vision_http_init(void);
void vision_http_deinit(void);

// 上传
esp_err_t vision_http_upload(const vision_upload_request_t* request,
                             vision_upload_response_t* response);
void vision_http_upload_async(const vision_upload_request_t* request,
                              upload_status_cb_t callback);

// 连接状态
bool vision_http_is_connected(void);
const char* vision_http_get_server_url(void);
void vision_http_set_server(const char* host, uint16_t port);
```

---

## 8. 调试

### 8.1 串口监控

| 参数 | 值 |
|------|-----|
| 波特率 | 115200 |
| 日志级别 | Debug (CORE_DEBUG_LEVEL=3) |

**日志标签**：

| 标签 | 模块 |
|------|------|
| MAIN | 主程序 |
| SPI_SLAVE | SPI从机驱动 |
| PROTOCOL | 协议解析 |
| WIFI_MGR | WiFi管理 |
| VISION_HTTP | HTTP上传 |

### 8.2 系统状态监控

固件每10秒通过MAIN标签输出：

```
[Status] WiFi: Connected, Queue: 0 pending, Free Heap: xxx
```

### 8.3 常见问题排查

| 现象 | 可能原因 | 排查/解决方法 |
|------|----------|--------------|
| 无法启动 | GPIO2/8/9冲突 | 检查是否有GPIO2/8/9配置 |
| SPI无数据 | 引脚连接错误 | 核对SCK/MOSI/MISO/CS接线 |
| 协议状态卡住 | 帧头不匹配 | 确认FPGA发送0x55 0xAA |
| Invalid Trailer | 高频干扰/丢位 | 降低SPI频率至5MHz |
| WiFi连接失败 | SSID/密码错误 | 检查config.h配置 |
| WiFi断开 | 信号弱/干扰 | 检查路由器信号强度 |
| HTTP上传失败 | 服务器不可达 | 测试网络连通性 |
| 内存不足 | 缓冲区分配失败 | 检查ESP.getFreeHeap() |

### 8.4 调试建议

1. **先用低速验证**：先用1MHz时钟验证基本通信
2. **单独测试模块**：WiFi和HTTP功能可单独测试
3. **监控内存使用**：定期检查`ESP.getFreeHeap()`
4. **降低时钟频率**：如果丢包严重，将SPI频率降至5MHz
5. **检查杜邦线长度**：高频传输建议使用10cm以内的短杜邦线

### 8.5 验证软硬件打通

当FPGA正确发送数据后，串口应打印：

```
I (xxx) PROTOCOL: Device ID Parsed: 'your_device_id'
I (xxx) PROTOCOL: JPEG Size Parsed: xxxx bytes
I (xxx) PROTOCOL: Packet strictly validated and completely received!
I (xxx) MAIN: Upload SUCCESS! Msg: xxx
```

---

## 9. 附录

### 9.1 协议状态机流程图

```
                         ┌──────────┐
                         │  IDLE    │
                         └────┬─────┘
                              │ 收到0x55
                              ▼
                         ┌──────────┐
                         │ HEADER_2 │
                         └────┬─────┘
                              │ 收到0xAA
                              ▼
                         ┌──────────┐
                    ┌───►│ DEVICE_ID│◄───┐
                    │    └────┬─────┘    │
                    │         │ 32字节  │
                    │         ▼          │
                    │    ┌──────────┐    │
                    │    │  TOKEN   │────┘ 空格填充
                    │    └────┬─────┘
                    │         │ 32字节
                    │         ▼
                    │    ┌──────────┐
                    │    │TIMESTAMP │ 4字节
                    │    └────┬─────┘
                    │         ▼
                    │    ┌──────────┐
                    │    │FILE_SIZE │ 4字节
                    │    └────┬─────┘
                    │         ▼
                    │    ┌──────────┐
                    │    │JPEG_DATA │◄───┐ 继续
                    │    └────┬─────┘    │
                    │         │ 完成     │
                    │         ▼          │
                    │    ┌──────────┐    │
                    └──┬─│ TRAILER_1│────┘ 0xCC
                       │ └────┬─────┘
                       │      │ 0x33
                       │      ▼
                       │ ┌──────────┐
                       └►│ COMPLETE │
                         └──────────┘
```

### 9.2 HTTP请求头说明

上传图像时，HTTP请求会包含以下自定义头部：

| 请求头字段 | 来源 | 说明 |
|-----------|------|------|
| `X-Device-ID` | 协议包 Device_ID (32字节) | 设备唯一标识 |
| `X-Device-Token` | 协议包 Token (32字节) | 设备认证令牌 |
| `X-Timestamp` | 协议包 Timestamp (4字节) | Unix时间戳(大端序转ASCII) |
| `Content-Type` | 固定值 | `application/octet-stream` |

**请求示例**：

```http
POST /api/device/vision HTTP/1.1
Host: 47.107.41.102:9000
X-Device-ID: 1001-01-01
X-Device-Token: device-token-001
X-Timestamp: 1777887600
Content-Type: application/octet-stream
Content-Length: 12345

<JPEG二进制数据>
```

### 9.3 修订历史

| 版本 | 日期 | 更新内容 |
|------|------|----------|
| v1.0 | - | 初始文档(变长协议格式) |
| v2.0 | 2026-05-04 | 重构：改为定长协议，新增帧尾，SPI Mode改为3 |
| v2.1 | 2026-05-04 | 更新HTTP请求头字段名，修正协议总长度计算 |

### 9.4 参考资料

- ESP32-C3技术参考手册
- ESP-IDF SPI Slave API文档
- 合宙ESP32-C3开发板引脚图
