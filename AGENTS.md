# 樱桃冷链品质检测系统 (CherryColdChain)

> 本文档供 AI 助手快速了解项目结构和技术细节

---

## 一、项目概述

解决冷链运输场景下樱桃品质监控问题，通过多光谱传感器与气体传感器融合，实现樱桃糖度与腐败程度的精准无损检测。

**核心功能**：
- 糖度检测（AS7341 多光谱 + MLR 算法，精度 ±0.3 Brix）
- 腐败度检测（MQ-135/MQ-137 气体融合，0~100%）
- 环境监控（温湿度、光照）
- AI 视觉品质分级（ConvNeXt-Tiny ONNX 推理）
- 无人机应急响应（快腐败预警广播）

**系统架构**：

```
┌─────────────────────────────────────────────────────────────────────┐
│                         感知层                                       │
│  ESP32(主控) ←──UART── Arduino UNO(气体)                           │
│  ├── AS7341 光谱 (I2C)  ──► 糖度计算                               │
│  ├── SHT31 温湿度 (I2C)                                             │
│  ├── BH1750 光照 (I2C)                                              │
│  ├── DS3231 RTC (I2C)                                               │
│  ├── LVGL 触摸屏 (SPI)                                              │
│  └── 气体传感器 ←──Serial2── Arduino UNO                           │
│                           ├── MQ-135 苯类气体                       │
│                           └── MQ-137 乙醇气体                       │
│                               └── OLED 显示                         │
└─────────────────────────────────────────────────────────────────────┘
                               │
                               │ HTTP POST
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         服务层 (C++)                                 │
│  端口: 9000                                                         │
│  ├── /api/device/upload    设备数据上传                            │
│  ├── /api/device/vision   图片上传 + ONNX 推理                    │
│  ├── /api/admin/*         管理员接口                              │
│  └── /api/drone/*         无人机接口                              │
└─────────────────────────────────────────────────────────────────────┘
                               │
                               │ HTTP
                               ▼
┌─────────────────────────────────────────────────────────────────────┐
│                         应用层 (Web)                                 │
│  樱桃深红主题 UI + ECharts 监控大屏                                │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 二、目录结构

```
CherryColdChain/
├── FieldAcquisitionTerminal/      # ESP32 设备端 (PlatformIO)
│   ├── config.h                  # WiFi/服务器/引脚配置
│   ├── platformio.ini            # 库依赖
│   └── src/
│       ├── main.cpp              # 入口、状态机
│       ├── algorithm/sugar_calc.h       # 糖度计算 (MLR)
│       ├── display/lvgl_ui.cpp          # 触摸屏 UI
│       ├── network/HttpClient.cpp       # HTTP 上传
│       ├── sensor_driver/SensorManager.cpp  # 传感器驱动
│       └── storage/StorageManager.cpp   # LittleFS 离线存储
│
├── FieldDataProcessingServer/      # C++ 后端 (CMake)
│   ├── CMakeLists.txt
│   ├── config.ini               # 数据库配置
│   ├── models/                  # ONNX 模型
│   ├── sql/init_db.sql          # 数据库初始化
│   └── src/
│       ├── main.cpp
│       ├── auth/                # 认证模块
│       ├── data_process/         # 算法计算
│       │   ├── SugarCalc.cpp    # 糖度 MLR
│       │   ├── SpoilCalc.cpp   # 腐败度融合
│       │   └── MaturityCalc.cpp # 成熟度
│       ├── db/MySQLDriver.cpp   # 数据库驱动
│       ├── http_server/         # HTTP 服务
│       ├── vision/              # ONNX 推理
│       └── drone/FastDecayQueue.cpp  # 快腐败队列
│
├── FieldMonitoringPlatform/      # Web 前端
│   ├── index.html              # 产品介绍
│   ├── admin/
│   │   ├── login.html          # 登录页
│   │   ├── dashboard.html      # 监控大屏
│   │   ├── css/style.css
│   │   └── js/
│   │       ├── api.js          # API 封装
│   │       ├── chart.js        # ECharts 图表
│   │       └── auto_switch.js  # 自动刷新
│   └── assets/
│       ├── libs/echarts.min.js
│       └── uploads/            # 设备图片
│
└── docs/
    ├── 系统开发者总文档.md     # 完整技术文档
    ├── 无人机模块开发者文档.md
    ├── 多模态AI视觉扩展模块.md
    └── 设备端引脚占用.md
```

---

## 三、技术栈速查

| 模块 | 技术栈 |
|------|--------|
| **设备端** | ESP32 + Arduino, PlatformIO, LVGL, AS7341, SHT31, BH1750, DS3231 |
| **后端** | C++17, cpp-httplib, MySQL, libsodium (Argon2), ONNX Runtime |
| **前端** | HTML5/CSS3/ES6+, ECharts v5, Cookie |
| **AI** | ConvNeXt-Tiny, ONNX, PyTorch |
| **数据库** | MySQL (FruitDataBase) |
| **服务器** | 47.107.41.102:9000 |

---

## 四、数据库表结构

```sql
-- 5 张核心表
admin_users       -- 管理员 (username, password_hash Argon2)
device_auth       -- 设备认证 (device_id, token, status)
field_production  -- 产区信息 (field_id, 品种, 成熟糖度阈值)
fruit_data        -- 樱桃数据 (device_id, collected_at, sugar_brix, maturity_score)
field_environment -- 环境数据 (field_id, collected_at, temp, humidity, gas135_ppm, gas137_ppm)
fruit_vision_data -- 视觉数据 (device_id, collected_at, quality_level, image_url)
```

**关键数据**：
- 设备编号格式：`产区号-组号-设备号` (例: `1001-01-01`)
- 产区号 = 设备编号前 4 位

---

## 五、核心算法

### 5.1 糖度计算 (Lambert-Beer MLR)

```cpp
// 吸光度计算
abs_555 = log10(chClear / (ch555 + 1.0))
abs_640 = log10(chClear / (ch640 + 1.0))
abs_680 = log10(chClear / (ch680 + 1.0))
abs_NIR = log10(chClear / (chNIR + 1.0))

// MLR 回归
raw_brix = -1.2×abs_555 + 3.5×abs_640 + 6.8×abs_680 + 12.5×abs_NIR - 2.5
sugar_brix = clamp(raw_brix, 0.0, 20.0)

// 安全门卫: chClear < 200 或 chNIR < 10 → 返回 0.0
```

### 5.2 腐败度计算 (双气体融合)

```cpp
// 容错: 两路气体均为 99.0 → 返回 0.0
if (gas135 == 99.0 && gas137 == 99.0) return 0.0;

// 风险归一化
risk135 = clamp((gas135 - 100.0) / 200.0, 0.0, 1.0)  // 100~300ppm
risk137 = clamp((gas137 - 120.0) / 230.0, 0.0, 1.0)  // 120~350ppm

// 加权融合 (乙醇权重更高)
spoil_score = risk135 × 0.4 + risk137 × 0.6
spoilage = spoil_score × 100.0

// 预警阈值: spoilage > 60% → 快腐败告警
```

### 5.3 成熟度计算

```cpp
maturity_score = sugar_brix / mature_sugar_threshold
// 产区阈值: Bing Cherry=15.0, Rainier=16.5, Brooks=14.5
```

---

## 六、关键接口速查

### 6.1 设备数据上传
```
POST /api/device/upload
Headers: device_id, token
Body: { spectrum_json, gas135, gas137, temperature, humidity, light, collected_at }
Response: { code, sugar_brix, maturity_score, server_time }
```

### 6.2 设备时间同步
```
GET /api/device/time
Response: { code, server_time }
```

### 6.3 管理员登录
```
POST /api/admin/login
Body: { username, password }
Response: Set-Cookie: session_id (HttpOnly, 1小时)
```

### 6.4 产区数据查询
```
GET /api/admin/fruit/history?device_id=1001-01-01
GET /api/admin/field/environment?field_id=1001
```

### 6.5 无人机快腐败队列
```
GET /api/drone/fast-decay/queue
Headers: device_id, token
Response: { hasFastDecayCherries, cherries: [{deviceId, timestamp, decayRate}] }
```

---

## 七、ESP32 配置

**文件**: `FieldAcquisitionTerminal/src/config.h`

```cpp
#define DEVICE_ID       "1001-01-01"
#define FRUIT_VARIETY   "Cherry"
#define DEVICE_TOKEN    "device-token-001"
#define WIFI_SSID       "YourSSID"
#define WIFI_PASS       "YourPassword"
#define SERVER_URL      "http://47.107.41.102:9000/api/device/upload"
#define AUTO_UPLOAD_INTERVAL 300000  // 5分钟
```

**引脚配置**：
- I2C 总线: GPIO27 (SDA), GPIO22 (SCL)
- 气体串口: GPIO35 (RX), GPIO21 (TX), 9600bps
- SPI TFT: GPIO5 (CS), GPIO21 (DC), GPIO4 (RST), GPIO18 (CLK)
- 触摸: GPIO33 (CS), GPIO25 (CLK), GPIO32 (MOSI), GPIO39 (MISO)

---

## 八、部署命令

### 8.1 编译 ESP32
```bash
cd FieldAcquisitionTerminal
pio run --target upload      # 烧录
pio run --target monitor      # 串口监视 (115200)
```

### 8.2 编译后端
```bash
cd FieldDataProcessingServer
mkdir build && cd build
cmake .. && make -j4
./FieldDataProcessingServer
# 或后台: nohup ./FieldDataProcessingServer > server.log 2>&1 &
```

### 8.3 数据库初始化
```bash
mysql -u root -pMhr289839. < sql/init_db.sql
```

---

## 九、访问凭证

| 系统 | 用户名 | 密码 |
|------|--------|------|
| 云服务器 SSH | root | Mhr289839. |
| MySQL (root) | root | Mhr289839. |
| MySQL (应用) | sws_user | Mhr289839. |
| Web 管理后台 | admin | admin123 |

---

## 十、常见开发任务

### 新增产区
```sql
INSERT INTO field_production (field_id, fruit_variety, mature_sugar_threshold) 
VALUES ('1004', 'Sweet Cherry', 15.50);

INSERT INTO device_auth (device_id, token, status) 
VALUES ('1004-01-01', 'device-token-004', 1);
```

### 新增设备
```cpp
// 修改 config.h
#define DEVICE_ID "1004-01-01"
#define DEVICE_TOKEN "device-token-004"
```

### 修改糖度算法系数
```cpp
// ESP32 端: FieldAcquisitionTerminal/src/algorithm/sugar_calc.h
// 服务器端: FieldDataProcessingServer/src/data_process/SugarCalc.cpp
// 两处必须同步修改！
```

### 调整腐败预警阈值
```cpp
// 搜索 spoilage > 60 或 SPOIL_THRESHOLD
// 前端: FieldMonitoringPlatform/admin/js/auto_switch.js
// 后端: FieldDataProcessingServer/src/drone/FastDecayQueue.cpp
```

### 部署 ONNX 模型
```bash
# 将模型放入
FieldDataProcessingServer/models/cherry_convnext_tiny.onnx
# 重启服务器即可自动加载
```

---

## 十一、故障排查

| 症状 | 排查方向 |
|------|----------|
| 糖度显示 0 | AS7341 未初始化, chClear<200 或 chNIR<10 |
| 气体数据 99 | Arduino 未连接, 串口波特率不匹配 (9600) |
| WiFi 连接失败 | config.h SSID/密码错误 |
| 数据上传 401 | device_auth 表 token 不匹配 |
| ONNX 返回 -1 | 模型文件不存在或加载失败 |
| 前端无数据 | Session 过期, 重新登录 |
| 时间显示 1970 | DS3231 未连接或电池没电 |

**调试命令**:
```bash
# 测试服务器连通性
curl http://47.107.41.102:9000/ping

# 查看服务器日志
tail -f server.log

# 数据库连接测试
mysql -u sws_user -pMhr289839. -e "SELECT COUNT(*) FROM fruit_data;" FruitDataBase
```

---

## 十二、快速定位代码

| 功能 | 文件路径 |
|------|----------|
| 糖度计算 (ESP32) | `src/algorithm/sugar_calc.h` |
| 糖度计算 (服务器) | `src/data_process/SugarCalc.cpp` |
| 腐败度计算 | `src/data_process/SpilCalc.cpp` |
| 设备认证 | `src/auth/DeviceAuth.cpp` |
| HTTP 路由 | `src/http_server/Router.cpp` |
| ONNX 推理 | `src/vision/ONNXInference.cpp` |
| 快腐败队列 | `src/drone/FastDecayQueue.cpp` |
| 前端图表 | `admin/js/chart.js` |
| 前端刷新逻辑 | `admin/js/auto_switch.js` |

---

## 十三、关键常量速查

| 常量 | 值 | 说明 |
|------|-----|------|
| 糖度范围 | 0~20 Brix | 超出钳制 |
| 腐败度预警 | >60% | 红色告警 |
| 离线告警 | 30分钟 | 卡片变灰 |
| 队列过期 | 24小时 | 自动清除 |
| Session TTL | 1小时 | HttpOnly Cookie |
| 历史数据 | 84天 | 前端查询限制 |
| 自动刷新 | 6秒 | 前端轮询 |

---

*文档基于 v4.1 版本，最后更新: 2026-05-06*
