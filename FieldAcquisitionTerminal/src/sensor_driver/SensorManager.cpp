#include "SensorManager.h"
#include "../config.h"

void SensorManager::begin() {
    
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
    //pinMode(LED_PIN, OUTPUT); digitalWrite(LED_PIN, LOW); 

    // ✅ 强制绑定串口 2 到指定的 RX 和 TX 引脚
    Serial2.begin(GAS_BAUD_RATE, SERIAL_8N1, GAS_RX_PIN, GAS_TX_PIN);
    // 加个延迟确保串口稳定
    delay(50);
    Serial.printf("Serial2 Started for Arduino. RX:%d, TX:%d, Baud:%d\n", GAS_RX_PIN, GAS_TX_PIN, GAS_BAUD_RATE);

    if (sht31.begin(0x44)) { has_sht31 = true; } else { Serial.println("SHT31 Error"); }
    if (bh1750.begin()) { has_bh1750 = true; } else { Serial.println("BH1750 Error"); }
    if (as7341.begin()) {
        has_as7341 = true; as7341.setATIME(50); 
        as7341.setASTEP(999); 
        as7341.setGain(AS7341_GAIN_256X);
    } else { Serial.println("AS7341 Error"); }
}

// ✅ 修复：自定义协议字符串解析器 (放弃 JSON 解析)
void SensorManager::loop() {
    while (Serial2.available() > 0) {
        char c = Serial2.read();
        
        // 遇到换行符，说明收到了一整条 Arduino 的数据
        if (c == '\n') { 
            // 如果数据以 "DATA:" 开头，说明是合法的传感器数据包
            if (gas_serial_buffer.startsWith("DATA:")) {
                
                // 寻找 G135 的数值
                int g135_idx = gas_serial_buffer.indexOf("G135:");
                if (g135_idx != -1) {
                    int comma_idx = gas_serial_buffer.indexOf(",", g135_idx);
                    if (comma_idx != -1) {
                        String g135_str = gas_serial_buffer.substring(g135_idx + 5, comma_idx);
                        latest_gas135 = g135_str.toFloat();
                    }
                }

                // 寻找 G137 的数值
                int g137_idx = gas_serial_buffer.indexOf("G137:");
                if (g137_idx != -1) {
                    int comma_idx = gas_serial_buffer.indexOf(",", g137_idx);
                    if (comma_idx != -1) {
                        String g137_str = gas_serial_buffer.substring(g137_idx + 5, comma_idx);
                        latest_gas137 = g137_str.toFloat();
                    }
                }
                
                Serial.printf("[Arduino] RAW: [%s]\n", gas_serial_buffer.c_str());
                Serial.printf("[Arduino] Parsed OK! GAS135: %.1f, GAS137: %.1f\n", latest_gas135, latest_gas137);
            }
            
            // 清空缓冲区，准备接下一包
            gas_serial_buffer = ""; 
        } 
        else if (c != '\r') { // 忽略 \r 字符
            gas_serial_buffer += c;
            
            // 防爆内存保护
            if (gas_serial_buffer.length() > 200) {
                gas_serial_buffer = "";
            }
        }
    }
}
// 返回环境数据时把气体数据带上
void SensorManager::readEnvironment(float &temp, float &hum, int &light, float &gas135, float &gas137) {
    if (has_sht31) {
        temp = sht31.readTemperature(); hum = sht31.readHumidity();
        if (isnan(temp)) temp = 99.0; if (isnan(hum)) hum = 99.0;
    } else { temp = 99.0; hum = 99.0; }

    if (has_bh1750) {
        light = bh1750.readLightLevel();
        if (light < 0) light = 99;
    } else { light = 99; }

    // 取出最新的气体数据（如果 Arduino 没发，这里就是 99.0）
    gas135 = latest_gas135;
    gas137 = latest_gas137;
}


bool SensorManager::readSpectrum(JsonObject& doc) {
    if (!has_as7341) return false;
    if (!as7341.readAllChannels()) return false;
    
    doc["ch415"] = as7341.getChannel(AS7341_CHANNEL_415nm_F1);
    doc["ch445"] = as7341.getChannel(AS7341_CHANNEL_445nm_F2);
    doc["ch480"] = as7341.getChannel(AS7341_CHANNEL_480nm_F3);
    doc["ch515"] = as7341.getChannel(AS7341_CHANNEL_515nm_F4);
    doc["ch555"] = as7341.getChannel(AS7341_CHANNEL_555nm_F5);
    doc["ch595"] = as7341.getChannel(AS7341_CHANNEL_590nm_F6);
    doc["ch640"] = as7341.getChannel(AS7341_CHANNEL_630nm_F7);
    doc["ch680"] = as7341.getChannel(AS7341_CHANNEL_680nm_F8);
    doc["chClear"] = as7341.getChannel(AS7341_CHANNEL_CLEAR);
    doc["chNIR"] = as7341.getChannel(AS7341_CHANNEL_NIR);
    return true;
}