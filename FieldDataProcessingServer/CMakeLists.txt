cmake_minimum_required(VERSION 3.10)

# ====================== 项目基础配置 ======================
# 项目名称：FieldDataProcessingServer（智能水果后端服务器）
project(FieldDataProcessingServer)

# 设置 C++ 标准（cpp-httplib、nlohmann/json 推荐 C++17）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 开启编译警告（帮助提前发现代码问题）
add_compile_options(-Wall -Wextra -Wpedantic)

# ====================== 头文件搜索路径 ======================
include_directories(
    # 你的源码根目录（让编译器能找到 src/http_server/HttpServer.h）
    ${PROJECT_SOURCE_DIR}/src
    # 第三方库：cpp-httplib（单头文件）
    ${PROJECT_SOURCE_DIR}/third_party/httplib
    # 第三方库：nlohmann/json（单头文件）
    ${PROJECT_SOURCE_DIR}/third_party/json
    # 第三方库：ONNX Runtime C++
    ${PROJECT_SOURCE_DIR}/third_party/onnxruntime/onnxruntime-linux-x64-1.16.3/include
)

# ====================== 查找系统依赖库 ======================
# 3.1 线程库（cpp-httplib 多线程必须）
find_package(Threads REQUIRED)

# 3.2 MySQL 客户端库（db/MySQLDriver.cpp 需要）
# 提示：如果找不到，先执行 sudo apt install libmysqlclient-dev
find_path(MYSQL_INCLUDE_DIR 
    NAMES mysql.h 
    PATHS /usr/include/mysql /usr/local/include/mysql
)
find_library(MYSQL_LIBRARY 
    NAMES mysqlclient 
    PATHS /usr/lib/mysql /usr/local/lib/mysql
)

if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "❌ MySQL 客户端库未找到！请先安装：\nsudo apt install libmysqlclient-dev")
endif()
include_directories(${MYSQL_INCLUDE_DIR})
message(STATUS "✅ MySQL 客户端库找到：${MYSQL_LIBRARY}")

# 3.3 libsodium 库（utils/PasswordHasher.cpp 需要，用于 Argon2 密码哈希）
# 提示：如果找不到，先执行 sudo apt install libsodium-dev
find_path(SODIUM_INCLUDE_DIR 
    NAMES sodium.h 
    PATHS /usr/include /usr/local/include
)
find_library(SODIUM_LIBRARY 
    NAMES sodium 
    PATHS /usr/lib /usr/local/lib
)
if(NOT SODIUM_INCLUDE_DIR OR NOT SODIUM_LIBRARY)
    message(FATAL_ERROR "❌ libsodium 库未找到！请先安装：\nsudo apt install libsodium-dev")
endif()
include_directories(${SODIUM_INCLUDE_DIR})
message(STATUS "✅ libsodium 库找到：${SODIUM_LIBRARY}")

# 3.4 ONNX Runtime 库（vision/ONNXInference.cpp 需要）
set(ONNX_RUNTIME_LIB ${PROJECT_SOURCE_DIR}/third_party/onnxruntime/onnxruntime-linux-x64-1.16.3/lib/libonnxruntime.so.1.16.3)
message(STATUS "✅ ONNX Runtime 库：${ONNX_RUNTIME_LIB}")

# ====================== 源文件列表 ======================
# 注意：根据你的新文件结构，已去掉 JsonParser.cpp/h
set(SRC_FILES
    # 入口文件
    src/main.cpp
    # HTTP 服务模块
    src/http_server/HttpServer.cpp
    src/http_server/Router.cpp
    # 认证模块
    src/auth/DeviceAuth.cpp
    src/auth/AdminAuth.cpp
    # 数据处理模块
    src/data_process/SugarCalc.cpp
    src/data_process/SpoilCalc.cpp
    src/data_process/MaturityCalc.cpp
    src/data_process/DataCheck.cpp
    # 数据库模块
    src/db/MySQLDriver.cpp
    src/db/ConnectionPool.cpp
    # Session 管理
    src/session/Session.cpp
    # 工具类
    src/utils/PasswordHasher.cpp
    src/utils/Logger.cpp
    # 视觉AI模块 (多模态AI扩展)
    src/vision/ImageProcessor.cpp
    src/vision/ONNXInference.cpp
    src/vision/VisionTask.cpp
    # 无人机应急响应模块
    src/drone/FastDecayQueue.cpp
)

# ====================== 生成可执行文件 ======================
add_executable(FieldDataProcessingServer ${SRC_FILES})

# ====================== 链接所有依赖库 ======================
target_link_libraries(FieldDataProcessingServer PRIVATE
    Threads::Threads      # 线程库
    ${MYSQL_LIBRARY}      # MySQL 客户端库
    ${SODIUM_LIBRARY}     # libsodium 密码哈希库
    ${ONNX_RUNTIME_LIB}    # ONNX Runtime C++ 推理库
)

# ====================== 编译成功后的友好提示 ======================
add_custom_command(TARGET FieldDataProcessingServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "======================================="
    COMMAND ${CMAKE_COMMAND} -E echo "   ✅ 编译成功！"
    COMMAND ${CMAKE_COMMAND} -E echo "   📦 可执行文件：${CMAKE_BINARY_DIR}/FieldDataProcessingServer"
    COMMAND ${CMAKE_COMMAND} -E echo "   📝 配置文件：${PROJECT_SOURCE_DIR}/config.ini"
    COMMAND ${CMAKE_COMMAND} -E echo "   🗄️  数据库脚本：${PROJECT_SOURCE_DIR}/sql/init_db.sql"
    COMMAND ${CMAKE_COMMAND} -E echo "   🚀 运行命令：./FieldDataProcessingServer"
    COMMAND ${CMAKE_COMMAND} -E echo "======================================="
    COMMAND ${CMAKE_COMMAND} -E echo ""
)