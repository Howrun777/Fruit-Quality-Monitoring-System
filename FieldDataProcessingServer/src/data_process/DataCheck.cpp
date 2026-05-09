#include "DataCheck.h"
#include "../db/MySQLDriver.h"

bool DataCheck::isEnvironmentValid(double temp, double hum, int light) {
    // 规则：不能三个都是 99
    return !(temp == 99.0 && hum == 99.0 && light == 99);
}

bool DataCheck::isFieldExist(const std::string& field_id) {
    std::string sql = "SELECT field_id FROM field_production WHERE field_id = '" + field_id + "'";
    auto result = MySQLDriver::getInstance().query(sql);
    return !result.empty(); // 只要能查到就说明瓜田存在
}