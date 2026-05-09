#pragma once
#include <string>

class MaturityCalc {
public:
    // 根据瓜田号查询阈值，计算成熟度
    static double calculate(const std::string& field_id, double sugar_brix);
};