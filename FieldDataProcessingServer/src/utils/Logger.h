#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>

/**
 * @brief 日志系统单例类
 * 
 * 功能特性：
 * - 线程安全
 * - 同时输出到文件和控制台
 * - 日志分级 (DEBUG/INFO/WARN/ERROR)
 * - 自动按日期分割日志文件
 * - 异步写入（可选）
 */
class Logger {
public:
    enum Level {
        DEBUG = 0,
        INFO = 1,
        WARN = 2,
        ERROR = 3
    };

    static Logger& getInstance();

    void init(const std::string& log_dir = "./logs", Level min_level = INFO);

    void log(Level level, const std::string& tag, const std::string& message);

    void debug(const std::string& tag, const std::string& message);
    void info(const std::string& tag, const std::string& message);
    void warn(const std::string& tag, const std::string& message);
    void error(const std::string& tag, const std::string& message);

    void logRequest(const std::string& method, const std::string& endpoint,
                    const std::string& device_id, int status, int64_t bytes = 0,
                    const std::string& extra = "");

    void logResponse(const std::string& method, const std::string& endpoint,
                     int status, int64_t duration_ms, const std::string& msg = "");

    std::string getLogFilePath() const { return current_log_file_; }

    void flush();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();
    ~Logger();

    std::string getCurrentTime();
    std::string getCurrentDate();
    std::string levelToString(Level level);
    void ensureLogFile();

    static const char* LEVEL_TAGS_[4];

    std::ofstream file_;
    std::recursive_mutex mutex_;
    Level min_level_;
    std::string log_dir_;
    std::string current_log_file_;
    std::string current_date_;
    bool initialized_;
};

#define LOGD(tag, msg) Logger::getInstance().debug(tag, msg)
#define LOGI(tag, msg) Logger::getInstance().info(tag, msg)
#define LOGW(tag, msg) Logger::getInstance().warn(tag, msg)
#define LOGE(tag, msg) Logger::getInstance().error(tag, msg)
#define LOG_REQUEST(method, endpoint, device_id, status, bytes, extra) \
    Logger::getInstance().logRequest(method, endpoint, device_id, status, bytes, extra)
#define LOG_RESPONSE(method, endpoint, status, duration, msg) \
    Logger::getInstance().logResponse(method, endpoint, status, duration, msg)
