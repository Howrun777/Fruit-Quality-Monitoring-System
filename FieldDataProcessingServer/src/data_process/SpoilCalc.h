#pragma once

class SpoilCalc {
public:
    // 根据 GAS135(苯类) 和 GAS137(乙醇) 计算腐败度百分比 (0.0 ~ 100.0)
    // 越接近 100 代表越腐败
    static double calculate(double gas135_ppm, double gas137_ppm);
};