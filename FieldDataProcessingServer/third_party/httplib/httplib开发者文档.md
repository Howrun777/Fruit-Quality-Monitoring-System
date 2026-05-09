# cpp-httplib：服务器核心接口（v0.14.x）

# cpp-httplib 开发者文档（v0.14.x）

针对你的 **智能西瓜无损糖度检测系统** 定制，重点讲解服务器端核心接口，附带项目场景示例。

---

## 目录

1. [快速集成](sslocal://flow/file_open?url=%231-快速集成&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=)

2. [核心类总览](sslocal://flow/file_open?url=%232-核心类总览&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=)

3. [Server 类（HTTP 服务器核心）](sslocal://flow/file_open?url=%233-server-类http-服务器核心&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=)

4. [Request 类（请求对象）](sslocal://flow/file_open?url=%234-request-类请求对象&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=)

5. [Response 类（响应对象）](sslocal://flow/file_open?url=%235-response-类响应对象&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=)

6. [项目场景实战示例](sslocal://flow/file_open?url=%236-项目场景实战示例&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=)

7. [常见问题与最佳实践](sslocal://flow/file_open?url=%237-常见问题与最佳实践&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=)

---

## 1. 快速集成

### 1.1 获取库

cpp-httplib 是 **单头文件（Header-only）** 库，无需编译安装：

1. 访问 [GitHub 仓库](sslocal://flow/file_open?url=https%3A%2F%2Fgithub.com%2Fyhirose%2Fcpp-httplib&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=)

2. 下载 `httplib.h` 文件

3. 放入你的项目 `third_party/httplib/` 目录

### 1.2 CMakeLists.txt 配置

```CMake

cmake_minimum_required(VERSION 3.10)
project(SmartWatermelonServer)

# 设置 C++ 标准（推荐 C++17）
set(CMAKE_CXX_STANDARD 17)

# 添加头文件搜索路径
include_directories(${PROJECT_SOURCE_DIR}/third_party/httplib)

# 查找线程库（必须）
find_package(Threads REQUIRED)

# 生成可执行文件并链接
add_executable(sws_server src/main.cpp)
target_link_libraries(sws_server PRIVATE Threads::Threads)
```

### 1.3 最小化服务器示例

```C++

#include "httplib.h"

int main() {
    // 1. 创建服务器实例
    httplib::Server svr;

    // 2. 注册路由：GET /ping
    svr.Get("/ping", [](sslocal://flow/file_open?url=const+httplib%3A%3ARequest%26+req%2C+httplib%3A%3AResponse%26+res&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=) {
        res.set_content(R"({"msg": "pong"})", "application/json");
    });

    // 3. 启动服务（监听 0.0.0.0:8080）
    svr.listen("0.0.0.0", 8080);
    return 0;
}
```

---

## 2. 核心类总览

|类名|作用|你的项目场景|
|---|---|---|
|`httplib::Server`|HTTP 服务器核心，负责监听端口、注册路由、处理请求|托管你的后端接口（设备上传、糖度查询等）|
|`httplib::Request`|封装客户端发来的请求（参数、Body、文件等）|接收 ESP32 发来的光谱数据、管理员登录参数|
|`httplib::Response`|封装返回给客户端的响应（内容、状态码、Header 等）|返回糖度计算结果、JSON 格式的历史数据|
---

## 3. Server 类（HTTP 服务器核心）

### 3.1 启动与停止

|方法|说明|示例|
|---|---|---|
|`bool listen(const char* host, int port)`|启动服务（阻塞式）|`svr.listen("0.0.0.0", 8080);`|
|`void stop()`|停止服务（需在另一个线程调用）|`svr.stop();`|
|`bool is_running() const`|检查服务是否在运行|`if (svr.is_running()) { ... }`|
### 3.2 路由注册（核心！）

支持 `GET`/`POST`/`PUT`/`DELETE`/`PATCH` 等 HTTP 方法，语法统一：

```C++

svr.Get("/路径", [](sslocal://flow/file_open?url=const+Request%26+req%2C+Response%26+res&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=) { /* 处理逻辑 */ });
svr.Post("/路径", [](sslocal://flow/file_open?url=const+Request%26+req%2C+Response%26+res&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=) { /* 处理逻辑 */ });
```

#### 路径参数（Pattern Matching）

支持提取 URL 中的动态参数：

```C++

// 匹配 /api/watermelon/123，id=123
svr.Get(R"(/api/watermelon/(\d+))", [](sslocal://flow/file_open?url=const+Request%26+req%2C+Response%26+res&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=) {
    std::string watermelon_id = req.matches[1]; // 提取第一个分组
    res.set_content("查询西瓜ID: " + watermelon_id, "text/plain");
});
```

### 3.3 静态文件服务

一行代码托管前端 HTML/CSS/JS/图片：

```C++

// 将 ./static 目录挂载到根路径 /
// 访问 http://your-server/index.html → 对应 ./static/index.html
svr.set_mount_point("/", "./static");

// 也可以挂载到子路径
svr.set_mount_point("/web", "./frontend/dist");
```

### 3.4 全局配置

|配置项|说明|示例|
|---|---|---|
|`set_default_headers(Headers headers)`|设置全局默认响应头（跨域必备）|`svr.set_default_headers({{"Access-Control-Allow-Origin", "*"}});`|
|`set_logger(Logger logger)`|设置请求日志回调|见下方示例|
|`set_error_handler(ErrorHandler handler)`|设置自定义错误页面（404/500）|见下方示例|
#### 日志配置示例

```C++

svr.set_logger([](sslocal://flow/file_open?url=const+auto%26+req%2C+const+auto%26+res&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=) {
    std::cout << "[HTTP] " << req.method << " " << req.path 
              << " | 状态码: " << res.status << std::endl;
});
```

#### 自定义 404 页面示例

```C++

svr.set_error_handler([](sslocal://flow/file_open?url=const+auto%26+req%2C+auto%26+res&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=) {
    res.set_content(R"({"code": 404, "msg": "接口不存在"})", "application/json");
});
```

---

## 4. Request 类（请求对象）

路由处理函数的第一个参数，封装了客户端发来的所有数据。

### 4.1 核心成员变量

|成员|类型|说明|你的项目场景|
|---|---|---|---|
|`method`|`std::string`|请求方法（`"GET"`/`"POST"` 等）|判断是查询还是上传|
|`path`|`std::string`|请求路径（如 `/api/device/upload`）|路由匹配|
|`params`|`Params`（`std::multimap`）|URL 查询参数（`?id=123&name=xx`）|分页查询参数|
|`headers`|`Headers`（`std::multimap`）|请求头（`Authorization`/`Content-Type` 等）|设备 Token 认证|
|`body`|`std::string`|请求 Body（JSON 数据等）|ESP32 发来的光谱 JSON|
|`files`|`MultipartFormData`|上传的文件（`multipart/form-data` 格式）|上传光谱 CSV 文件|
|`matches`|`std::vector<std::string>`|路径参数匹配结果|提取西瓜 ID|
### 4.2 常用方法

|方法|说明|示例|
|---|---|---|
|`bool has_param(const std::string& key) const`|检查是否有指定 URL 参数|`if (req.has_param("watermelon_id")) { ... }`|
|`std::string get_param_value(const std::string& key) const`|获取 URL 参数值|`std::string id = req.get_param_value("watermelon_id");`|
|`bool has_header(const std::string& key) const`|检查是否有指定请求头|`if (req.has_header("Device-Token")) { ... }`|
|`std::string get_header_value(const std::string& key) const`|获取请求头值|`std::string token = req.get_header_value("Device-Token");`|
|`bool has_file(const std::string& key) const`|检查是否有指定上传文件|`if (req.has_file("spectrum_file")) { ... }`|
|`MultipartFormData get_file_value(const std::string& key) const`|获取上传文件|见下方示例|
### 4.3 文件上传处理示例

```C++

svr.Post("/api/device/upload_file", [](sslocal://flow/file_open?url=const+Request%26+req%2C+Response%26+res&flow_extra=eyJsaW5rX3R5cGUiOiJjb2RlX2ludGVycHJldGVyIn0=) {
    // 1. 验证设备 Token
    if (!req.has_header("Device-Token")) {
        res.status = 401;
        res.set_content(R"({"code": 401, "msg": "缺少Token"})", "application/json");
        return;
    }

    // 2. 获取上传的文件（字段名 spectrum_file）
    auto file = req.get_file_value("spectrum_file");
    // file.filename: 文件名（如 spectrum_20260410.csv）
    // file.content: 文件内容（二进制字符串）
    // file.content_type: 文件类型（如 text/csv）

    // 3. 保存文件到本地
    std::ofstream ofs("./upload/" + file.filename, std::ios::binary);
    ofs << file.content;
    ofs.close();

    res.set_content(R"({"code": 200, "msg": "文件上传成功"})", "application/json");
});
```

---

## 5. Response 类（响应对象）

路由处理函数的第二个参数，用于设置返回给客户端的内容。

### 5.1 核心成员变量

|成员|类型|说明|示例|
|---|---|---|---|
|`status`|`int`|HTTP 状态码|`res.status = 200;`（成功）/ `res.status = 404;`（未找到）|
|`body`|`std::string`|响应内容|`res.body = R"({"sugar": 12.5})";`|
|`headers`|`Headers`|响应头|见下方示例|
### 5.2 常用方法

|方法|说明|示例|
|---|---|---|
|`void set_content(const std::string& s, const std::string& content_type)`|设置响应内容和类型|`res.set_content(json_str, "application/json");`|
|`void set_header(const std::string& key, const std::string& val)`|设置响应头|`res.set_header("Access-Control-Allow-Origin", "*");`|
|`void set_redirect(const std::string& url, int status = 302)`|重定向|`res.set_redirect("/login");`|
### 5.3 常见 Content-Type

|Content-Type|说明|你的项目场景|
|---|---|---|
|`application/json`|JSON 数据|返回糖度结果、历史数据|
|`text/plain`|纯文本|调试用|
|`text/html`|HTML 页面|托管前端|
|`application/octet-stream`|二进制流|下载文件|
---

## 6. 项目场景实战示例

结合你的 **智能西瓜无损糖度检测系统**，给出 3 个核心接口的完整实现。

### 前置准备：引入 JSON 库

推荐配合 `nlohmann/json`（也是单头文件库）使用，处理 JSON 更方便：

```C++

#include "httplib.h"
#include "nlohmann/json.hpp"
using json = nlohmann::json;
```

### 示例1：设备上传光谱数据（POST）

```C++

svr.Post("/api/device/upload", [](const Request& req, Response& res) {
    // 1. 验证设备 Token
    if (!req.has_header("Device-Token")) {
        res.status = 401;
        json result;
        result["code"] = 401;
        result["msg"] = "缺少设备 Token";
        res.set_content(result.dump(), "application/json");
        return;
    }
    std::string device_token = req.get_header_value("Device-Token");
    // TODO: 调用你的 DeviceAuth::checkToken(device_token) 验证

    // 2. 解析 JSON Body
    try {
        json data = json::parse(req.body);
        std::string device_id = data["device_id"];
        std::vector<double> spectrum = data["spectrum"].get<std::vector<double>>();

        // 3. 调用糖度计算逻辑
        // TODO: double sugar = SugarCalc::calculate(spectrum);
        double mock_sugar = 12.5;

        // 4. 存入数据库
        // TODO: MySQLDriver::getInstance()->insertData(device_id, spectrum, mock_sugar);

        // 5. 返回结果
        json result;
        result["code"] = 200;
        result["msg"] = "数据上传成功";
        result["data"]["sugar_brix"] = mock_sugar;
        res.set_content(result.dump(), "application/json");
    } catch (const json::parse_error& e) {
        res.status = 400;
        json result;
        result["code"] = 400;
        result["msg"] = "JSON 解析失败";
        res.set_content(result.dump(), "application/json");
    }
});
```

### 示例2：查询西瓜糖度历史（GET）

```C++

svr.Get("/api/watermelon/history", [](const Request& req, Response& res) {
    // 1. 获取 URL 参数
    if (!req.has_param("watermelon_id")) {
        res.status = 400;
        json result;
        result["code"] = 400;
        result["msg"] = "缺少 watermelon_id 参数";
        res.set_content(result.dump(), "application/json");
        return;
    }
    std::string watermelon_id = req.get_param_value("watermelon_id");

    // 2. 查询数据库
    // TODO: auto history = MySQLDriver::getInstance()->queryHistory(watermelon_id);
    std::vector<double> mock_history = {12.1, 12.3, 12.5, 12.4};

    // 3. 返回结果
    json result;
    result["code"] = 200;
    result["data"]["watermelon_id"] = watermelon_id;
    result["data"]["sugar_history"] = mock_history;
    res.set_content(result.dump(), "application/json");
});
```

### 示例3：全局跨域配置（Web 前端必备）

```C++

// 在注册路由之前配置
svr.set_default_headers({
    {"Access-Control-Allow-Origin", "*"}, // 允许所有来源，生产环境建议换成具体域名
    {"Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS"},
    {"Access-Control-Allow-Headers", "Content-Type, Device-Token, Authorization"}
});

// 处理 OPTIONS 预检请求
svr.Options(".*", [](const Request& req, Response& res) {
    res.status = 200;
});
```

---

## 7. 常见问题与最佳实践

### 7.1 编译错误：`undefined reference to 'pthread_create'`

**原因**：忘记链接线程库  

**解决**：在 CMakeLists.txt 中添加：

```CMake

find_package(Threads REQUIRED)
target_link_libraries(your_target PRIVATE Threads::Threads)
```

### 7.2 端口被占用

**错误**：`listen() failed with error: Address already in use`  

**解决**：

1. 换一个端口

2. 或者杀掉占用端口的进程：

```Bash

# 查找占用 8080 端口的进程
netstat -tulpn | grep 8080
# 杀掉进程（PID 替换成实际的进程号）
kill -9 PID
```

### 7.3 最佳实践

1. **使用 ** **`0.0.0.0`** ** 监听**：云服务器必须用 `0.0.0.0`，不能用 `127.0.0.1`，否则外网无法访问

2. **开启日志**：方便调试接口问题

3. **异常处理**：解析 JSON、操作数据库时一定要加 try-catch

4. **参数校验**：所有接口都要校验必填参数，避免空指针

5. **状态码规范**：

    - `200`：成功

    - `400`：参数错误

    - `401`：未授权（Token 无效）

    - `404`：接口不存在

    - `500`：服务器内部错误

---

## 总结

cpp-httplib 是一个非常轻量、易用的 HTTP 库，完全能满足你的毕业设计需求。重点掌握：

1. `Server` 类的路由注册和静态文件服务

2. `Request` 类的参数、Body、文件获取

3. `Response` 类的 JSON 响应设置

配合 `nlohmann/json` 库，你可以快速完成所有后端接口的开发。
> （注：文档部分内容可能由 AI 生成）