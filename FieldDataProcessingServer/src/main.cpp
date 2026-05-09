#include <iostream>
#include <sodium.h>
#include "http_server/HttpServer.h"
#include "db/MySQLDriver.h"
#include "utils/PasswordHasher.h"
#include "utils/Logger.h"
#include "vision/ONNXInference.h"
#include "vision/VisionTask.h"

int main() {
    std::cout << "=======================================" << std::endl;
    std::cout << "   Smart Fruit System - V4.0           " << std::endl;
    std::cout << "   Logging System Enabled               " << std::endl;
    std::cout << "=======================================" << std::endl;
    
    Logger::getInstance().init("./logs", Logger::INFO);
    LOGI("Main", "Application starting...");

    if (sodium_init() < 0) {
        LOGE("Main", "libsodium initialization failed!");
        return 1;
    }

    bool db_ok = MySQLDriver::getInstance().connect("127.0.0.1", "sws_user", "Mhr289839.", "FruitDataBase", 3306);
    if (!db_ok) {
        LOGE("Main", "Database connection failed!");
        return 1;
    }
    LOGI("Main", "Database connected successfully");

    LOGI("Main", "Updating 'admin' password to Argon2 hash...");
    std::string new_hash = PasswordHasher::hash("admin123");
    MySQLDriver::getInstance().execute("UPDATE admin_users SET password_hash = '" + new_hash + "' WHERE username = 'admin'");

    std::string model_path = "../../models/cherry_convnext_tiny.onnx";
    if (ONNXInference::getInstance().initialize(model_path)) {
        LOGI("Main", "ONNX model loaded successfully!");
    } else {
        LOGW("Main", "ONNX model not available");
    }

    LOGI("Main", "Starting Vision Task Processor...");
    VisionTaskProcessor::getInstance().start();

    LOGI("Main", "Starting HTTP Server on 0.0.0.0:9000...");
    HttpServer server;
    server.start("0.0.0.0", 9000);

    LOGI("Main", "Shutting down...");
    VisionTaskProcessor::getInstance().stop();
    Logger::getInstance().flush();

    return 0;
}
