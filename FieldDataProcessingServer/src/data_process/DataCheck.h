#pragma once
#include <string>

class DataCheck {
public:
    static bool isEnvironmentValid(double temp, double hum, int light);
    static bool isFieldExist(const std::string& field_id);
};