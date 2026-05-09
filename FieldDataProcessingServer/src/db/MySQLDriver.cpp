#include "MySQLDriver.h"
#include <iostream>

MySQLDriver::MySQLDriver() {
    conn = mysql_init(nullptr);
}

MySQLDriver::~MySQLDriver() {
    if (conn) {
        mysql_close(conn);
    }
}

MySQLDriver& MySQLDriver::getInstance() {
    static MySQLDriver instance;
    return instance;
}

bool MySQLDriver::connect(const std::string& host, const std::string& user, const std::string& pwd, const std::string& db, int port) {
    // 1. 设置字符集
    mysql_options(conn, MYSQL_SET_CHARSET_NAME, "utf8mb4");

    // 2. 开启自动重连
    bool reconnect = true;
    mysql_options(conn, MYSQL_OPT_RECONNECT, &reconnect);

    // 3. 连接数据库
    if (mysql_real_connect(conn, host.c_str(), user.c_str(), pwd.c_str(), db.c_str(), port, nullptr, 0) == nullptr) {
        std::cerr << "[MySQL Error] Failed to connect: " << mysql_error(conn) << std::endl;
        return false;
    }
 
    // ✅ 新增：强制将当前连接的休眠超时时间拉长到 30 天
    // 30天 = 30 * 24 * 60 * 60 = 2592000 秒
    mysql_query(conn, "SET SESSION wait_timeout = 2592000;");
    mysql_query(conn, "SET SESSION interactive_timeout = 2592000;");

    std::cout << "[MySQL] Connected to database: " << db << " successfully! (Timeout set to 30 days)" << std::endl;
    return true;
}

bool MySQLDriver::execute(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mtx); // 加锁防并发冲突
    
    // 1. 直接尝试执行
    if (mysql_query(conn, sql.c_str()) != 0) {
        unsigned int errCode = mysql_errno(conn);
        // 2. 如果报错是 2006 (Server gone away) 或 2013 (Connection lost)
        if (errCode == 2006 || errCode == 2013) {
            std::cerr << "[MySQL Warning] Connection lost. Attempting to reconnect..." << std::endl;
            mysql_ping(conn); // 触发底层的 MYSQL_OPT_RECONNECT 自动重连
            
            // 3. 重连后重试一次
            if (mysql_query(conn, sql.c_str()) != 0) {
                std::cerr << "[MySQL Error] Execute failed after reconnect: " << mysql_error(conn) << "\nSQL: " << sql << std::endl;
                return false;
            }
        } else {
            std::cerr << "[MySQL Error] Execute failed: " << mysql_error(conn) << "\nSQL: " << sql << std::endl;
            return false;
        }
    }
    return true;
}

std::vector<std::map<std::string, std::string>> MySQLDriver::query(const std::string& sql) {
    std::lock_guard<std::mutex> lock(mtx);
    std::vector<std::map<std::string, std::string>> result_list;

    // 1. 乐观地直接执行
    if (mysql_query(conn, sql.c_str()) != 0) {
        unsigned int errCode = mysql_errno(conn);
        // 2. 如果发现断线，则执行重连
        if (errCode == 2006 || errCode == 2013) {
            std::cerr << "[MySQL Warning] Connection lost. Attempting to reconnect..." << std::endl;
            mysql_ping(conn); // 触发重连
            
            // 3. 重试查询
            if (mysql_query(conn, sql.c_str()) != 0) {
                std::cerr << "[MySQL Error] Query failed after reconnect: " << mysql_error(conn) << "\nSQL: " << sql << std::endl;
                return result_list;
            }
        } else {
            std::cerr << "[MySQL Error] Query failed: " << mysql_error(conn) << "\nSQL: " << sql << std::endl;
            return result_list;
        }
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) return result_list;

    int num_fields = mysql_num_fields(res);
    MYSQL_FIELD* fields = mysql_fetch_fields(res);
    MYSQL_ROW row;

    while ((row = mysql_fetch_row(res))) {
        std::map<std::string, std::string> row_map;
        for (int i = 0; i < num_fields; i++) {
            row_map[fields[i].name] = row[i] ? row[i] : "";
        }
        result_list.push_back(row_map);
    }
    
    mysql_free_result(res);
    return result_list;
}

// 过滤外部字符串，防止 SQL 注入
std::string MySQLDriver::escapeString(const std::string& str) {
    std::lock_guard<std::mutex> lock(mtx);
    if (!conn) return str;
    
    // 注意：mysql_real_escape_string 只是在本地内存处理转义，它不发送网络包，
    // 因此这里不需要加 mysql_ping，只要连接对象初始化过就是安全的。
    char* buffer = new char[str.length() * 2 + 1];
    mysql_real_escape_string(
        conn,           // [输入] MySQL 连接句柄
        buffer,         // [输出] 转义后的结果缓冲区
        str.c_str(),    // [输入] 原始字符串
        str.length()    // [输入] 原始字符串的长度
    );
    std::string escaped(buffer);
    delete[] buffer;
    return escaped;
}