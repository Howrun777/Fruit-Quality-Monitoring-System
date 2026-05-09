#pragma once
#include "../../third_party/httplib/httplib.h"
#include <string>

class HttpServer {
public:
    HttpServer();
    ~HttpServer();

    // 启动服务器
    void start(const std::string& host, int port);
    // 停止服务器
    void stop();

private:
    httplib::Server svr; // cpp-httplib 的核心服务器对象
};