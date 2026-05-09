#pragma once
#include <json.hpp>

class SugarCalc {
public:
    // 根据光谱数据计算糖度 (MLR算法)
    static double calculate(const nlohmann::json& spectrum);
};

