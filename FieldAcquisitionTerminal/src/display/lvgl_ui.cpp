#include "lvgl_ui.h"
#include "../config.h"
#include <Arduino.h> 

// 辅助颜色
#define COLOR_BG lv_color_hex(0xccffcc) // 淡绿色背景
#define COLOR_TEXT lv_color_hex(0x000000) // 黑色文字
#define COLOR_BTN_TEST lv_color_hex(0x3498db) // 蓝色测试按钮
#define COLOR_BTN_IND lv_color_hex(0xe67e22) // 橙色工业按钮
#define COLOR_DANGER lv_color_hex(0xe74c3c) // 红色警告
#define COLOR_GREY lv_color_hex(0x95a5a6) // 灰色

void UIManager::init(MeasureCallback measureCb, ModeSelectCallback modeCb, ClearTestCallback clearCb, ReturnHomeCallback homeCb) {
    this->onMeasure = measureCb;
    this->onModeSelect = modeCb;
    this->onClearTest = clearCb;
    this->onReturnHome = homeCb; // 绑定返回主页事件

    showBootScreen();
}


// ==========================================
// 1. 启动页 (动态显示模式按钮)
// ==========================================
void UIManager::showBootScreen() {
    // 1. 暴力清空当前屏幕上的所有控件
    // 防止从其他页面切回来时，旧控件残留造成内存泄漏和显示混乱
    lv_obj_clean(lv_scr_act()); 
    
    // 2. 设置整个屏幕的背景颜色为淡绿色
    // COLOR_BG 是在文件顶部定义的宏：淡绿色 (0xccffcc)
    lv_obj_set_style_bg_color(lv_scr_act(), COLOR_BG, 0);

    // ✅ 新增：每次进入启动屏时，必须重置指针为 null，代表还没解锁
    btn_mode_ind = nullptr;

    // --- 3. 创建顶部标题 "Smart Fruit System" ---
    lv_obj_t * title = lv_label_create(lv_scr_act()); // 创建一个文本标签控件
    lv_obj_set_style_text_color(title, COLOR_TEXT, 0); // 设置文字颜色为黑色 (COLOR_TEXT)
    lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0); // 设置字体为 16 号
    lv_label_set_text(title, "Smart Fruit System"); // 设置标题的具体文字内容
    // 设置标签位置：顶部居中对齐
    // 参数说明：X轴偏移 0，Y轴偏移 30 (距离屏幕顶部 30 像素)
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // --- 4. 创建设备信息区 (显示 ID 和 品种) ---
    lv_obj_t * info = lv_label_create(lv_scr_act()); // 创建文本标签
    // ✅ 修改1：颜色改成深蓝色 (经典商务蓝 #2980b9)
    lv_obj_set_style_text_color(info, lv_color_hex(0x2980b9), 0); 
    // ✅ 修改2：字体放大到 20 号 (超大醒目)
    lv_obj_set_style_text_font(info, &lv_font_montserrat_16, 0);
    // 使用 \n 换行符，一次性显示两行内容：
    // 第一行显示设备 ID，第二行显示水果品种
    lv_label_set_text_fmt(info, "ID: %s\nVar: %s", DEVICE_ID, FRUIT_VARIETY);
    // 设置标签位置：顶部居中对齐
    // 参数说明：X轴偏移 0，Y轴偏移 60
    lv_obj_align(info, LV_ALIGN_TOP_MID, 0, 60);

    // --- 5. 创建状态提示标签 (显示当前同步状态) ---
    // ✅ 状态提示：默认处于等待时间同步状态
    boot_status_label = lv_label_create(lv_scr_act());
    // 设置文字颜色为红色 (COLOR_DANGER)，醒目提示用户当前状态
    lv_obj_set_style_text_color(boot_status_label, COLOR_DANGER, 0);
    // 设置默认文字：等待时间同步
    lv_label_set_text(boot_status_label, "Syncing time...");
    // 设置标签位置：屏幕正中心，稍微往上一点
    // 参数说明：X轴偏移 0，Y轴偏移 -30 (在屏幕中心上方 30 像素)
    lv_obj_align(boot_status_label, LV_ALIGN_CENTER, 0, -30);

    // --- 6. 创建测试模式按钮 (用户可以直接点击进入) ---
    // ✅ 测试模式按钮：开机立即可见！不需要等时间同步
    btn_mode_test = lv_btn_create(lv_scr_act()); // 创建按钮控件
    lv_obj_set_size(btn_mode_test, 180, 45); // 设置按钮大小：宽 180 像素，高 45 像素
    // 设置按钮位置：屏幕中心对齐
    // 参数说明：X轴偏移 0，Y轴偏移 20 (在状态标签下方 50 像素)
    lv_obj_align(btn_mode_test, LV_ALIGN_CENTER, 0, 20);
    // 设置按钮背景颜色为蓝色 (COLOR_BTN_TEST)
    lv_obj_set_style_bg_color(btn_mode_test, COLOR_BTN_TEST, 0);
    // 绑定按钮点击事件：用户点击后，会调用 btn_mode_test_cb 回调函数
    // 最后一个参数 this 是把当前 UIManager 对象传给回调函数
    lv_obj_add_event_cb(btn_mode_test, btn_mode_test_cb, LV_EVENT_CLICKED, this);
    // 在按钮上创建文字标签
    lv_obj_t * lbl_test = lv_label_create(btn_mode_test);
    // 设置按钮上的文字：Consumer Test Mode
    lv_label_set_text(lbl_test, "Consumer Test Mode");
    // 让文字在按钮内部居中显示
    lv_obj_center(lbl_test);

    // --- 7. 创建底部设计者签名 ---
    lv_obj_t * author = lv_label_create(lv_scr_act());
    // 设置文字颜色为黑色
    lv_obj_set_style_text_color(author, COLOR_TEXT, 0);
    // 设置签名文字：Designed by: Howrun
    lv_label_set_text(author, "Designed by: Howrun");
    // 设置标签位置：底部居中对齐
    // 参数说明：X轴偏移 0，Y轴偏移 -20 (距离屏幕底部 20 像素)
    lv_obj_align(author, LV_ALIGN_BOTTOM_MID, 0, -20);
}

// 检查橙色按钮是否已经存在
bool UIManager::isIndustrialModeUnlocked() {
    return (btn_mode_ind != nullptr);
}

// ✅ 当时间真正同步成功后，调用此函数释放工业模式
void UIManager::showModeSelection() {
    
    // 改变提示语和颜色
    lv_label_set_text(boot_status_label, "Time Synced! Select Mode:");
    lv_obj_set_style_text_color(boot_status_label, COLOR_TEXT, 0);

    // 动态创建并显示工业模式按钮
    btn_mode_ind = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_mode_ind, 180, 45);
    lv_obj_align(btn_mode_ind, LV_ALIGN_CENTER, 0, 80);
    lv_obj_set_style_bg_color(btn_mode_ind, COLOR_BTN_IND, 0);
    lv_obj_add_event_cb(btn_mode_ind, btn_mode_ind_cb, LV_EVENT_CLICKED, this);
    lv_obj_t * lbl_ind = lv_label_create(btn_mode_ind);
    lv_label_set_text(lbl_ind, "Industrial Mode");
    lv_obj_center(lbl_ind);
}

// ==========================================
// 2. 测试模式 UI
// ==========================================
void UIManager::buildTestUI() {
    current_mode = MODE_TEST;
    lv_obj_clean(lv_scr_act());
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xfff5e6), 0); 

    lv_obj_t * title = lv_label_create(lv_scr_act());
    lv_label_set_text(title, "Test Mode (Local Only)");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    // ✅ 双色大字排版 (利用 Flex 容器水平排列)
    lv_obj_t * sugar_cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(sugar_cont, 220, 40);
    lv_obj_align(sugar_cont, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_bg_opa(sugar_cont, 0, 0); 
    lv_obj_set_style_border_width(sugar_cont, 0, 0);
    lv_obj_set_flex_flow(sugar_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(sugar_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(sugar_cont, 0, 0);

    label_test_sugar = lv_label_create(sugar_cont);
    lv_obj_set_style_text_font(label_test_sugar, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(label_test_sugar, COLOR_DANGER, 0);
    lv_label_set_text(label_test_sugar, "--.-");

    label_test_sugar_ref = lv_label_create(sugar_cont);
    lv_obj_set_style_text_font(label_test_sugar_ref, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label_test_sugar_ref, COLOR_GREY, 0);
    lv_label_set_text(label_test_sugar_ref, " /12.5Brix");

    // ✅ 历史记录表格 (5列，极致压榨空间)
    table_test_history = lv_table_create(lv_scr_act());
    lv_obj_set_size(table_test_history, 240, 160);
    lv_obj_align(table_test_history, LV_ALIGN_TOP_MID, 0, 75);
    lv_obj_set_style_text_font(table_test_history, &lv_font_montserrat_12, LV_PART_ITEMS);
    
    lv_table_set_col_cnt(table_test_history, 5);
    lv_table_set_col_width(table_test_history, 0, 50); // 次数
    lv_table_set_col_width(table_test_history, 1, 50); // 糖度
    lv_table_set_col_width(table_test_history, 2, 50); // 成熟度
    lv_table_set_col_width(table_test_history, 3, 50); // 均糖
    lv_table_set_col_width(table_test_history, 4, 60); // 均熟
    
    lv_table_set_cell_value(table_test_history, 0, 0, "No.");
    lv_table_set_cell_value(table_test_history, 0, 1, "Brix");
    lv_table_set_cell_value(table_test_history, 0, 2, "M%");
    lv_table_set_cell_value(table_test_history, 0, 3, "Avg");
    lv_table_set_cell_value(table_test_history, 0, 4, "AM%");

    // 底部按钮组 (3个)
    lv_obj_t * btn_home = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_home, 60, 35);
    lv_obj_align(btn_home, LV_ALIGN_BOTTOM_LEFT, 5, -10);
    lv_obj_set_style_bg_color(btn_home, COLOR_GREY, 0);
    lv_obj_add_event_cb(btn_home, btn_home_cb, LV_EVENT_CLICKED, this);
    lv_obj_t * lbl_home = lv_label_create(btn_home);
    lv_label_set_text(lbl_home, "HOME"); lv_obj_center(lbl_home);

    lv_obj_t * btn_clear = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_clear, 65, 35);
    lv_obj_align(btn_clear, LV_ALIGN_BOTTOM_MID, -10, -10);
    lv_obj_set_style_bg_color(btn_clear, COLOR_DANGER, 0);
    lv_obj_add_event_cb(btn_clear, btn_clear_test_cb, LV_EVENT_CLICKED, this);
    lv_obj_t * lbl_clr = lv_label_create(btn_clear);
    lv_label_set_text(lbl_clr, "CLEAR"); lv_obj_center(lbl_clr);

    lv_obj_t * btn_test_measure = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btn_test_measure, 85, 35);
    lv_obj_align(btn_test_measure, LV_ALIGN_BOTTOM_RIGHT, -5, -10);
    lv_obj_set_style_bg_color(btn_test_measure, COLOR_BTN_TEST, 0);
    lv_obj_add_event_cb(btn_test_measure, btn_measure_cb, LV_EVENT_CLICKED, this);
    lv_obj_t * lbl_msr = lv_label_create(btn_test_measure);
    lv_label_set_text(lbl_msr, "MEASURE"); lv_obj_center(lbl_msr);
}

void UIManager::updateTestStats(float brix, int count, float avgBrix) {
    if (current_mode != MODE_TEST) return;
    String bStr = String(brix, 1);
    lv_label_set_text_fmt(label_test_sugar, "%s Brix", bStr.c_str());
}

// ✅ 反转录入表格 (头插法) + 自动计算成熟度百分比
void UIManager::addTestHistory(int index, float brix, float avg) {
    if (current_mode != MODE_TEST) return;
    
    // 以 12.5 为基准计算成熟度
    int mat = round((brix / 12.5) * 100);
    int avg_mat = round((avg / 12.5) * 100);

    uint16_t row_cnt = lv_table_get_row_cnt(table_test_history);
    lv_table_set_row_cnt(table_test_history, row_cnt + 1); // 增加一行

    // 把现有的所有数据全部往下挪一行
    for(int r = row_cnt; r > 1; r--) {
        for(int c = 0; c < 5; c++) {
            lv_table_set_cell_value(table_test_history, r, c, lv_table_get_cell_value(table_test_history, r - 1, c));
        }
    }

    // 把新数据插在第 1 行 (紧贴表头)
    String bStr = String(brix, 1);
    String aStr = String(avg, 1);
    lv_table_set_cell_value_fmt(table_test_history, 1, 0, "%d", index);
    lv_table_set_cell_value_fmt(table_test_history, 1, 1, "%s", bStr.c_str());
    lv_table_set_cell_value_fmt(table_test_history, 1, 2, "%d", mat);
    lv_table_set_cell_value_fmt(table_test_history, 1, 3, "%s", aStr.c_str());
    lv_table_set_cell_value_fmt(table_test_history, 1, 4, "%d", avg_mat);
}

void UIManager::clearTestTable() {
    lv_table_set_row_cnt(table_test_history, 1); // 只保留表头
    lv_label_set_text(label_test_sugar, "--.- Brix");
}

// ==========================================
// 3. 工业模式 UI (双页滑动，带上传表格)
// ==========================================
void UIManager::buildIndustrialUI() {
    // 1. 初始化模式标记
    current_mode = MODE_INDUSTRIAL;
    // 2. 暴力清空当前屏幕上的所有控件，防止内存泄漏和旧控件残留
    lv_obj_clean(lv_scr_act()); 
    // 3. 设置整个屏幕的背景颜色为浅灰色 (工业风底色)
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0xf4f7f6), 0);
    // --- 创建滑动翻页容器 (Tileview) ---
    // 这是一个可以左右滑动的容器，用来放"第一页(数据)"和"第二页(历史表格)"
    lv_obj_t * tv = lv_tileview_create(lv_scr_act());
    // 设置 Tileview 的大小：宽240像素，高260像素 (占满屏幕大部分区域)
    lv_obj_set_size(tv, 240, 260); 
    // 【关键】定位 Tileview：顶部居中对齐，X轴偏移0，Y轴偏移0
    // 如果你想让整体内容上移，把最后一个 0 改成负数，例如 -30
    lv_obj_align(tv, LV_ALIGN_TOP_MID, 0, 0);
    // 向 Tileview 中添加两个"页面"(Tile)
    lv_obj_t * tile1 = lv_tileview_add_tile(tv, 0, 0, LV_DIR_RIGHT); // 第一页：可以向右滑到第二页
    lv_obj_t * tile2 = lv_tileview_add_tile(tv, 1, 0, LV_DIR_LEFT);  // 第二页：可以向左滑回第一页

    // ==========================================
    // --- 工业第一页 (数据展示页) ---
    // ==========================================
    
    // 设置第一页的布局方式：垂直流式布局 (Flex Column)
    // 意思是：添加到这个页面里的控件，会自动从上往下依次排列
    lv_obj_set_flex_flow(tile1, LV_FLEX_FLOW_COLUMN);
    
    // 设置 Flex 对齐方式：
    // 1. LV_FLEX_ALIGN_START: 主轴(垂直方向)从顶部开始排列
    // 2. LV_FLEX_ALIGN_CENTER: 侧轴(水平方向)居中对齐
    // 3. LV_FLEX_ALIGN_CENTER: 多行对齐方式(这里用不到)
    lv_obj_set_flex_align(tile1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 设置第一页的内边距：所有方向都是5像素 (控件不会贴着边缘)
    lv_obj_set_style_pad_all(tile1, 5, 0);

// ==========================================
    // --- 1. 顶部状态栏容器 (隐形包装盒，用于左右排版) ---
    // ==========================================
    lv_obj_t * header_row = lv_obj_create(tile1);
    lv_obj_set_size(header_row, 230, 20); // 宽度 230，高度 20
    lv_obj_set_style_bg_opa(header_row, 0, 0); // 背景完全透明
    lv_obj_set_style_border_width(header_row, 0, 0); // 去除边框
    lv_obj_set_style_pad_all(header_row, 0, 0); // 去除内边距

    // --- 1.1 左侧：状态/时间标签 ---
    label_status = lv_label_create(header_row); // 注意：父对象改成了 header_row
    lv_label_set_text(label_status, "Initializing...");
    lv_obj_set_style_text_font(label_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(label_status, COLOR_BTN_TEST, 0);
    lv_obj_align(label_status, LV_ALIGN_LEFT_MID, 0, 0); // 绝对定位：靠左垂直居中

    // --- 1.2 右侧：WiFi状态指示器 ---
    label_wifi = lv_label_create(header_row); // 注意：父对象改成了 header_row
    lv_label_set_text(label_wifi, "WiFi: --");
    lv_obj_set_style_text_color(label_wifi, COLOR_GREY, 0);
    lv_obj_set_style_text_font(label_wifi, &lv_font_montserrat_12, 0);
    lv_obj_align(label_wifi, LV_ALIGN_RIGHT_MID, 0, 0); // 绝对定位：靠右垂直居中

    // --- 2. 设备身份信息区 (ID, Token, Variety) ---
    // 创建一个容器(面板)，用来包裹这三行信息，方便统一设置背景色和内边距
    lv_obj_t * id_panel = lv_obj_create(tile1);
    
    // 设置面板宽度为230像素 (占满页面宽度)
    lv_obj_set_width(id_panel, 230);

    lv_obj_set_size(id_panel, 230, 80); 
    
    // 设置面板内边距：8像素 (文字不会贴着面板边框)
    lv_obj_set_style_pad_all(id_panel, 8, 0);
    
    // 设置面板背景色：淡蓝色 (区分身份区和数据区)
    lv_obj_set_style_bg_color(id_panel, lv_color_hex(0xeaf2f8), 0);
    
    // 设置面板边框宽度：0 (隐藏边框，只要背景色)
    lv_obj_set_style_border_width(id_panel, 0, 0);
    
    // 在面板内部创建真正显示文字的标签
    lv_obj_t * dev_label = lv_label_create(id_panel);
    
    // 设置字体：20号大字体 (醒目)
    lv_obj_set_style_text_font(dev_label, &lv_font_montserrat_20, 0);
    
    // 设置文字颜色：深灰色 (专业、不刺眼)
    lv_obj_set_style_text_color(dev_label, lv_color_hex(0x2c3e50), 0);
    
    // 设置文字内容：使用 \n 换行，一次性显示 ID、TK、Vr 三行
    lv_label_set_text_fmt(dev_label, "ID: %s\nTK: %s\nVr: %s", DEVICE_ID, DEVICE_TOKEN, FRUIT_VARIETY);
    
    // 将这个文字标签在面板中居中显示
    lv_obj_center(dev_label);

    // --- 3. 传感器综合数据区 (白色卡片) ---
    // 再创建一个容器(白色卡片)，包裹所有传感器数据
    lv_obj_t * data_panel = lv_obj_create(tile1);
    
    // 设置卡片宽度：230像素
    lv_obj_set_width(data_panel, 230);
    
    // 设置卡片内部布局：垂直流式布局 (数据从上往下排)
    lv_obj_set_flex_flow(data_panel, LV_FLEX_FLOW_COLUMN);
    
    // 设置卡片内边距：10像素
    lv_obj_set_style_pad_all(data_panel, 10, 0);
    
    // 设置卡片背景色：纯白色
    lv_obj_set_style_bg_color(data_panel, lv_color_hex(0xffffff), 0);
    
    // 设置卡片边框宽度：1像素 (显示边框)
    lv_obj_set_style_border_width(data_panel, 1, 0);
    
    // 设置卡片边框颜色：浅灰色
    lv_obj_set_style_border_color(data_panel, lv_color_hex(0xbdc3c7), 0);
    
    // --- 在白色卡片里创建各个数据行 ---
    // 糖度行 (最醒目，红色)
    label_sugar = lv_label_create(data_panel); 
    lv_obj_set_style_text_color(label_sugar, COLOR_DANGER, 0); // 红色文字
    lv_obj_set_style_text_font(label_sugar, &lv_font_montserrat_16, 0); // 16号字体
    lv_label_set_text(label_sugar, "Sugar: -- Brix"); // 默认文字

    // ✅ 新增：在糖度之上显示新鲜度/气体浓度
    label_gas = lv_label_create(data_panel);
    lv_obj_set_style_text_color(label_gas, lv_color_hex(0x2980b9), 0); // 蓝色
    lv_obj_set_style_text_font(label_gas, &lv_font_montserrat_16, 0); // 16号字体
    lv_label_set_text(label_gas, "Fresh: --");

    // 温度行
    label_temp = lv_label_create(data_panel); 
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_16, 0); // 16号字体
    lv_label_set_text(label_temp, "Temp: -- C"); // 默认文字

    // 湿度行
    label_hum = lv_label_create(data_panel); 
    lv_obj_set_style_text_font(label_hum, &lv_font_montserrat_16, 0); // 16号字体
    lv_label_set_text(label_hum, "Hum: -- %"); // 默认文字

    // 光照行
    label_light = lv_label_create(data_panel); 
    lv_obj_set_style_text_font(label_light, &lv_font_montserrat_16, 0); // 16号字体
    lv_label_set_text(label_light, "Light: -- Lux"); // 默认文字

    // 光谱数据块 (多行显示，字号较小)
    label_spectrum = lv_label_create(data_panel);
    
    // 设置长文本模式：自动换行 (如果光谱数据太长，会自动换到下一行)
    lv_label_set_long_mode(label_spectrum, LV_LABEL_LONG_WRAP);
    
    // 设置标签宽度：200像素 (超过这个宽度就换行)
    lv_obj_set_width(label_spectrum, 200);
    
    // 设置字体：12号小字体 (节省空间)
    lv_obj_set_style_text_font(label_spectrum, &lv_font_montserrat_12, 0);
    
    // 设置文字颜色：灰色 (不抢眼，作为辅助数据)
    lv_obj_set_style_text_color(label_spectrum, lv_color_hex(0x7f8c8d), 0);
    
    // 设置默认文字
    lv_label_set_text(label_spectrum, "[Spectrum]\nWaiting...");

    // ==========================================
    // --- 工业第二页 (历史数据表格页) ---
    // ==========================================
    
    // 设置第二页布局：垂直流式布局
    lv_obj_set_flex_flow(tile2, LV_FLEX_FLOW_COLUMN);
    
    // 创建标题 "Upload History"
    lv_obj_t * tbl_title = lv_label_create(tile2);
    lv_label_set_text(tbl_title, "Upload History");

    // 创建历史记录表格
    table_ind_history = lv_table_create(tile2);
    
    // 设置表格宽度：230像素
    lv_obj_set_width(table_ind_history, 230); 
    
    // 设置表格单元格字体：12号小字体 (挤下更多列)
    lv_obj_set_style_text_font(table_ind_history, &lv_font_montserrat_12, LV_PART_ITEMS);
    
    // 设置表格列数：5列
    lv_table_set_col_cnt(table_ind_history, 5); 
    
    // 设置每一列的宽度 (像素)
    lv_table_set_col_width(table_ind_history, 0, 70);  // 第1列：Time
    lv_table_set_col_width(table_ind_history, 1, 46);  // 第2列：Sgr (糖度)
    lv_table_set_col_width(table_ind_history, 2, 42);  // 第3列：T (温度)
    lv_table_set_col_width(table_ind_history, 3, 42);  // 第4列：H (湿度)
    lv_table_set_col_width(table_ind_history, 4, 45);  // 第5列：UP (上传状态)
    
    // 填充表头文字 (第0行)
    lv_table_set_cell_value(table_ind_history, 0, 0, "Time");
    lv_table_set_cell_value(table_ind_history, 0, 1, "Sgr");
    lv_table_set_cell_value(table_ind_history, 0, 2, "T");
    lv_table_set_cell_value(table_ind_history, 0, 3, "H");
    lv_table_set_cell_value(table_ind_history, 0, 4, "UP");

    // ==========================================
    // --- 屏幕底部固定按钮区 (不在 Tileview 里，固定在屏幕最下方) ---
    // ==========================================
    
    // 1. HOME 按钮 (左下角)
    lv_obj_t * btn_home = lv_btn_create(lv_scr_act()); // 直接在屏幕上创建，不在 Tileview 里
    lv_obj_set_size(btn_home, 60, 35); // 按钮大小
    // 按钮位置：左下角对齐，X轴右移5px，Y轴上移15px
    lv_obj_align(btn_home, LV_ALIGN_BOTTOM_LEFT, 5, -15);
    lv_obj_set_style_bg_color(btn_home, COLOR_GREY, 0); // 灰色背景
    lv_obj_add_event_cb(btn_home, btn_home_cb, LV_EVENT_CLICKED, this); // 绑定点击事件
    lv_obj_t * lbl_home = lv_label_create(btn_home); // 按钮上的文字
    lv_label_set_text(lbl_home, "HOME"); 
    lv_obj_center(lbl_home); // 文字在按钮中居中

    // 2. MEASURE 按钮 (底部中间)
    btn_measure = lv_btn_create(lv_scr_act());
    // 按钮位置：底部中间对齐，X轴右移15px，Y轴上移15px
    lv_obj_align(btn_measure, LV_ALIGN_BOTTOM_MID, 15, -15); 
    lv_obj_set_size(btn_measure, 120, 35); // 按钮大小
    lv_obj_set_style_bg_color(btn_measure, COLOR_BTN_IND, 0); // 橙色背景
    lv_obj_add_event_cb(btn_measure, btn_measure_cb, LV_EVENT_CLICKED, this); // 绑定点击事件
    lv_obj_t * btn_label = lv_label_create(btn_measure); // 按钮上的文字
    lv_label_set_text(btn_label, "MEASURE"); 
    lv_obj_center(btn_label); // 文字居中

    // 3. 倒计时标签 (右下角)
    label_countdown = lv_label_create(lv_scr_act());
    // 标签位置：右下角对齐，X轴左移70px，Y轴上移50px
    lv_obj_align(label_countdown, LV_ALIGN_BOTTOM_RIGHT, -70, -50); 
    lv_obj_set_style_text_font(label_countdown, &lv_font_montserrat_12, 0); // 12号字体
    lv_label_set_text(label_countdown, "Auto:00:00"); // 默认文字
}

// ==========================================
// 更新函数 (确保字符串转换格式正确)
// ==========================================
void UIManager::updateEnv(float t, float h, int l, float g135, float g137) {
    if (current_mode != MODE_INDUSTRIAL) return;
    
    // 基础环境数据转换
    String tStr = String(t, 1); 
    String hStr = String(h, 1);
    lv_label_set_text_fmt(label_temp, "Temp: %s C", tStr.c_str());
    lv_label_set_text_fmt(label_hum, "Hum: %s %%", hStr.c_str());
    lv_label_set_text_fmt(label_light, "Light: %d Lux", l);
    
    // ✅ 修复气体浮点数显示问题
    if (g135 != 99.0 || g137 != 99.0) {
        String g135Str = String(g135, 1); // 保留1位小数
        String g137Str = String(g137, 1);
        lv_label_set_text_fmt(label_gas, "Fresh: %s / %s", g135Str.c_str(), g137Str.c_str());
    } else {
        lv_label_set_text(label_gas, "Fresh: N/A"); 
    }
}

void UIManager::updateSugar(float brix) {
    if (current_mode != MODE_INDUSTRIAL) return;
    String bStr = String(brix, 1);
    lv_label_set_text_fmt(label_sugar, "Sugar: %s Brix", bStr.c_str());
}

void UIManager::updateSpectrum(uint16_t c415, uint16_t c445, uint16_t c480, uint16_t c515, uint16_t c555, uint16_t c595, uint16_t c640, uint16_t c680, uint16_t clr, uint16_t nir) {
    if (current_mode != MODE_INDUSTRIAL) return;
    lv_label_set_text_fmt(label_spectrum, "415:%d  445:%d  480:%d  515:%d\n555:%d  595:%d  640:%d  680:%d\nCLR:%d  NIR:%d",
        c415, c445, c480, c515, c555, c595, c640, c680, clr, nir);
}

void UIManager::updateStatus(const char* msg) {
    if (current_mode == MODE_INDUSTRIAL) {
        lv_label_set_text(label_status, msg);
    } else if (current_mode == MODE_WAITING) {
        lv_label_set_text(boot_status_label, msg);
    }
}

// ✅ WiFi状态更新函数
void UIManager::updateWifiStatus(bool connected) {
    if (current_mode != MODE_INDUSTRIAL || !label_wifi) return;
    if (connected) {
        lv_label_set_text(label_wifi, "WiFi: OK");
        lv_obj_set_style_text_color(label_wifi, lv_color_hex(0x27ae60), 0); // 绿色
    } else {
        lv_label_set_text(label_wifi, "WiFi: X");
        lv_obj_set_style_text_color(label_wifi, COLOR_DANGER, 0); // 红色
    }
}

void UIManager::updateCountdown(uint32_t remain_ms, uint32_t total_ms) {
    if (current_mode != MODE_INDUSTRIAL) return;
    uint32_t r_sec = remain_ms / 1000;
    char buf[32];
    sprintf(buf, "Auto: %02d:%02d", (r_sec % 3600) / 60, (r_sec % 60));
    lv_label_set_text(label_countdown, buf);
}

// 追加工业历史表格数据
// ✅ 修复：绕过 LVGL 不支持 %f 的缺陷，使用 String 强转
void UIManager::addIndustrialHistory(const char* timeStr, float brix, float temp, float hum, int light, bool uploaded) {
    if (current_mode != MODE_INDUSTRIAL) return;
    uint16_t row = lv_table_get_row_cnt(table_ind_history);
    
    String ts(timeStr); String shortTime = ts.substring(11, 16); 
    String bStr = String(brix, 1);
    
    lv_table_set_cell_value(table_ind_history, row, 0, shortTime.c_str());
    lv_table_set_cell_value(table_ind_history, row, 1, bStr.c_str());
    
    if (temp == 99.0) lv_table_set_cell_value(table_ind_history, row, 2, "--");
    else { String tStr = String(temp, 0); lv_table_set_cell_value(table_ind_history, row, 2, tStr.c_str()); }
    
    if (hum == 99.0) lv_table_set_cell_value(table_ind_history, row, 3, "--");
    else { String hStr = String(hum, 0); lv_table_set_cell_value(table_ind_history, row, 3, hStr.c_str()); }
    
    lv_table_set_cell_value(table_ind_history, row, 4, uploaded ? "Y" : "N");
}

void UIManager::markIndustrialHistoryUploaded(const char* timeStr) {
    if (current_mode != MODE_INDUSTRIAL || !table_ind_history) return;

    // 只截取时间部分的 hh:mm
    String ts(timeStr);
    String targetTime = ts.substring(11, 16); 
    
    uint16_t row_cnt = lv_table_get_row_cnt(table_ind_history);
    
    // 从第 1 行开始遍历（第 0 行是表头）
    for (uint16_t i = 1; i < row_cnt; i++) {
        const char* cell_time = lv_table_get_cell_value(table_ind_history, i, 0);
        
        // 如果时间匹配上了，就把第 4 列(UP)的值改成 'Y'
        if (cell_time != nullptr && String(cell_time) == targetTime) {
            lv_table_set_cell_value(table_ind_history, i, 4, "Y");
            // 只要找到一个匹配的就退出循环，因为同一分钟可能有多条，我们只标记其中一条
            break; 
        }
    }
}

// --- 事件代理分发 ---
void UIManager::btn_measure_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui->onMeasure) ui->onMeasure(); 
}
void UIManager::btn_mode_test_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui->onModeSelect) ui->onModeSelect(MODE_TEST); 
}
void UIManager::btn_mode_ind_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui->onModeSelect) ui->onModeSelect(MODE_INDUSTRIAL); 
}
void UIManager::btn_clear_test_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui->onClearTest) ui->onClearTest(); 
}
void UIManager::btn_home_cb(lv_event_t * e) {
    UIManager* ui = (UIManager*)lv_event_get_user_data(e);
    if (ui->onReturnHome) ui->onReturnHome(); 
}

