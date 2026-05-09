# 智能图像采集与无线传输系统 - FPGA 部分

## 项目概述

本项目实现了基于 FPGA 的智能图像采集与无线传输系统，通过 SPI 接口将 OV5640 摄像头采集的 JPEG 图像数据传输到 ESP32-C3，再由 ESP32 通过 WiFi 上传到服务器。

## 目录结构

```
FPGA/rtl/
├── image_capture_system.v      # 系统顶层模块
├── image_capture_system.qsf    # Quartus 引脚分配文件
├── uart_cmd_parser/           # UART 指令解析模块
│   ├── uart_rx.v              # UART 接收器
│   └── uart_cmd_parser.v       # 指令解析主模块
├── buzzer/                     # 蜂鸣器控制模块
│   └── buzzer_pwm_ctrl.v       # PWM 蜂鸣器驱动
├── display/                    # 数码管显示模块
│   └── rtc_display_74hc595.v   # 时间显示驱动
├── jpeg_capture/               # JPEG 捕获模块
│   └── ov5640_jpeg_capture.v   # OV5640 JPEG 采集
├── spi_master/                 # SPI 主设备模块
│   └── spi_master_tx.v         # SPI 发送模块
├── ov5640/                     # 原始 OV5640 模块
│   ├── ov5640_top.v
│   ├── ov5640_cfg.v
│   ├── ov5640_data.v
│   └── i2c_ctrl.v
├── sdram/                      # SDRAM 控制器
│   ├── sdram_top.v
│   ├── sdram_ctrl.v
│   ├── sdram_init.v
│   ├── sdram_arbit.v
│   ├── sdram_a_ref.v
│   └── fifo_ctrl.v
├── hdmi/                       # HDMI 显示模块
│   ├── hdmi_ctrl.v
│   ├── encode.v
│   └── par_to_ser.v
└── vga_ctrl.v
```

## 模块说明

### 1. uart_cmd_parser - UART 指令解析模块

**功能**：接收并解析 PC 或外部设备发送的 ASCII 指令

**指令格式**：
```
CAM|<Device_ID>|<Token>|<Timestamp>\n
```

**示例**：
```
CAM|1001-01-01|token_001|1712800000\n
```

**输出信号**：
- `cmd_valid`: 指令有效脉冲
- `device_id`: 设备 ID (32位)
- `token`: 认证令牌 (64位)
- `timestamp`: UNIX 时间戳 (32位)

### 2. buzzer_pwm_ctrl - 蜂鸣器控制模块

**功能**：接收到有效指令后，驱动无源蜂鸣器发出 2 秒提示音

**规格**：
- 频率：3kHz
- 持续时间：2 秒
- 占空比：50%

### 3. rtc_display_74hc595 - 时间显示模块

**功能**：
- 将 UNIX 时间戳转换为北京时间 (UTC+8)
- 通过 74HC595 移位寄存器驱动 6 位数码管
- 显示格式：HH.MM.SS

**74HC595 接口**：
- `ds`: 串行数据
- `shcp`: 移位时钟
- `stcp`: 锁存时钟
- `oe`: 输出使能 (低有效)

### 4. ov5640_jpeg_capture - JPEG 捕获模块

**功能**：
- 通过 SCCB (类 I2C) 配置 OV5640 摄像头
- 输出 JPEG 格式图像
- 检测 JPEG 帧边界 (SOI: 0xFF 0xD8, EOI: 0xFF 0xD9)

**分辨率**：640x480

### 5. spi_master_tx - SPI 发送模块

**功能**：将数据打包并通过 SPI 发送至 ESP32-C3

**数据包格式**：
```
[0xAA 0xBB]        - 包头 (2字节)
[ID_LEN][ID...]    - 设备ID长度 + 数据
[TOKEN_LEN][TOKEN]  - Token长度 + 数据
[TS_LEN][TS...]     - 时间戳长度 + 数据
[FILE_SIZE(4B)]    - JPEG 文件大小 (大端)
[JPEG_DATA...]      - JPEG 图像数据
```

**SPI 规格**：
- 时钟：10MHz
- 模式：SPI Mode 0
- 位序：MSB First

### 6. sdram_frame_buffer - SDRAM 缓存

复用现有的 `sdram_top.v` 模块，实现 JPEG 数据的跨时钟域缓存。

## 引脚分配

详见 `image_capture_system.qsf` 文件。

| 功能 | FPGA 引脚 | 说明 |
|:---|:---|:---|
| sys_clk | PIN_E1 | 50MHz 系统时钟 |
| sys_rst_n | PIN_M15 | 复位按钮 |
| uart_rx | PIN_N6 | UART 接收 |
| beep | PIN_J11 | 蜂鸣器 PWM |
| spi_sck | PIN_A10 | SPI 时钟 |
| spi_mosi | PIN_B10 | SPI MOSI |
| spi_miso | PIN_A9 | SPI MISO |
| spi_cs_n | PIN_B9 | SPI 片选 |

## 使用方法

### 1. Quartus 工程设置

1. 打开 Quartus Prime
2. 创建新工程，选择 Cyclone IV EP4CE10F17C8N
3. 添加所有 `.v` 文件到工程
4. 导入 `image_capture_system.qsf` 引脚分配文件
5. 综合 (Analysis & Synthesis)
6. 编译 (Compile)
7. 编程 (Program)

### 2. 硬件连接

```
+------------------+       +------------------+
|      FPGA       |       |    ESP32-C3      |
+------------------+       +------------------+
| PIN_A10 - SCK --+--------+- GPIO2 (HSPI)   |
| PIN_B10 - MOSI -+--------+- GPIO3 (HSPI)   |
| PIN_A9  - MISO -+--------+- GPIO10 (HSPI)  |
| PIN_B9  - CS ---+--------+- GPIO6 (HSPI)   |
| GND     - GND --+--------+- GND            |
+------------------+       +------------------+
```

### 3. 测试步骤

#### 步骤 1: 验证 UART 接收
通过串口发送：`CAM|1001-01-01|token_001|1712800000\n`
观察蜂鸣器是否响 2 秒，数码管是否显示时间。

#### 步骤 2: 验证 JPEG 捕获
观察 OV5640 摄像头输出，确认 JPEG 数据被正确捕获。

#### 步骤 3: 验证 SPI 传输
使用逻辑分析仪或示波器观察 SPI 信号，确认数据包格式正确。

### 4. 服务器测试

确保服务器运行在 `http://47.107.41.102:9000`，使用 Python 测试脚本：

```python
import requests

response = requests.post(
    "http://47.107.41.102:9000/api/device/vision",
    headers={
        "Device_ID": "1001-01-01",
        "Token": "token_001",
        "Timestamp": "1712800000"
    },
    data=open("test.jpg", "rb").read()
)
print(response.status_code)
```

## 注意事项

1. **OV5640 配置**：JPEG 模式寄存器配置需要参考 OV5640 数据手册
2. **SDRAM 时序**：确保 SDRAM 控制器正确初始化
3. **SPI 时钟**：根据 FPGA 和 ESP32 的能力调整 SPI 时钟频率
4. **JPEG 边界检测**：确保准确检测 SOI 和 EOI 标记

## 状态机流程

```
IDLE -> WAIT_CMD -> CAPTURE -> SAVE_SDRAM -> SEND_SPI -> DONE
```

1. `IDLE`: 等待 UART 指令
2. `WAIT_CMD`: 等待摄像头配置完成
3. `CAPTURE`: 捕获 JPEG 帧
4. `SAVE_SDRAM`: 保存到 SDRAM
5. `SEND_SPI`: 通过 SPI 发送到 ESP32
6. `DONE`: 完成，返回 IDLE

## 扩展功能

### 启用 HDMI 显示

如需在捕获图像的同时显示实时画面，可以：
1. 修改 `ov5640_jpeg_capture.v` 输出 RGB565 格式
2. 添加 `vga_ctrl` 和 `hdmi_ctrl` 模块
3. 在系统状态机中添加显示模式

### 批量拍摄

修改指令解析器，支持连续拍摄：
```
CAM|<Device_ID>|<Token>|<Timestamp>|<Count>\n
```

## 维护记录

- 2026-05-01: 初始版本创建
