#pragma once
#include <mysql/mysql.h>
#include <string>
#include <mutex>
#include <vector>
#include <map>

class MySQLDriver {
public:
    // 单例模式，全局共享一个数据库连接
    static MySQLDriver& getInstance();

    // 连接数据库
    bool connect(const std::string& host, const std::string& user, const std::string& pwd, const std::string& db, int port);
    
    // 执行增删改 SQL 语句
    bool execute(const std::string& sql);
    
    // 防 SQL 注入：转义危险字符
    std::string escapeString(const std::string& str);
    
    // 执行查询语句，返回一个包含多行字典（字段名: 值）的数组
    std::vector<std::map<std::string, std::string>> query(const std::string& sql);

private:
    MySQLDriver();
    ~MySQLDriver();
    
    MYSQL* conn;
    std::mutex mtx; // 保证多线程写入安全
};