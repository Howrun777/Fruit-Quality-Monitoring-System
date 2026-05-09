#pragma once
#include <ArduinoJson.h>
#include <math.h>

class SugarCalc {
public:
    static float calculate(const JsonObject& spectrum) {
        // 使用 0.0f 明确告诉 ESP32 这是单精度浮点数
        float ch555 = spectrum["ch555"] | 0.0f;
        float ch640 = spectrum["ch640"] | 0.0f;
        float ch680 = spectrum["ch680"] | 0.0f;
        float chNIR = spectrum["chNIR"] | 0.0f;
        float chClear = spectrum["chClear"] | 1.0f; 

        // 严苛的环境判据
        if (chClear < 200.0f || chNIR < 10.0f) return 0.0f; 
        
        // ✅ 核心修复：使用单片机专用的 log10f，防止 double 转换导致的内存/硬件异常
        float abs_555 = log10f(chClear / (ch555 + 1.0f)); 
        float abs_640 = log10f(chClear / (ch640 + 1.0f)); 
        float abs_680 = log10f(chClear / (ch680 + 1.0f)); 
        float abs_NIR = log10f(chClear / (chNIR + 1.0f)); 

        float k = -2.5f;
        float a = -1.2f;
        float b = 3.5f;
        float c = 6.8f;
        float d = 12.5f;

        float raw_brix = (a * abs_555) + (b * abs_640) + (c * abs_680) + (d * abs_NIR) + k;

        // ✅ 核心防御：防止算出 NaN (非数字) 导致 LVGL 界面卡死
        if (isnan(raw_brix) || raw_brix < 0.0f) return 0.0f;
        if (raw_brix > 20.0f) return 20.0f;

        return raw_brix;
    }
};