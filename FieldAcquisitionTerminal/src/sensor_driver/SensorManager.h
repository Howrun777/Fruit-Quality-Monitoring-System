#pragma once
#include <Arduino.h>
#include <Adafruit_AS7341.h>
#include <Adafruit_SHT31.h>
#include <BH1750.h>
#include <ArduinoJson.h>

class SensorManager {
public:
    void begin();
    // 必须放在 loop() 中高频调用，随时接收 Arduino 发来的 JSON
    void loop(); 
    // 为包含气体的环境读取接口
    void readEnvironment(float &temp, float &hum, int &light, float &gas135, float &gas137);
    bool readSpectrum(JsonObject& doc);

private:
    Adafruit_AS7341 as7341;
    Adafruit_SHT31 sht31;
    BH1750 bh1750;
    bool has_as7341 = false;
    bool has_sht31 = false;
    bool has_bh1750 = false;

    // 气敏数据缓存
    float latest_gas135 = 99.0f; // 默认99表示未检测到
    float latest_gas137 = 99.0f;
    String gas_serial_buffer = "";
};