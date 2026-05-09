#include "SugarCalc.h"
#include <cmath> // 必须引入，为了使用 log10

double SugarCalc::calculate(const nlohmann::json& spectrum) {
    // ==========================================
    // 工业级 V3：宽动态范围映射 + 吸收率模型
    // ==========================================
    
    double ch555 = spectrum.value("ch555", 0.0);
    double ch640 = spectrum.value("ch640", 0.0);
    double ch680 = spectrum.value("ch680", 0.0);
    double chNIR = spectrum.value("chNIR", 0.0);
    double chClear = spectrum.value("chClear", 1.0); 

    // 1. 环境判据
    if (chClear < 200.0 || chNIR < 10.0) return 0.0; 
    
    // 2. 朗伯-比尔定律 (取对数转吸收率)
    double abs_555 = log10(chClear / (ch555 + 1.0)); 
    double abs_640 = log10(chClear / (ch640 + 1.0)); 
    double abs_680 = log10(chClear / (ch680 + 1.0)); 
    double abs_NIR = log10(chClear / (chNIR + 1.0)); 

    // 3. 模型系数
    double k = -2.5;
    double a = -1.2;
    double b = 3.5;
    double c = 6.8;
    double d = 12.5;

    // 4. MLR 计算
    double raw_brix = (a * abs_555) + (b * abs_640) + (c * abs_680) + (d * abs_NIR) + k;

    // 5. 物理极限软约束
    if (raw_brix < 0.0) return 0.0;
    if (raw_brix > 20.0) return 20.0;

    return raw_brix;
}