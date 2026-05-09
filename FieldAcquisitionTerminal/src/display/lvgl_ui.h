#pragma once
#include <lvgl.h>

// 定义系统运行模式
enum SystemMode {
    MODE_WAITING = 0,
    MODE_TEST = 1,
    MODE_INDUSTRIAL = 2
};

typedef void (*MeasureCallback)();
typedef void (*ModeSelectCallback)(SystemMode mode);
typedef void (*ClearTestCallback)();
typedef void (*ReturnHomeCallback)(); // ✅ 新增：返回主页回调

class UIManager {
public:
    void init(MeasureCallback measureCb, ModeSelectCallback modeCb, ClearTestCallback clearCb, ReturnHomeCallback homeCb);
    
    // 启动页相关
    void showBootScreen();
    void showModeSelection(); 
    bool isIndustrialModeUnlocked();
    
    // 工业模式相关
    void buildIndustrialUI();
    void updateEnv(float t, float h, int l, float g135, float g137);
    void updateSpectrum(uint16_t c415, uint16_t c445, uint16_t c480, uint16_t c515, uint16_t c555, uint16_t c595, uint16_t c640, uint16_t c680, uint16_t clr, uint16_t nir);
    void updateCountdown(uint32_t remain_ms, uint32_t total_ms);
    void addIndustrialHistory(const char* timeStr, float brix, float temp, float hum, int light, bool uploaded);
    void markIndustrialHistoryUploaded(const char* timeStr);

    // 测试模式相关
    void buildTestUI();
    void updateTestStats(float brix, int count, float avgBrix);
    void addTestHistory(int index, float brix, float avg);
    void clearTestTable();

    // 通用
    void updateSugar(float brix);
    void updateStatus(const char* msg);
    void updateWifiStatus(bool connected); // ✅ 新增：WiFi状态更新

private:
    SystemMode current_mode = MODE_WAITING;

    // --- 回调 ---
    MeasureCallback onMeasure;
    ModeSelectCallback onModeSelect;
    ClearTestCallback onClearTest;
    ReturnHomeCallback onReturnHome; // ✅ 新增

    // --- 屏幕公共指针 ---
    lv_obj_t * boot_status_label;
    lv_obj_t * btn_mode_test;
    lv_obj_t * btn_mode_ind;

    lv_obj_t * label_temp;
    lv_obj_t * label_hum;   
    lv_obj_t * label_light;
    lv_obj_t * label_gas; // 新增气体显示标签
    lv_obj_t * label_sugar;
    lv_obj_t * label_spectrum; 
    lv_obj_t * label_status;
    lv_obj_t * label_countdown;
    lv_obj_t * label_wifi; // ✅ 新增：WiFi状态指示器
    lv_obj_t * table_ind_history; 

    lv_obj_t * label_test_sugar;
    lv_obj_t * label_test_sugar_ref; // ✅ 新增：测试模式的参考成熟度标签
    lv_obj_t * table_test_history; 
    
    lv_obj_t * btn_measure;

    // --- 事件代理 ---
    static void btn_measure_cb(lv_event_t * e);
    static void btn_mode_test_cb(lv_event_t * e);
    static void btn_mode_ind_cb(lv_event_t * e);
    static void btn_clear_test_cb(lv_event_t * e);
    static void btn_home_cb(lv_event_t * e); // ✅ 新增
};