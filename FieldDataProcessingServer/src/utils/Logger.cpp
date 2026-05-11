#include "Logger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <cstring>

const char* Logger::LEVEL_TAGS_[4] = {"DEBUG", "INFO", "WARN", "ERROR"};

Logger::Logger() : min_level_(INFO), initialized_(false) {
}

Logger::~Logger() {
    if (file_.is_open()) {
        file_.flush();
        file_.close();
    }
}

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

std::string Logger::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
        << "." << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string Logger::getCurrentDate() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
    return oss.str();
}

std::string Logger::levelToString(Level level) {
    if (level >= DEBUG && level <= ERROR) {
        return LEVEL_TAGS_[level];
    }
    return "UNKNOWN";
}

void Logger::init(const std::string& log_dir, Level min_level) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    log_dir_ = log_dir;
    min_level_ = min_level;
    current_date_ = getCurrentDate();

#ifdef _WIN32
    std::string mkdir_cmd = "mkdir /d " + log_dir + " 2>nul";
#else
    std::string mkdir_cmd = "mkdir -p " + log_dir;
#endif
    system(mkdir_cmd.c_str());

    current_log_file_ = log_dir_ + "/server_" + current_date_ + ".log";
    
    file_.open(current_log_file_, std::ios::app);
    if (!file_.is_open()) {
        std::cerr << "[Logger] WARNING: Cannot open log file: " << current_log_file_ << std::endl;
        current_log_file_ = "./server_" + current_date_ + ".log";
        file_.open(current_log_file_, std::ios::app);
    }

    initialized_ = true;
    log(INFO, "Logger", "Logging system initialized. Log file: " + current_log_file_);
}

void Logger::ensureLogFile() {
    std::string today = getCurrentDate();
    if (today != current_date_) {
        if (file_.is_open()) {
            file_.flush();
            file_.close();
        }
        current_date_ = today;
        current_log_file_ = log_dir_ + "/server_" + current_date_ + ".log";
        file_.open(current_log_file_, std::ios::app);
        if (!file_.is_open()) {
            std::cerr << "[Logger] WARNING: Cannot open log file: " << current_log_file_ << std::endl;
        }
    }
}

void Logger::log(Level level, const std::string& tag, const std::string& message) {
    if (!initialized_ || level < min_level_) return;

    std::lock_guard<std::recursive_mutex> lock(mutex_);
    
    ensureLogFile();

    std::ostringstream oss;
    oss << "[" << getCurrentTime() << "] "
        << "[" << levelToString(level) << "] "
        << "[" << tag << "] "
        << message;

    std::string log_line = oss.str();

    if (file_.is_open()) {
        file_ << log_line << std::endl;
        file_.flush();
    }

    std::cout << log_line << std::endl;
}

void Logger::debug(const std::string& tag, const std::string& message) {
    log(DEBUG, tag, message);
}

void Logger::info(const std::string& tag, const std::string& message) {
    log(INFO, tag, message);
}

void Logger::warn(const std::string& tag, const std::string& message) {
    log(WARN, tag, message);
}

void Logger::error(const std::string& tag, const std::string& message) {
    log(ERROR, tag, message);
}

void Logger::logRequest(const std::string& method, const std::string& endpoint,
                        const std::string& device_id, int status, int64_t bytes,
                        const std::string& extra) {
    std::ostringstream oss;
    oss << "[" << method << "] " << endpoint;
    
    if (!device_id.empty()) {
        oss << " | Device: " << device_id;
    }
    oss << " | Status: " << status;
    
    if (bytes > 0) {
        oss << " | Size: " << bytes << " bytes";
    }
    
    if (!extra.empty()) {
        oss << " | " << extra;
    }

    Level level = (status < 400) ? INFO : ERROR;
    log(level, "HTTP", oss.str());
}

void Logger::logResponse(const std::string& method, const std::string& endpoint,
                         int status, int64_t duration_ms, const std::string& msg) {
    std::ostringstream oss;
    oss << "[" << method << "] " << endpoint;
    oss << " | Status: " << status;
    oss << " | Duration: " << duration_ms << "ms";
    
    if (!msg.empty()) {
        oss << " | " << msg;
    }

    Level level = (status < 400) ? INFO : ERROR;
    log(level, "HTTP", oss.str());
}

void Logger::flush() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (file_.is_open()) {
        file_.flush();
    }
}
