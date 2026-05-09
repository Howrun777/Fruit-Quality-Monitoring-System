#include "MaturityCalc.h"
#include "../db/MySQLDriver.h"
#include <iostream>

double MaturityCalc::calculate(const std::string& field_id, double sugar_brix) {
    std::string sql = "SELECT mature_sugar_threshold FROM field_production WHERE field_id = '" + field_id + "'";
    auto result = MySQLDriver::getInstance().query(sql);
    
    if (result.empty()) {
        std::cerr << "Warning: Field not found when calculating maturity!" << std::endl;
        return 0.0;
    }
    
    // 从数据库拿到的都是 string，需要转成 double
    double threshold = std::stod(result[0]["mature_sugar_threshold"]);
    if (threshold <= 0) return 0.0;
    
    return sugar_brix / threshold; // 比如：当前 13.0 / 阈值 12.5 = 1.04
}