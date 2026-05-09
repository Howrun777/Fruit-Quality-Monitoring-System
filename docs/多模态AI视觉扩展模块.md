# 智能果园多模态 AI 视觉扩展模块 - 开发者架构白皮书

> **模块代号**：Vision_AI_Extension
>
> **前置依赖**：集成至 `FieldAcquisitionTerminal`（主副控架构）与 `FieldDataProcessingServer`（C++ 云端）
>
> **核心选型定调**：
>
> - **AI 模型**：ConvNeXt-Tiny（纯视觉 CNN 王者，极高细粒度特征提取能力）
> - **图像流转**：OV5640 硬件输出 **640×480 (VGA)** -> SPI 传给 ESP32 -> 云端 C++ 接收 -> **中心裁剪 480×480** -> **缩放 224×224** -> 喂给 ONNX 推理
> - **部署策略**：脱离 Cursor 释放内存 + 队列机制，确保 2核2G 服务器永不 OOM

---

## 1. 架构核心思想 

本扩展模块旨在为原有的"光谱+环境+气体"检测系统引入**"OV5640 图像采集与 ConvNeXt 深度学习推理"**能力，实现对樱桃表面微小瑕疵、色差的 1~10 级细粒度品质分级。

针对 2核2G 云服务器的内存局限，以及 ESP32 边缘侧的内存瓶颈，系统坚决摒弃"主控打包统发"的集中式方案，采用**"主从解耦、分布式上传、凭证动态下发、云端逻辑聚合"**的微服务化架构。

### 1.1 三大核心设计原则

- **身份动态共享**：副 ESP32 (FPGA端) 不硬编码任何身份信息。主控在触发时，通过 UART 将 `device_id`、`token` 和 `timestamp` 动态下发给副机，实现安全鉴权。
- **物理分表存储**：图片和 AI 推理结果在云端采用独立的 `fruit_vision_data` 表存储，杜绝高并发下的事务死锁。
- **云端对齐融合**：Web 端在查询时，后端通过统一的 `(device_id, collected_at)` 联合主键执行 `LEFT JOIN`，将传感器的数值数据与 AI 的视觉数据完美拼接展示。

### 1.2 端-云图像处理流水线

```
OV5640 (640×480 JPEG)
    ↓ DVP
FPGA (缓存于 SDRAM)
    ↓ SPI Master
副控 ESP32 (PSRAM 接收，60KB+ Buffer)
    ↓ Wi-Fi HTTP POST multipart/form-data
云端 C++ 服务器
    ↓ 落盘
stb_image 内存解码
    ↓ Center Crop (640×480 → 480×480)
Resize (480×480 → 224×224)
    ↓ ImageNet 归一化
ONNX Runtime (ConvNeXt-Tiny)
    ↓ quality_level (1~10)
INSERT fruit_vision_data
```

### 1.3 云端防宕机架构

2核2G 服务器（实际可用物理内存 ~600MB）的内存极为紧张。云端 C++ 后端必须采用**快速响应 + 队列缓冲 + 单线程消费**三层架构，确保推理过程永不 OOM：

1. **快速响应层**：HTTP 回调仅完成鉴权 + 落盘，将任务压入 `std::queue<VisionTask>` 后立刻返回 200 OK，绝不在 HTTP 线程中跑模型。
2. **单线程消费层**：后台唯一工作线程（`while(true)`）从队列取任务，完成图像预处理 + ONNX 推理 + 数据入库。
3. **内存隔离**：推理完成后立即释放图像内存，ONNX Session 全局仅保留一个实例。

---

## 2. 硬件间通信协议 (C&C Protocol)

**物理层**：TTL 异步串口 (UART)
**通信方向**：单向通信（主控 ESP32 ➡️ 副控 ESP32 / FPGA）

### 2.1 主控下发触发指令 (UART)

当主控 ESP32 处于工业模式（每 5 分钟定时任务到达）或测试模式（用户主动点击"MEASURE"）时，通过 Serial2 下发"通行证包"。

**数据包格式（明文 ASCII，以 `\n` 结尾）：**

```
CAM|<Device_ID>|<Token>|<Timestamp>\n
```

**示例：**

```
CAM|1001-01-01|device-token-001|1712800000\n
```

| 字段 | 说明 |
|------|------|
| `CAM` | 固定指令头，标识摄像头采集任务 |
| `Device_ID` | 设备编号（与传感器数据共用同一 ID） |
| `Token` | 动态下发的鉴权令牌 |
| `Timestamp` | 采集时间戳（Unix 秒级，用于云端数据对齐） |

### 2.2 副控 (FPGA端) 执行逻辑

1. 串口监听提取出身份凭证（ID, Token）与时间戳对齐凭证（Timestamp）。
2. 驱动 FPGA（配置 OV5640 为 **VGA 640×480**，JPEG 格式）捕获一帧图像，存入 SDRAM。
3. FPGA 经 SPI 高速推流至副控 ESP32（存入 PSRAM）。
4. 副控将凭证与图片组装，发起 `POST /api/device/vision` 直传云服务器。

### 2.3 SPI 数据链路协议 (FPGA → 副控 ESP32)

FPGA 作为 SPI 主机 (Master)，副控 ESP32 作为 SPI 从机 (Slave)，遵循以下自定义数据包格式：

| 帧头 | 数据长度 | 图像负载 | 帧尾 |
|------|----------|----------|------|
| `0x55 0xAA` (2 Byte) | JPEG 字节数 (4 Byte, 大端) | `FF D8 ... FF D9` (JPEG 裸数据) | `0xCC 0x33` (2 Byte) |

---

## 3. 云端服务层改造指南 (FieldDataProcessingServer)

### 3.1 数据库结构升级 (`sql/init_db.sql`)

在现有的 `FruitDataBase` 中，**新增视觉专用数据表**，以 `(device_id, collected_at)` 为联合主键与传感器数据表严格对齐。

```sql
-- 樱桃果实视觉与AI分析表
CREATE TABLE IF NOT EXISTS fruit_vision_data (
    device_id VARCHAR(32) NOT NULL COMMENT '设备编号',
    collected_at INT NOT NULL COMMENT '采集时间戳（与fruit_data绝对对齐）',
    quality_level TINYINT NOT NULL DEFAULT 0 COMMENT 'AI评估品质梯度(1~10级)，-1表示ONNX不可用/推理失败',
    image_url VARCHAR(255) NOT NULL COMMENT '服务器本地图片相对路径',
    PRIMARY KEY (device_id, collected_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;
```

### 3.2 接收图片与防爆内存推理 API (Router.cpp & VisionTask.cpp)

新增路由：`POST /api/device/vision`

**【极其重要】服务器 2核2G 防宕机架构实现：**

#### 快速响应层（Router.cpp）

HTTP 回调中**严禁直接跑模型**，必须严格遵循以下流程：

1. **防爆破鉴权**：提取 Header 中的 `device_id` 和 `token`，调用 `DeviceAuth::authenticate()`。
2. **提取对齐凭证**：提取表单字段 `collected_at`。
3. **落盘存储**：提取表单文件 `image`，以 `<device_id>_<timestamp>.jpg` 命名，保存在 `../../FieldMonitoringPlatform/assets/uploads/` 目录。
4. **入队**：将任务（包含图片路径、device_id、collected_at）压入全局 `std::queue<VisionTask>` 队列。
5. **立刻返回**：向 ESP32 返回 `200 OK`，绝不阻塞。

#### 单线程消费层 (VisionTask.cpp)

后台维护唯一的 `while(true)` 工作线程，从队列取任务执行以下流程：

1. **图像解码**（stb_image）：加载硬盘中的 640×480 JPEG 到内存。
2. **Center Crop**：在 C++ 内存中将 640×480 截取为 **480×480** 像素（防止樱桃形状被压扁导致 AI 识别畸变）。
3. **Resize**：将 480×480 缩放为标准正方形 **224×224**。
4. **归一化**：将 RGB 数据转化为 `std::vector<float>`（形状 1×3×224×224），并进行 ImageNet 标准归一化（mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225]）。
5. **ONNX 可用性检查**：调用 `checkONNXAvailable()` 确认 Session 可用。
   - 若不可用：跳过推理，直接执行步骤 6，数据入库时 `quality_level = -1`（哨兵值，标识推理失败）。
   - 若可用：继续执行步骤 6 推理。
6. **ONNX 推理**：
   - 调用 ONNX Runtime C++ API，全局仅保留一个 Session 实例。
   - 设置线程限制：`SetIntraOpNumThreads(1)`，避免多线程争抢 CPU。
   - 将预处理后的 Tensor 喂给 ConvNeXt-Tiny，获取 1~10 的分类结果。
7. **数据入库**：执行 `INSERT INTO fruit_vision_data`，随后**立刻释放图像内存**。

> **【设计说明】ONNX 降级机制**：图片上传与 AI 推理完全解耦。即使 ONNX 模型文件不存在、加载失败或 Session 异常，图片依然正常落盘入库，`quality_level` 记为 `-1`。此设计使得图片传输功能可独立于 AI 推理进行验证，极大方便了 ESP32 端图片采集与 HTTP 上传链路的调试。

### 3.3 数据下发 API 聚合 (Router.cpp)

修改原有的获取水果历史接口 `GET /api/admin/fruit/history`，使用 `LEFT JOIN` 将传感器数据与环境数据、图片聚合。

```sql
SELECT
    f.collected_at, f.sugar_brix, f.maturity_score,
    e.temperature_c, e.humidity_rh, e.light_lux, e.gas135_ppm, e.gas137_ppm,
    v.quality_level, v.image_url
FROM fruit_data f
LEFT JOIN field_environment e ON f.device_id = e.field_id AND f.collected_at = e.collected_at
LEFT JOIN fruit_vision_data v ON f.device_id = v.device_id AND f.collected_at = v.collected_at
WHERE f.device_id = ? ORDER BY f.collected_at ASC
```

---

## 4. Web 监控大屏适配指南 (FieldMonitoringPlatform)

### 4.1 视图层 (`dashboard.html`)

在樱桃深红主题的大屏中，于"阵列实时状态"左侧或下方，新增**【AI 视觉分析档案】**容器。

- **位置**：独立 Card 组件，建议在左侧阵列状态下方或右侧信息面板中。
- **元素要求**：
  - `<div id="vision-quality-badge">`：展示 ConvNeXt-Tiny 推理出的级别（1~10 数字）。
  - `<img id="vision-photo">`：展示服务器返回的实拍图（限制高宽比为 1:1，匹配 480×480 的视觉感）。

### 4.2 逻辑渲染 (`auto_switch.js`)

前端通过轮询触发 `window.reloadAllChartsData`，从 `/api/admin/fruit/history` 接口获取数据：

1. **定位最新数据**：取数组的最后一个元素（时间戳最新的一次检测）。
2. **容错判空**：检查 `image_url` 字段是否存在（图片走独立上传通道，可能存在延迟）。
3. **动态渲染**：
   - 若存在：修改 `<img src>` 为对应的相对路径，根据 `quality_level` 赋予不同颜色徽章：
     - **Level -1（推理失败）**：灰色背景 `#888888`，图标使用 `⚠`，标识 ONNX 不可用或推理异常。
     - **Level 8-10（优等）**：深红背景 `#8B0000` + 金色边框，传达高品质感。
     - **Level 4-7（中等）**：绯红背景 `#DC143C`，中性提示。
     - **Level 1-3（劣等/瑕疵）**：亮橙告警色 `#FF4500`，并联动腐败气体（gas135/gas137）的告警逻辑。
   - 若不存在：显示一张占位图（Placeholder），提示"当前时刻无视觉快照"。

---

## 5. AI 模型训练对接规范 (Python 端 - RTX 5090 工作站)

基于计算资源和最高精度的诉求，模型在本地 **RTX 5090** 工作站脱机训练，交付 `.onnx` 文件给 C++ 调用。

### 5.1 模型选型

- **架构**：ConvNeXt-Tiny（官方 ImageNet 预训练权重）
- **理由**：ConvNeXt 是纯视觉 CNN 的当代王者，在细粒度图像分类任务（如瑕疵检测）上具备极高特征提取能力，优于 MobileNetV2 等轻量模型。
- **输出修改**：将 ConvNeXt-Tiny 的最后一个 Linear 层输出从 1000 类修改为 **10 类**（对应 1~10 品质梯度）。

### 5.2 数据集规格

| 项目 | 数量 | 说明 |
|------|------|------|
| 训练集 | 5000+ 张 | 覆盖 10 个梯度，涵盖不同品种、成熟度、瑕疵程度 |
| 验证集 | 1000+ 张 | 用于调参与早停 |
| 测试集 | 500+ 张 | 独立样本，用于最终精度评估 |

> 图片来源：实地拍摄（OV5640 VGA 640×480），覆盖不同光照、角度、遮挡场景。

### 5.3 数据预处理代码（必须与 C++ 线上处理绝对一致）

训练时的 Transform **必须严格模拟线上 C++ 预处理流程**，这是确保训练-推理一致性的关键：

```python
import torchvision.transforms as transforms

# 训练时的 Transform（严格模拟服务器 C++ 预处理流程）
train_transform = transforms.Compose([
    # 1. Center Crop：将 640×480 裁成 480×480（极其关键，与 C++ stb_image 裁剪逻辑完全一致）
    transforms.CenterCrop(480),
    # 2. Resize：缩放到 ConvNeXt 标准输入 224×224
    transforms.Resize(224),
    # 3. 数据增强（上线后 C++ 不执行这些操作，仅训练时使用）
    transforms.RandomHorizontalFlip(),
    transforms.RandomRotation(15),
    transforms.ColorJitter(brightness=0.1, contrast=0.1),
    # 4. Tensor 化 + ImageNet 归一化
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
])

val_transform = transforms.Compose([
    transforms.CenterCrop(480),    # 与 C++ 一致
    transforms.Resize(224),         # 与 C++ 一致
    transforms.ToTensor(),
    transforms.Normalize(mean=[0.485, 0.456, 0.406], std=[0.229, 0.224, 0.225])
])
```

### 5.4 模型导出与验证

```python
import torch
import onnxruntime as ort

# 1. 导出 ONNX
torch_model.eval()
dummy_input = torch.randn(1, 3, 224, 224)
torch.onnx.export(
    torch_model,
    dummy_input,
    "cherry_convnext_tiny.onnx",
    input_names=["input"],
    output_names=["output"],
    opset_version=13
)

# 2. Python 端验证（必须验证输入输出节点名称与 C++ 解析一致）
session = ort.InferenceSession("cherry_convnext_tiny.onnx")
input_name = session.get_inputs()[0].name   # 期望 "input"
output_name = session.get_outputs()[0].name  # 期望 "output"
print(f"Input: {input_name}, Output: {output_name}")

# 3. 导出产物
# cherry_convnext_tiny.onnx  (体积约 110MB)
```

---

## 6. 硬件规格 (Hardware Specifications)

### 6.1 云服务器配置（推理专用）

| 项目 | 规格 |
|------|------|
| 部署平台 | 云服务器（推理专用） |
| 后端语言 | C++ |
| CPU 核心数 | 2 核 |
| 物理内存 | 2 GB |
| 虚拟内存 | 8 GB（Swap） |
| 磁盘（映射） | 6 GB |
| 推理框架 | ONNX Runtime C++ |
| 图像处理库 | stb_image（轻量内存级解码，禁止使用 OpenCV） |

> **说明**：该云服务器仅承担推理任务（加载 .onnx 模型对上传图片进行 AI 品质评估），不参与模型训练。

### 6.2 模型训练工作站配置

| 项目 | 规格 |
|------|------|
| 用途 | 模型训练（不参与推理） |
| 显卡 | NVIDIA RTX 5090 |
| CPU | AMD R9-9950X3D |
| 操作系统 | Linux / Windows（按需选择） |
| 训练框架 | PyTorch |
| 输出产物 | `.onnx` 模型文件 |

> **说明**：模型训练完成后，将生成的 `.onnx` 文件部署至云服务器，由 C++ 后端调用 ONNX Runtime 进行推理。

### 6.3 FPGA 端摄像头 (OV5640)

| 项目 | 规格 |
|------|------|
| 摄像头型号 | OV5640 |
| 通信接口 | DVP (Digital Video Port) / SCCB (兼容 I2C) |
| 自动对焦 | 支持（需配置 AF 固件） |
| 输出格式 | JPEG / RGB565 / YUV422 |
| **本项目配置分辨率** | **VGA 640×480（固定）** |

> **说明**：本项目固定使用 VGA 640×480 输出。FPGA 端通过 SCCB 寄存器配置选择该分辨率。云端 C++ 后端负责 Center Crop 480×480 → Resize 224×224 的处理流程。

**OV5640 支持分辨率参考：**

| 分辨率 | 像素 | 帧率 |
|--------|------|------|
| 2592 × 1944 | 500 万 | 15 FPS |
| 1920 × 1080 | 1080P | 30 FPS |
| 1280 × 720 | 720P | 60 FPS |
| 640 × 480 | VGA | 60 FPS |

---

## 6.1 FPGA 端模块架构详细设计

本节补充 FPGA 侧的详细实现说明，作为对系统架构文档第二章"硬件间通信协议"的底层技术支撑。

### 6.1.1 模块架构总览

本系统代码结构采用**自顶向下 (Top-Down)** 的模块化设计，顶层模块（Top Module）负责实例化以下 6 个子模块，并完成它们之间的数据流互联：

| 序号 | 模块名称 | 功能描述 |
|------|----------|----------|
| 1 | `uart_cmd_parser` | 指令接收与解析模块 |
| 2 | `buzzer_pwm_ctrl` | 蜂鸣器提示音控制模块 |
| 3 | `rtc_display_74hc595` | 时间换算与数码管串行驱动模块 |
| 4 | `ov5640_jpeg_capture` | 摄像头配置与 JPEG 帧捕获模块 |
| 5 | `sdram_frame_buffer` | 图像跨时钟域 SDRAM 缓存模块 |
| 6 | `spi_master_tx` | 自定义协议打包与 SPI 发送模块 |

> **注**：ESP32 侧固件作为独立工程开发（详见 6.2 节）。

### 6.1.2 模块 1：指令接收与解析模块 (`uart_cmd_parser`)

**功能描述**：通过串口接收主控发来的触发字符串，解析出鉴权信息与时间戳，并生成全局触发信号。

**输入/输出信号**：
- `Input`: `sys_clk` (50MHz), `sys_rst_n`, `uart_rx` (PIN_N6)
- `Output`: `cmd_valid` (指令有效脉冲), `device_id`, `token`, `timestamp`

**实现逻辑 (FSM 状态机)**：

| 状态 | 动作 |
|------|------|
| **IDLE** | 等待 UART 接收完成标志位。当匹配到字符 `C`、`A`、`M`、`|` 序列时，进入数据提取状态 |
| **EXTRACT_ID** | 将接收到的字符存入 `device_id` 移位寄存器，遇到 `|` 切换状态 |
| **EXTRACT_TOKEN** | 将字符存入 `token` 移位寄存器，遇到 `|` 切换状态 |
| **EXTRACT_TIME** | 将时间戳 ASCII 码转为 HEX 数字存入 `timestamp`（如将 "1712800000" 转为 32-bit 整数） |
| **DONE** | 遇到 `\n` (0x0A)，校验数据完整性，拉高 `cmd_valid` 一个时钟周期，随后返回 IDLE |

### 6.1.3 模块 2：蜂鸣器提示音控制模块 (`buzzer_pwm_ctrl`)

**功能描述**：接收到指令有效信号后，驱动开发板上的**无源蜂鸣器**发出 2 秒的清脆提示音。

**输入/输出信号**：
- `Input`: `sys_clk` (50MHz), `sys_rst_n`, `cmd_valid` (触发信号)
- `Output`: `beep` (PIN_J11, PWM 输出)

**实现逻辑**：
- **2 秒定时器**：检测到 `cmd_valid` 脉冲时，启动一个计数器，目标值为 `100,000,000`（50MHz 下为 2 秒）。
- **PWM 生成**：在计数器运行的这 2 秒内，由另一个子计数器生成频率约 **3kHz**（计数周期约 16666）、占空比 **50%** 的方波，赋值给 `beep` 引脚。
- 2 秒到达后，计数器清零，`beep` 强制拉低（静音）。

### 6.1.4 模块 3：时间换算与数码管串行驱动模块 (`rtc_display_74hc595`)

**功能描述**：将 32-bit 的 UNIX 时间戳转换为北京时间的 HH.MM.SS，并通过 74HC595 移位寄存器点亮 6 位数码管。

**实现逻辑**：
1. **时区与算术模块**：
   - `Timestamp_BJ = Timestamp + 28800`（UTC+8 北京时间）
   - `Current_Seconds = Timestamp_BJ % 86400`（当天走过的秒数）
   - 使用分步除法/状态机，算出 `Hours` (/ 3600), `Minutes` (/ 60 % 60), `Seconds` (% 60)，并拆分为 6 个十进制个位数
2. **74HC595 串行移位扫描 (1kHz 定时)**：
   - 准备一个 16-bit 寄存器：`[8位段选码 (数字字模)] + [8位位选码 (选择哪一位数码管)]`
   - **Shift 状态**：拉低 `shcp`，将最高位推入 `ds`，拉高 `shcp`，循环 16 次
   - **Latch 状态**：给 `stcp` 一个高脉冲，将 16 位数据锁存输出
   - **使能**：维持 `oe` 为低电平。每隔 1ms 切换下一位数码管的数据

### 6.1.5 模块 4：摄像头配置与 JPEG 帧捕获模块 (`ov5640_jpeg_capture`)

**功能描述**：通过 I2C/SCCB 初始化 OV5640 摄像头输出 JPEG 流，并精准提取单帧图像的有效载荷。

**实现逻辑**：
1. **SCCB Master**：系统复位后，向 OV5640 的 I2C 地址 (写 0x78, 读 0x79) 依次发送初始化寄存器数组（配置为 640x480, JPEG 压缩格式）
2. **JPEG 边界检测**：
   - 由于 JPEG 每帧大小不一，无法依靠行列计数判断结束
   - 在 `href` 为高的 `pclk` 节拍下，实时监控 `data[7:0]` 流
   - **帧起始**：检测到连续两字节为 `0xFF` 紧接着 `0xD8`，标记帧开始
   - **帧结束**：检测到连续两字节为 `0xFF` 紧接着 `0xD9`，标记一帧结束
3. 输出 `jpeg_data_en` 和 `jpeg_data` 字节流，并统计出当前帧的**总字节数** `file_size`

### 6.1.6 模块 5：图像跨时钟域 SDRAM 缓存模块 (`sdram_frame_buffer`)

**功能描述**：解决摄像头输出速度（PCLK）与 SPI 传输速度（SCK）不匹配的问题。

**实现逻辑**：
- **写端口 (Camera 端)**：将模块 4 输出的 JPEG 字节流打包成 16-bit，存入一个写 FIFO。FIFO 水线到达阈值时，向 SDRAM 发起 Burst Write 请求，存入 SDRAM 指定 Bank
- **读端口 (SPI 端)**：当检测到模块 6 请求发送数据时，从 SDRAM 发起 Burst Read 请求，存入读 FIFO，供 SPI 发送端平滑读取

### 6.1.7 模块 6：自定义协议打包与 SPI 发送模块 (`spi_master_tx`)

**功能描述**：将鉴权数据和 SDRAM 中的图片数据组装成数据包，通过 SPI 发送给 ESP32。

**实现逻辑**：
1. 检测到完整的一帧 JPEG 已写入 SDRAM 且接收到触发指令后，拉低 `spi_cs_n`
2. 按照严格顺序将数据 Shift Out 至 `spi_mosi` 引脚：
   - 发送 `0xAA 0xBB`（2 字节包头）
   - 发送 `Device_ID` 长度及数据
   - 发送 `Token` 长度及数据
   - 发送 `Timestamp` 长度及数据
   - 发送 `file_size`（4 字节大端格式）
   - 切换至 **图像流发送状态**，持续从 SDRAM 读 FIFO 中取数据并由 SPI 发送
3. 拉高 `spi_cs_n` 结束传输

### 6.1.8 FPGA 侧引脚分配表

在 Quartus 软件中请依次核对以下引脚：

| 模块分类 | 信号名称 | FPGA引脚 | I/O 标准 |
|----------|----------|----------|----------|
| **系统** | `sys_clk` (50MHz) | PIN_E1 | 2.5V |
| **系统** | `sys_rst_n` | PIN_M15 | 2.5V |
| **串口接收** | `uart_rx` | PIN_N6 | 2.5V |
| **蜂鸣器** | `beep` (PWM) | PIN_J11 | 2.5V |
| **数码管(595)** | `ds` | PIN_R1 | 2.5V |
| **数码管(595)** | `oe` | PIN_L11 | 2.5V |
| **数码管(595)** | `shcp` | PIN_B1 | 2.5V |
| **数码管(595)** | `stcp` | PIN_K9 | 2.5V |
| **SPI 通信** | `spi_sck` | PIN_A10 | 2.5V |
| **SPI 通信** | `spi_mosi` | PIN_B10 | 2.5V |
| **SPI 通信** | `spi_miso` | PIN_A9 | 2.5V |
| **SPI 通信** | `spi_cs_n` | PIN_B9 | 2.5V |
| **摄像头时钟** | `cam_24mhz` | PIN_D15 | 2.5V |
| **其他摄像头/SDRAM** | (参考之前约束) | 保持原样 | 2.5V |

---

## 6.2 副控 ESP32-C3 固件详细设计

### 6.2.1 SPI 引脚定义

> **重要说明**：以下为推荐的 ESP32-C3 侧 SPI 引脚配置。

**推荐方案（HSPI 接口）**：

| 功能 | ESP32-C3 GPIO | 说明 |
|------|---------------|------|
| **SPI 时钟 (SCK)** | GPIO2 | HSPI 专用时钟，可直接连接 FPGA |
| **SPI MOSI (主收从发)** | GPIO3 | 从 FPGA 接收数据 |
| **SPI MISO (主发从收)** | GPIO10 | 固定为输入 |
| **SPI 片选 (CS)** | GPIO6 | 接受 FPGA 控制 |

**引脚选择理由**：

| 对比项 | VSPI (GPIO0/1/9/10) | HSPI (GPIO2/3/6/10) |
|--------|---------------------|---------------------|
| GPIO0/1 | 通常用于烧录/启动模式，容易冲突 | GPIO2/3 是普通 GPIO，灵活性高 |
| 推荐度 | ⚠️ 谨慎使用 | ✅ **推荐使用** |

**完整 SPI 连接对照表**：

| 功能 | FPGA 侧 | ESP32-C3 侧 |
|------|---------|-------------|
| SPI 时钟 | PIN_A10 | GPIO2 (HSPI SCK) |
| SPI MOSI | PIN_B10 | GPIO3 (HSPI MOSI) |
| SPI MISO | PIN_A9 | GPIO10 (HSPI MISO) |
| SPI 片选 | PIN_B9 | GPIO6 (HSPI CS) |
| 共地 | GND | GND |

### 6.2.2 固件核心工作流

**功能描述**：作为 SPI Slave 接收 FPGA 传来的拼装流，剥离数据并发送 HTTP 网络请求。

**核心工作流**：
1. **SPI DMA 接收**：由于涉及几十 KB 的图像数据，必须使用 ESP32 的 **SPI Slave DMA** 模式  
2. **协议状态机解析**：
   - 在内存 Buffer 中搜索 `0xAA 0xBB`
   - 按约定偏移量依次读取 `ID`、`Token`、`Timestamp` 字符串
   - 读取 `file_size` 变量
   - 将剩余的 `file_size` 长度的内容锁定为 **纯 JPEG Buffer**
3. **HTTP POST 构建**：

```c
esp_http_client_config_t config = {
    .url = "http://47.107.41.102:9000/api/device/vision",
    .method = HTTP_METHOD_POST,
};
esp_http_client_handle_t client = esp_http_client_init(&config);

// 动态注入 Header (利用之前提取的变量)
esp_http_client_set_header(client, "Device_ID", device_id_str);
esp_http_client_set_header(client, "Token", token_str);
esp_http_client_set_header(client, "Timestamp", timestamp_str);
esp_http_client_set_header(client, "Content-Type", "application/octet-stream");

// 挂载 JPEG 二进制流
esp_http_client_set_post_field(client, jpeg_buffer, file_size);

// 发送并清理
esp_err_t err = esp_http_client_perform(client);
```

---

## 7. 生产环境部署规范 (极度重要)

> 针对 2核2G 服务器的特殊体质，正式部署 FieldDataProcessingServer（C++ 推理后端）时，必须严格遵守以下规范。任何一步疏漏都可能导致服务器卡死或 OOM。

### 7.1 脱离开发环境（释放内存）

正式部署前，必须关闭云服务器上所有开发相关进程：

```bash
# 关闭 Cursor 远程 IDE 及 Node 守护进程
pkill -9 -f cursor-server
pkill -9 -f node

# 确认进程已终止
ps aux | grep -E "cursor|node" | grep -v grep
```

> **预计释放内存**：~500MB ~ 600MB，将可用物理内存从 ~100MB 提升至 ~600MB。

### 7.2 清理虚拟内存

使用 XShell（或 SSH）登录后，执行以下命令清空积压的 Swap：

```bash
sudo swapoff -a && sudo swapon -a
```

> **说明**：2G 物理内存的服务器长期运行后，Swap 可能积压 1.6GB 以上的数据。清空后可使物理内存处于最佳状态。

### 7.3 内存与性能预期

| 指标 | 预期值 |
|------|--------|
| ConvNeXt-Tiny ONNX 模型内存占用 | ~200MB（Session 加载后） |
| 单张图片推理时内存峰值 | ~250MB（含图像缓冲） |
| 单张图片推理耗时 | 0.2s ~ 0.5s |
| 系统周期吞吐 | 完全满足 5 分钟/次的采集频率 |
| 服务器状态 | 永不触发 OOM，永不卡死 |

### 7.4 部署检查清单

- [ ] 确认 Cursor 远程 IDE 及所有 node 进程已终止
- [ ] 确认 Swap 已清空（`free -h` 确认 available 充足）
- [ ] 确认 `cherry_convnext_tiny.onnx` 已部署到服务器指定路径
- [ ] 确认 `fruit_vision_data` 表已创建
- [ ] 确认 `FieldDataProcessingServer` 已编译为 Release 版本
- [ ] 使用 `nohup ./server &` 后台启动，并通过 `top` 或 `htop` 确认进程内存占用正常
- [ ] 测试 `POST /api/device/vision` 接口，确认图片能正常上传和推理

---


