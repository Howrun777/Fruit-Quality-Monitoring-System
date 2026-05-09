#pragma once
#include "../../third_party/httplib/httplib.h"

class Router {
public:
    // 将所有路由规则注册到传入的 Server 实例上
    static void registerRoutes(httplib::Server& svr);
};