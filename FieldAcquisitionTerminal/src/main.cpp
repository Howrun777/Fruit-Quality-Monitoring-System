#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <WiFi.h>

#include "../config.h"

#include "sensor_driver/SensorManager.h"
#include "sensor_driver/RTCManager.h"
#include "sensor_driver/BuzzerManager.h"
#include "sensor_driver/FPGAManager.h"
#include "storage/StorageManager.h"
#include "network/HttpClient.h"
#include "display/lvgl_ui.h"
#include "algorithm/sugar_calc.h"

TFT_eSPI tft = TFT_eSPI();
SensorManager sensorMgr;
UIManager ui;

SPIClass touchSPI(HSPI); 
XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

static const uint16_t screenWidth  = 240;
static const uint16_t screenHeight = 320;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];

unsigned long lastEnvUpdate = 0;
unsigned long lastAutoMeasure = 0; 
SystemMode sysMode = MODE_WAITING;

// 测试模式本地内存数据
float test_sugar_sum = 0;
int test_count = 0;
#define MAX_TEST_COUNT 20

// 错峰补发
bool wasWiFiConnected = false;
bool isRecoveringData = false;
unsigned long recoverStartTime = 0;
unsigned long recoverDelayDelay = 0; 

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1); uint32_t h = (area->y2 - area->y1 + 1);
    tft.startWrite(); tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors((uint16_t *)&color_p->full, w * h, true); tft.endWrite();
    lv_disp_flush_ready(disp);
}

void my_touch_read(lv_indev_drv_t * indrv, lv_indev_data_t * data) {
    if (ts.tirqTouched() && ts.touched()) {
        TS_Point p = ts.getPoint(); 
        data->state = LV_INDEV_STATE_PR;
        data->point.x = map(p.x, 250, 3800, 0, screenWidth);
        data->point.y = map(p.y, 250, 3800, 0, screenHeight);
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void onClearTestClicked() {
    test_sugar_sum = 0;
    test_count = 0;
    ui.clearTestTable();
}

void onModeSelected(SystemMode mode) {
    sysMode = mode;
    if (mode == MODE_TEST) {
        ui.buildTestUI();
    } else if (mode == MODE_INDUSTRIAL) {
        ui.buildIndustrialUI();
        lastAutoMeasure = millis();
        ui.updateWifiStatus(WiFi.status() == WL_CONNECTED);
    }
}

void onReturnHomeClicked() {
    sysMode = MODE_WAITING;
    test_count = 0;      
    test_sugar_sum = 0;
    ui.showBootScreen();
    lv_timer_handler();
    if (RTCManager::getInstance().getTimestamp() > 1600000000) {
        ui.showModeSelection(); 
    }
}

// ==== 回调：统一测量引擎 ====
void onMeasureBtnClicked() {
    if (sysMode == MODE_WAITING) return;

    ui.updateStatus("Measuring..."); 
    lv_refr_now(NULL); // ✅ 安全刷新界面

    float t, h; int l; float g135, g137;
    sensorMgr.readEnvironment(t, h, l, g135, g137); 
    
    if (sysMode == MODE_TEST) { t = 99.0; h = 99.0; l = 99; g135 = 99.0; g137 = 99.0; }

    StaticJsonDocument<512> payload;
    JsonObject specObj = payload.createNestedObject("spectrum_json");
    bool specOk = sensorMgr.readSpectrum(specObj);

    if (!specOk) { 
        if (sysMode == MODE_INDUSTRIAL) ui.updateStatus("ERR: Spectrum Failed!"); 
        return; 
    }

    BuzzerManager::getInstance().beep(2000);
    float brix = SugarCalc::calculate(specObj);

    if (sysMode == MODE_TEST) {
        if (test_count >= MAX_TEST_COUNT) {
            onClearTestClicked();
        }
        test_count++;
        test_sugar_sum += brix;
        float avg = test_sugar_sum / test_count;
        ui.updateTestStats(brix, test_count, avg);
        ui.addTestHistory(test_count, brix, avg);

    } else if (sysMode == MODE_INDUSTRIAL) {
        ui.updateStatus("Uploading..."); 
        lv_refr_now(NULL); // ✅ 安全刷新界面

        ui.updateSpectrum(specObj["ch415"], specObj["ch445"], specObj["ch480"], specObj["ch515"],
                          specObj["ch555"], specObj["ch595"], specObj["ch640"], specObj["ch680"],
                          specObj["chClear"], specObj["chNIR"]);
        ui.updateSugar(brix);

        uint32_t current_ts = RTCManager::getInstance().getTimestamp();
        payload["collected_at"] = current_ts;

        // 🌟🌟🌟 这里是真正呼叫 FPGA 发送命令的地方 🌟🌟🌟
        // 只有工业模式且传感器不报错，才会完美发送到 IO5！
        FPGAManager::getInstance().triggerCapture(current_ts);

        payload["temperature"] = t; 
        payload["humidity"] = h; 
        payload["light"] = l;
        payload["gas135"] = g135;
        payload["gas137"] = g137;

        String serverMsg;
        uint32_t serverTime = 0;
        bool uploadOk = NetworkManager::uploadData(payload, serverMsg, &serverTime);
        
        StorageManager::getInstance().saveRecord(current_ts, brix, t, h, l, g135, g137, specObj, uploadOk);

        if (uploadOk) {
            ui.updateStatus("Success & Saved!");
            if (serverTime > 0) RTCManager::getInstance().syncTime(serverTime);
        } else {
            ui.updateStatus(serverMsg.c_str());
        }

        String timeStr = RTCManager::getInstance().formatTime(current_ts);
        ui.addIndustrialHistory(timeStr.c_str(), brix, t, h, l, uploadOk);
        lastAutoMeasure = millis();
    }
}

void setup() {
    Serial.begin(115200);
    BuzzerManager::getInstance().begin();
    
    // 初始化 FPGA 管理器 (此时会挂载到 IO5，并开启最大电流)
    FPGAManager::getInstance().begin();
    
    sensorMgr.begin();
    StorageManager::getInstance().begin();
    
    pinMode(TFT_BL, OUTPUT); digitalWrite(TFT_BL, HIGH); 
    tft.begin(); tft.setRotation(0); 

    touchSPI.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
    ts.begin(touchSPI); ts.setRotation(0); 
    lv_init(); lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);
    static lv_disp_drv_t disp_drv; lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = screenWidth; disp_drv.ver_res = screenHeight;
    disp_drv.flush_cb = my_disp_flush; disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    static lv_indev_drv_t indev_drv; lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER; indev_drv.read_cb = my_touch_read;
    lv_indev_drv_register(&indev_drv);

    ui.init(onMeasureBtnClicked, onModeSelected, onClearTestClicked, onReturnHomeClicked);
    lv_timer_handler();

    bool timeIsSynced = false;
    bool hasWiFi = false;
    bool hasServer = false;

    RTCManager::getInstance().begin(); 
    Wire.setTimeOut(150); // ✅ 防止 I2C 死锁

    if (RTCManager::getInstance().getTimestamp() > 1600000000) {
        timeIsSynced = true;
        BuzzerManager::getInstance().beep(2000); 
    } 

    if (!timeIsSynced) {
        if (NetworkManager::connectWiFi()) {
            hasWiFi = true; 
            ui.updateStatus("WIFI OK! Syncing Server..."); 
            lv_timer_handler();
            
            uint32_t st = NetworkManager::fetchServerTime();
            if (st > 1600000000) {
                hasServer = true;
                RTCManager::getInstance().syncTime(st);
                timeIsSynced = true;
                BuzzerManager::getInstance().beep(2000);
            }
        }
    }
    
    if (timeIsSynced) {
        ui.showModeSelection();
    } else {
        if (!hasWiFi) ui.updateStatus("Sync Failed!\nRTC: X | WIFI: X | SVR: ?");
        else if (!hasServer) ui.updateStatus("Sync Failed!\nRTC: X | WIFI: OK | SVR: X");
    }
    
    lastAutoMeasure = millis();
}

void loop() {
    sensorMgr.loop(); 
    unsigned long now = millis();

    BuzzerManager::getInstance().loop();
    NetworkManager::maintainWiFi(); 
    bool currentWiFiState = (WiFi.status() == WL_CONNECTED);
    
    if (currentWiFiState && !wasWiFiConnected) {
        isRecoveringData = true;
        recoverStartTime = now;
        recoverDelayDelay = random(0, 10000); 

        if (sysMode == MODE_WAITING && !ui.isIndustrialModeUnlocked()) {
            ui.updateStatus("WIFI Restored! Syncing SVR...");
            lv_timer_handler(); 

            uint32_t st = NetworkManager::fetchServerTime();
            if (st > 1600000000) {
                RTCManager::getInstance().syncTime(st);
                ui.showModeSelection(); 
                BuzzerManager::getInstance().beep(4000);
            } else {
                ui.updateStatus("Sync Failed!\nRTC: X | WIFI: OK | SVR: X");
            }
        }
        ui.updateWifiStatus(true);
    }
    
    if (!currentWiFiState && wasWiFiConnected) {
        ui.updateWifiStatus(false);
    }
    
    wasWiFiConnected = currentWiFiState;

    if (currentWiFiState && !isRecoveringData) {
        static unsigned long lastCheckTime = 0;
        if (now - lastCheckTime > 15000) {
            lastCheckTime = now;
            StaticJsonDocument<512> tempDoc;
            JsonObject recordObj = tempDoc.to<JsonObject>();
            uint32_t target_ts = 0;
            if (StorageManager::getInstance().getUnuploadedRecord(recordObj, target_ts)) {
                isRecoveringData = true;
                recoverStartTime = now;
                recoverDelayDelay = random(0, 10000); 
                Serial.printf("[Cloud Check] Found unsent data! Recovery starts in %lu sec...\n", recoverDelayDelay / 1000);
            }
        }
    }

    if (isRecoveringData && currentWiFiState && (now - recoverStartTime > recoverDelayDelay)) {
        StaticJsonDocument<512> tempDoc;
        JsonObject recordObj = tempDoc.to<JsonObject>();
        uint32_t target_ts = 0;
        
        if (StorageManager::getInstance().getUnuploadedRecord(recordObj, target_ts)) {
            StaticJsonDocument<512> payload;
            payload["collected_at"] = target_ts;
            payload["temperature"] = recordObj["t"];
            payload["humidity"] = recordObj["h"];
            payload["light"] = recordObj["l"];
            payload["gas135"] = recordObj["gas135"]; 
            payload["gas137"] = recordObj["gas137"];
            payload["spectrum_json"] = recordObj["spec"];

            String serverMsg;
            Serial.printf("Recovering old data (TS: %u)...\n", target_ts);
            
            if (NetworkManager::uploadData(payload, serverMsg)) {
                StorageManager::getInstance().markAsUploaded(target_ts);
                String timeStr = RTCManager::getInstance().formatTime(target_ts);
                ui.markIndustrialHistoryUploaded(timeStr.c_str());
                
                // 错峰补发休息
                recoverStartTime = millis();
                recoverDelayDelay = 1000;
            } else {
                Serial.println("Server still down. Pause recovery.");
                isRecoveringData = false; 
            }
        } else {
            isRecoveringData = false; 
            Serial.println("All old data recovered successfully!");
        }
    }

    if (sysMode == MODE_INDUSTRIAL) {
        if (now - lastEnvUpdate > ENV_UPDATE_INTERVAL) {
            lastEnvUpdate = now;
             float t, h, g135, g137; int l;
            sensorMgr.readEnvironment(t, h, l, g135, g137); 
            ui.updateEnv(t, h, l, g135, g137);
            ui.updateStatus(RTCManager::getInstance().formatTime(RTCManager::getInstance().getTimestamp()).c_str());
        }

        unsigned long elapsed = now - lastAutoMeasure;
        if (elapsed > AUTO_UPLOAD_INTERVAL) elapsed = AUTO_UPLOAD_INTERVAL;
        ui.updateCountdown(AUTO_UPLOAD_INTERVAL - elapsed, AUTO_UPLOAD_INTERVAL);

        if (elapsed >= AUTO_UPLOAD_INTERVAL) {
            onMeasureBtnClicked();
        }
    }

    lv_timer_handler();
    delay(5);
}