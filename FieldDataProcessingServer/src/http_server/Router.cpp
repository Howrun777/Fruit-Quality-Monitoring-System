#include "Router.h"
#include "../db/MySQLDriver.h"
#include "../auth/DeviceAuth.h"
#include "../data_process/DataCheck.h"
#include "../data_process/SugarCalc.h"
#include "../data_process/MaturityCalc.h"
#include "../utils/Logger.h"
#include <json.hpp>
#include <iostream>
#include <chrono>
#include "../auth/AdminAuth.h"
#include "../session/Session.h"
#include <fstream>
#include <sstream>
#include "../data_process/SpoilCalc.h"
#include "../vision/VisionTask.h"
#include "../drone/FastDecayQueue.h"

using json = nlohmann::json;

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "<h1>404 Not Found - 找不到文件</h1>";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

void Router::registerRoutes(httplib::Server& svr) {
    
    svr.Get("/ping", [](const httplib::Request& req, httplib::Response& res) {
        res.set_content(R"({"msg": "pong", "status": "success"})", "application/json");
    });

    svr.Get("/api/device/time", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        response["code"] = 200;
        response["msg"] = "时间同步成功";
        response["data"]["server_time"] = now; 
        res.set_content(response.dump(), "application/json");
    });

    // ==========================================
    // ✅ 接口 1: 硬件上传数据 (支持新气体数据入库)
    // ==========================================
    svr.Post("/api/device/upload", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();

        try {
            std::string device_id = req.has_header("device_id") ? req.get_header_value("device_id") : "";
            std::string token = req.has_header("token") ? req.get_header_value("token") : "";

            LOG_REQUEST("POST", "/api/device/upload", device_id, 0, req.body.size(), "Request received");

            if (!DeviceAuth::authenticate(device_id, token)) {
                response["code"] = 401; response["msg"] = "认证失败：Token错误、设备不存在或已被禁用";
                res.set_content(response.dump(), "application/json");
                LOG_REQUEST("POST", "/api/device/upload", device_id, 401, 0, "Authentication failed");
                return;
            }

            auto body = json::parse(req.body);
            double temp = body.value("temperature", 99.0);
            double hum = body.value("humidity", 99.0);
            int light = body.value("light", 99);
            
            // ✅ 核心新增：提取气体数据 (没传就默认99.0)
            double gas135 = body.value("gas135", 99.0);
            double gas137 = body.value("gas137", 99.0);
            
            auto spectrum = body["spectrum_json"]; 
            
            long long collected_at = body.value("collected_at", 0LL);
            if (collected_at <= 0) collected_at = now; 

            std::string field_id = device_id.substr(0, 4);
            if (!DataCheck::isFieldExist(field_id)) {
                response["code"] = 400; response["msg"] = "该设备所属瓜田未在生产信息表中注册";
                res.set_content(response.dump(), "application/json"); return;
            }

            double sugar_brix = SugarCalc::calculate(spectrum);
            double maturity_score = MaturityCalc::calculate(field_id, sugar_brix);
            
            std::string safe_device_id = MySQLDriver::getInstance().escapeString(device_id);
            std::string safe_field_id = MySQLDriver::getInstance().escapeString(field_id);
            std::string safe_spectrum = MySQLDriver::getInstance().escapeString(spectrum.dump());

            std::string sql_w = "INSERT INTO fruit_data (device_id, collected_at, sugar_brix, maturity_score, spectrum_json) VALUES ('" 
                + safe_device_id + "', " + std::to_string(collected_at) + ", " + std::to_string(sugar_brix) + ", " 
                + std::to_string(maturity_score) + ", '" + safe_spectrum + "')";

            bool w_ok = MySQLDriver::getInstance().execute(sql_w);
            bool e_ok = true; 
            std::string extra_msg = "";

            if (DataCheck::isEnvironmentValid(temp, hum, light)) {
                // ✅ 核心新增：SQL 增加 gas135_ppm 和 gas137_ppm 两列写入
                std::string sql_e = "INSERT INTO field_environment (field_id, collected_at, temperature_c, humidity_rh, light_lux, gas135_ppm, gas137_ppm) VALUES ('" 
                    + safe_field_id + "', " + std::to_string(collected_at) + ", " + std::to_string(temp) + ", " 
                    + std::to_string(hum) + ", " + std::to_string(light) + ", " + std::to_string(gas135) + ", " + std::to_string(gas137) + ")";
                e_ok = MySQLDriver::getInstance().execute(sql_e);
            } else {
                extra_msg = " (⚠️无环境传感器，跳过环境保存)";
            }

            if (w_ok && e_ok) {
                response["code"] = 200;
                response["msg"] = "数据上传成功" + extra_msg;
                response["data"]["sugar_brix"] = sugar_brix;
                response["data"]["maturity_score"] = maturity_score;
                response["data"]["server_time"] = now;
                
                // ========== 无人机应急响应：快腐败检测 ==========
                // 腐败度 > 60% 时，将樱桃加入快腐败队列
                double spoilage = SpoilCalc::calculate(gas135, gas137);
                if (spoilage > 60.0) {
                    FastDecayQueue::getInstance().addItem(device_id, spoilage);
                    LOGI("FastDecay", "快腐败樱桃入队 - Device: " + device_id + 
                         ", Spoilage: " + std::to_string(spoilage) + "%");
                }
                // ===============================================
                
                LOG_REQUEST("POST", "/api/device/upload", device_id, 200, req.body.size(),
                    "Data uploaded successfully, sugar_brix: " + std::to_string(sugar_brix));
            } else {
                response["code"] = 500; response["msg"] = "写入失败，可能是主键冲突";
                LOG_REQUEST("POST", "/api/device/upload", device_id, 500, 0, "Database write failed");
            }
        } catch (const std::exception& e) {
            response["code"] = 400; response["msg"] = std::string("JSON格式错误: ") + e.what();
            LOGE("Router", std::string("JSON parse error: ") + e.what());
        }

        res.set_content(response.dump(), "application/json");
    });

    // ==========================================
    // ✅ 多模态AI视觉扩展 (FPGA图像采集模块)
    // POST /api/device/vision
    // 支持两种格式:
    // 1. multipart/form-data: device_id, token, collected_at, image (现有ESP32设备)
    // 2. application/octet-stream: Header中传X-Device-ID/X-Token/X-Timestamp, Body为纯JPEG (FPGA)
    // ==========================================
    svr.Post("/api/device/vision", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        auto request_time = std::chrono::steady_clock::now();

        try {
            std::string device_id;
            std::string token;
            std::string content_type = req.has_header("Content-Type") ? req.get_header_value("Content-Type") : "";
            bool is_fpga_mode = (content_type.find("octet-stream") != std::string::npos);

            LOG_REQUEST("POST", "/api/device/vision", "", 0, req.body.size(),
                is_fpga_mode ? "FPGA mode (octet-stream)" : "ESP32 mode (multipart)");

            if (is_fpga_mode) {
                // ===== FPGA裸二进制JPEG格式 =====
                device_id = req.has_header("X-Device-ID") ? req.get_header_value("X-Device-ID") : "";
                token = req.has_header("X-Device-Token") ? req.get_header_value("X-Device-Token") : "";
                std::string timestamp_str = req.has_header("X-Timestamp") ? req.get_header_value("X-Timestamp") : "";

                LOGI("Vision", "FPGA mode request - Device: " + device_id + ", Timestamp: " + timestamp_str);

                if (device_id.empty() || !DeviceAuth::authenticate(device_id, token)) {
                    response["code"] = 401;
                    response["msg"] = "认证失败：Token错误、设备不存在或已被禁用";
                    res.set_content(response.dump(), "application/json");
                    LOGE("Vision", "FPGA auth failed for device: " + device_id);
                    return;
                }

                int64_t collected_at = 0;
                if (!timestamp_str.empty()) {
                    try {
                        collected_at = std::stoll(timestamp_str);
                    } catch (...) {
                        auto now = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                        collected_at = now;
                    }
                } else {
                    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    collected_at = now;
                }

                const std::string& jpeg_data = req.body;
                size_t actual_size = jpeg_data.size();

                if (actual_size < 100) {
                    response["code"] = 400;
                    response["msg"] = "JPEG数据为空或过小，可能是传输失败";
                    res.set_content(response.dump(), "application/json");
                    LOGE("Vision", "FPGA JPEG data too small: " + std::to_string(actual_size) + " bytes");
                    return;
                }

                if (jpeg_data[0] != (char)0xFF || jpeg_data[1] != (char)0xD8) {
                    response["code"] = 400;
                    response["msg"] = "数据不是有效的JPEG格式（缺少SOI标记）";
                    res.set_content(response.dump(), "application/json");
                    LOGE("Vision", "Invalid JPEG header for device: " + device_id);
                    return;
                }

                std::string timestamp_str2 = std::to_string(collected_at);
                std::string filename = device_id + "_" + timestamp_str2 + ".jpg";
                std::string upload_dir = "../../FieldMonitoringPlatform/assets/uploads/";

                std::string mkdir_cmd = "mkdir -p " + upload_dir;
                system(mkdir_cmd.c_str());

                std::string full_path = upload_dir + filename;
                // ✅ 防呆机制：只存储文件名，不存储路径前缀
                std::string db_filename = filename;

                std::ofstream ofs(full_path, std::ios::binary);
                if (!ofs) {
                    LOGE("Vision", "Failed to create file: " + full_path);
                    response["code"] = 500;
                    response["msg"] = "服务器保存图片失败";
                    res.set_content(response.dump(), "application/json");
                    return;
                }
                ofs.write(jpeg_data.data(), jpeg_data.size());
                ofs.close();

                LOGI("Vision", "FPGA JPEG saved: " + full_path + " (" + std::to_string(actual_size) + " bytes)");

                VisionTask task;
                task.device_id = device_id;
                task.collected_at = collected_at;
                task.image_path = full_path;
                task.image_relative_path = db_filename;

                VisionTaskProcessor::getInstance().enqueue(task);

                auto now = std::chrono::duration_cast<std::chrono::seconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                response["code"] = 200;
                response["msg"] = "FPGA图片上传成功，AI推理任务已加入队列";
                response["data"]["server_time"] = now;
                response["data"]["image_url"] = "assets/uploads/" + db_filename;
                response["data"]["image_size"] = actual_size;
                response["data"]["device_id"] = device_id;
                response["data"]["collected_at"] = collected_at;

                LOG_REQUEST("POST", "/api/device/vision", device_id, 200, actual_size,
                    "FPGA image queued for inference");

            } else {
                // ===== 现有ESP32 multipart/form-data格式 =====
                device_id = req.has_header("device_id") ? req.get_header_value("device_id") : "";
                token = req.has_header("token") ? req.get_header_value("token") : "";

                LOGI("Vision", "ESP32 multipart mode - Device: " + device_id);

                if (!DeviceAuth::authenticate(device_id, token)) {
                    response["code"] = 401;
                    response["msg"] = "认证失败：Token错误、设备不存在或已被禁用";
                    res.set_content(response.dump(), "application/json");
                    LOGE("Vision", "ESP32 auth failed for device: " + device_id);
                    return;
                }

                if (!req.form.has_file("image")) {
                    response["code"] = 400;
                    response["msg"] = "缺少图片文件";
                    res.set_content(response.dump(), "application/json");
                    LOGE("Vision", "Missing image file for device: " + device_id);
                    return;
                }

                auto file = req.form.get_file("image");
                std::string collected_at_str = req.form.get_field("collected_at");
                int64_t collected_at = 0;
                if (!collected_at_str.empty()) {
                    try {
                        collected_at = std::stoll(collected_at_str);
                    } catch (...) {
                        collected_at = std::chrono::duration_cast<std::chrono::seconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();
                    }
                } else {
                    auto now = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                    collected_at = now;
                }

                std::string timestamp = std::to_string(collected_at);
                std::string filename = device_id + "_" + timestamp + ".jpg";
                std::string upload_dir = "../../FieldMonitoringPlatform/assets/uploads/";

                std::string full_path = upload_dir + filename;
                // ✅ 防呆机制：只存储文件名，不存储路径前缀
                std::string db_filename = filename;

                std::ofstream ofs(full_path, std::ios::binary);
                if (!ofs) {
                    LOGE("Vision", "Failed to create file: " + full_path);
                    response["code"] = 500;
                    response["msg"] = "服务器保存图片失败";
                    res.set_content(response.dump(), "application/json");
                    return;
                }
                ofs.write(file.content.data(), file.content.size());
                ofs.close();

                VisionTask task;
                task.device_id = device_id;
                task.collected_at = collected_at;
                task.image_path = full_path;
                task.image_relative_path = db_filename;

                VisionTaskProcessor::getInstance().enqueue(task);

                response["code"] = 200;
                response["msg"] = "图片上传成功，AI推理任务已加入队列";
                response["data"]["image_url"] = "assets/uploads/" + db_filename;
                LOGI("Vision", "ESP32 image saved: " + full_path + " (" + std::to_string(file.content.size()) + " bytes)");
                LOG_REQUEST("POST", "/api/device/vision", device_id, 200, file.content.size(),
                    "ESP32 image queued for inference");
            }

        } catch (const std::exception& e) {
            response["code"] = 500;
            response["msg"] = std::string("服务器内部错误: ") + e.what();
            LOGE("Vision", std::string("Server error: ") + e.what());
        }

        res.set_content(response.dump(), "application/json");
    });


    // ========== Web 端接口 ==========

    svr.Post("/api/admin/login",[](const httplib::Request& req, httplib::Response& res) {
        json response;
        try {
            auto body = json::parse(req.body);
            std::string session_id = AdminAuth::login(body.value("username", ""), body.value("password", ""));
            if (!session_id.empty()) {
                response["code"] = 200; response["msg"] = "登录成功"; response["role"] = 0;
                res.set_header("Set-Cookie", "session_id=" + session_id + "; Path=/; HttpOnly; Max-Age=3600");
            } else {
                response["code"] = 401; response["msg"] = "账号或密码错误";
            }
        } catch (const json::exception& e) {
            response["code"] = 400; response["msg"] = std::string("JSON格式错误: ") + e.what();
        } catch (const std::exception& e) {
            // 关键：捕获数据库等底层异常并返回 500
            response["code"] = 500; response["msg"] = std::string("服务器内部错误: ") + e.what();
        } catch (...) {
            response["code"] = 500; response["msg"] = "未知的系统异常";
        }
        res.set_content(response.dump(), "application/json");
    });
    
    svr.Get("/api/admin/field/list", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        std::string cookie = req.has_header("Cookie") ? req.get_header_value("Cookie") : "";
        std::string sid = ""; size_t pos = cookie.find("session_id=");
        if (pos != std::string::npos) sid = cookie.substr(pos + 11, 32);
        if (!Session::getInstance().isValid(sid)) {
            response["code"] = 401; response["msg"] = "未登录"; res.set_content(response.dump(), "application/json"); return;
        }

        auto db_res = MySQLDriver::getInstance().query("SELECT field_id, fruit_variety FROM field_production");
        json field_list = json::array();
        for (const auto& row : db_res) field_list.push_back({{"field_id", row.at("field_id")}, {"variety", row.at("fruit_variety")}});
        
        response["code"] = 200; response["data"]["total"] = field_list.size(); response["data"]["list"] = field_list;
        res.set_content(response.dump(), "application/json");
    });

    svr.Options(R"(.*)", [](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, device_id, token, X-Device-ID, X-Device-Token, X-Timestamp");
    });

    // ==========================================
    // ✅ 无人机应急响应模块接口
    // GET /api/drone/fast-decay/queue
    // 查询快腐败樱桃队列
    // ==========================================
    svr.Get("/api/drone/fast-decay/queue", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        
        try {
            // 获取设备认证信息
            std::string device_id = req.has_header("device_id") ? req.get_header_value("device_id") : "";
            std::string token = req.has_header("token") ? req.get_header_value("token") : "";
            
            LOG_REQUEST("GET", "/api/drone/fast-decay/queue", device_id, 0, 0, "Drone query request");
            
            // 设备认证
            if (!DeviceAuth::authenticate(device_id, token)) {
                response["code"] = 401;
                response["msg"] = "认证失败：Token错误、设备不存在或已被禁用";
                res.set_content(response.dump(), "application/json");
                LOG_REQUEST("GET", "/api/drone/fast-decay/queue", device_id, 401, 0, "Authentication failed");
                return;
            }
            
            // 清理过期项（超过24小时的记录）
            FastDecayQueue::getInstance().cleanupExpired(86400);
            
            // 获取当前队列
            auto queue = FastDecayQueue::getInstance().getQueue();
            
            // 构建响应数据
            json cherries = json::array();
            for (const auto& item : queue) {
                cherries.push_back({
                    {"deviceId", item.deviceId},
                    {"timestamp", item.timestamp},
                    {"decayRate", item.decayRate}
                });
            }
            
            response["code"] = 200;
            response["msg"] = "success";
            response["data"]["hasFastDecayCherries"] = !queue.empty();
            response["data"]["cherries"] = cherries;
            
            LOG_REQUEST("GET", "/api/drone/fast-decay/queue", device_id, 200, 0,
                "Queue returned " + std::to_string(queue.size()) + " items");
            
        } catch (const std::exception& e) {
            response["code"] = 500;
            response["msg"] = std::string("服务器内部错误: ") + e.what();
            LOGE("FastDecay", std::string("Server error: ") + e.what());
        }
        
        res.set_content(response.dump(), "application/json");
    });

    // 网页路由保护机制
    svr.Get("/", [](const httplib::Request& req, httplib::Response& res) { res.set_content(readFile("../../FieldMonitoringPlatform/index.html"), "text/html; charset=utf-8"); });
    svr.Get("/index.html", [](const httplib::Request& req, httplib::Response& res) { res.set_redirect("/"); });
    svr.Get("/admin/login.html", [](const httplib::Request& req, httplib::Response& res) { res.set_content(readFile("../../FieldMonitoringPlatform/admin/login.html"), "text/html; charset=utf-8"); });

    svr.Get("/admin/dashboard.html", [](const httplib::Request& req, httplib::Response& res) {
        std::string cookie = req.has_header("Cookie") ? req.get_header_value("Cookie") : "";
        std::string sid = ""; size_t pos = cookie.find("session_id=");
        if (pos != std::string::npos) sid = cookie.substr(pos + 11, 32);

        if (!Session::getInstance().isValid(sid)) {
            res.set_redirect("/admin/login.html"); return;
        }
        res.set_content(readFile("../../FieldMonitoringPlatform/admin/dashboard.html"), "text/html; charset=utf-8");
    });
    
    svr.Get("/api/admin/fruit/list", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        std::string field_id = req.has_param("field_id") ? req.get_param_value("field_id") : "";
        if (field_id.empty()) { response["code"] = 400; res.set_content(response.dump(), "application/json"); return; }
        std::string safe_field = MySQLDriver::getInstance().escapeString(field_id);
        
        std::string sql = "SELECT a.device_id, a.sugar_brix, a.maturity_score, a.collected_at FROM fruit_data a "
                          "INNER JOIN (SELECT device_id, MAX(collected_at) as max_time FROM fruit_data GROUP BY device_id) b "
                          "ON a.device_id = b.device_id AND a.collected_at = b.max_time "
                          "WHERE a.device_id LIKE '" + safe_field + "-%'";
        
        auto db_res = MySQLDriver::getInstance().query(sql);
        json fruit_list = json::array();
        for (const auto& row : db_res) {
            fruit_list.push_back({
                {"device_id", row.at("device_id")},
                {"sugar_brix", std::stod(row.at("sugar_brix"))},
                {"maturity_score", std::stod(row.at("maturity_score"))},
                {"collected_at", std::stoll(row.at("collected_at"))} 
            });
        }
        response["code"] = 200; response["data"] = fruit_list;
        res.set_content(response.dump(), "application/json");
    });

    svr.Get("/api/admin/fruit/history", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        std::string device_id = req.has_param("device_id") ? req.get_param_value("device_id") : "";
        auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        long long past_time = now - 84 * 24 * 3600; 
        std::string safe_device = MySQLDriver::getInstance().escapeString(device_id);

        // 使用 LEFT JOIN 聚合传感器数据、环境数据和视觉数据
        // 注意：device_id格式为"1001-01-01"，field_id为"1001"，需要用SUBSTRING提取
        std::string sql = R"(
            SELECT
                f.collected_at,
                f.sugar_brix,
                f.maturity_score,
                e.temperature_c,
                e.humidity_rh,
                e.light_lux,
                e.gas135_ppm,
                e.gas137_ppm,
                v.quality_level,
                v.image_url
            FROM fruit_data f
            LEFT JOIN field_environment e ON SUBSTRING_INDEX(f.device_id, '-', 1) = e.field_id AND f.collected_at = e.collected_at
            LEFT JOIN fruit_vision_data v ON f.device_id = v.device_id AND f.collected_at = v.collected_at
            WHERE f.device_id = ')" + safe_device + R"(' AND f.collected_at > )" + std::to_string(past_time) + R"(
            ORDER BY f.collected_at ASC
        )";

        auto db_res = MySQLDriver::getInstance().query(sql);
        json history = json::array();
        for (const auto& row : db_res) {
            json entry = {
                {"timestamp", std::stoll(row.at("collected_at"))},
                {"sugar_brix", std::stod(row.at("sugar_brix"))},
                {"maturity_score", std::stod(row.at("maturity_score"))}
            };

            // 环境数据（可能为NULL）
            if (row.count("temperature_c") && !row.at("temperature_c").empty()) {
                entry["temperature"] = std::stod(row.at("temperature_c"));
                entry["humidity"] = std::stod(row.at("humidity_rh"));
                entry["light"] = std::stoi(row.at("light_lux"));
                if (row.count("gas135_ppm") && !row.at("gas135_ppm").empty()) {
                    entry["gas135_ppm"] = std::stod(row.at("gas135_ppm"));
                    entry["gas137_ppm"] = std::stod(row.at("gas137_ppm"));
                }
            }

            // 视觉数据（可能为NULL）
            if (row.count("quality_level") && !row.at("quality_level").empty()) {
                entry["quality_level"] = std::stoi(row.at("quality_level"));
                // ✅ 防呆机制：返回完整路径，数据库只存文件名
                entry["image_url"] = "assets/uploads/" + row.at("image_url");
            }

            history.push_back(entry);
        }
        response["code"] = 200; response["data"] = history;
        res.set_content(response.dump(), "application/json");
    });

    // ==========================================
    // ✅ 接口 2: 获取环境历史数据 (连带气体数据下发)
    // ==========================================
    svr.Get("/api/admin/field/environment", [](const httplib::Request& req, httplib::Response& res) {
        json response;
        try {                                               // ✅ 加上 try-catch
            std::string field_id = req.has_param("field_id") ? req.get_param_value("field_id") : "";
            if (field_id.empty()) {
                response["code"] = 400; response["msg"] = "缺少 field_id 参数";
                res.set_content(response.dump(), "application/json"); return;
            }
            auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            long long past_time = now - 84 * 24 * 3600;
            std::string safe_field = MySQLDriver::getInstance().escapeString(field_id);
            std::string sql = "SELECT collected_at, temperature_c, humidity_rh, light_lux, gas135_ppm, gas137_ppm "
                              "FROM field_environment WHERE field_id = '" + safe_field + 
                              "' AND collected_at > " + std::to_string(past_time) + 
                              " ORDER BY collected_at ASC";
            auto db_res = MySQLDriver::getInstance().query(sql);
            json env_data = json::array();
            for (const auto& row : db_res) {
                double g135 = std::stod(row.at("gas135_ppm"));
                double g137 = std::stod(row.at("gas137_ppm"));
                double spoilage = SpoilCalc::calculate(g135, g137);
                env_data.push_back({
                    {"timestamp", std::stoll(row.at("collected_at"))},
                    {"temperature", std::stod(row.at("temperature_c"))},
                    {"humidity", std::stod(row.at("humidity_rh"))},
                    {"light", std::stoi(row.at("light_lux"))},
                    {"spoilage", spoilage}
                });
            }
            response["code"] = 200; response["data"] = env_data;
        } catch (const std::exception& e) {               // ✅ 捕获异常，返回合法 JSON
            response["code"] = 500;
            response["msg"] = std::string("服务器内部错误: ") + e.what();
            std::cerr << "[ERROR] /api/admin/field/environment: " << e.what() << std::endl;
        }
        res.set_content(response.dump(), "application/json");
    });
}