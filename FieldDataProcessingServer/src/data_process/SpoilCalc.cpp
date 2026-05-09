#include "SpoilCalc.h"

double SpoilCalc::calculate(double gas135_ppm, double gas137_ppm) {
    // 容错处理：如果没接气体传感器 (收到 99.0)，默认返回 0% (绝对新鲜)
    if (gas135_ppm == 99.0 && gas137_ppm == 99.0) return 0.0;

    // ==========================================
    // 樱桃腐败度评估模型 (Spoilage Index Model)
    // 综合考量苯类挥发物(135)与发酵产生的乙醇(137)
    // ==========================================
    
    // 你在 Arduino 设定的阈值基准：
    // G135 新鲜 < 150， 变质 > 300
    // G137 新鲜 < 180， 变质 > 350
    
    // 1. 将浓度映射到 0~1 的风险系数
    double risk135 = (gas135_ppm - 100.0) / 200.0; // 假设100以下风险为0，300以上风险为1
    double risk137 = (gas137_ppm - 120.0) / 230.0; // 假设120以下风险为0，350以上风险为1
    
    if (risk135 < 0) risk135 = 0; if (risk135 > 1.0) risk135 = 1.0;
    if (risk137 < 0) risk137 = 0; if (risk137 > 1.0) risk137 = 1.0;
    
    // 2. 赋予权重 (乙醇对于水果发酵腐败的指示意义更大，权重给高一点)
    double spoil_score = (risk135 * 0.4) + (risk137 * 0.6);
    
    // 3. 转为百分比 (0 ~ 100)
    return spoil_score * 100.0;
}