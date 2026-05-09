#pragma once
#include <string>

class DeviceAuth {
public:
    // 校验设备是否合法且启用
    static bool authenticate(const std::string& device_id, const std::string& token);
};