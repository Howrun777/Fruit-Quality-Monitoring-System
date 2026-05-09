#include "HttpClient.h"
#include "../config.h"
#include <WiFi.h>
#include <HTTPClient.h>

unsigned long NetworkManager::last_reconnect_attempt = 0;

bool NetworkManager::connectWiFi() {
    Serial.printf("Connecting to %s ", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) { 
        delay(500); 
        Serial.print(".");
        retries++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected! IP: " + WiFi.localIP().toString());
        return true;
    } else {
        Serial.println("\nWiFi Failed. Entering Offline Mode.");
        return false;
    }
}

void NetworkManager::maintainWiFi() {
    if (WiFi.status() != WL_CONNECTED) {
        unsigned long now = millis();
        if (now - last_reconnect_attempt > 30000) {
            last_reconnect_attempt = now;
            Serial.println("WiFi disconnected. Attempting to reconnect...");
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASS);
        }
    }
}

uint32_t NetworkManager::fetchServerTime() {
    if (WiFi.status() != WL_CONNECTED) return 0;
    
    // ✅ 强行切断底层阻塞
    WiFiClient client;
    client.setTimeout(3); 

    HTTPClient http;
    String url = String(SERVER_URL);
    url.replace("/device/upload", "/device/time"); 
    http.begin(client, url); // 传入 Client
    http.setTimeout(3000); 
    
    int httpResponseCode = http.GET();
    uint32_t s_time = 0;
    
    if (httpResponseCode == 200) {
        String responseStr = http.getString();
        StaticJsonDocument<512> resDoc;
        if (!deserializeJson(resDoc, responseStr)) {
            s_time = resDoc["data"]["server_time"].as<uint32_t>();
        }
    }
    http.end();
    return s_time;
}

bool NetworkManager::uploadData(const JsonDocument& payload, String& outMsg, uint32_t* outServerTime) {
    if (WiFi.status() != WL_CONNECTED) {
        outMsg = "WiFi Offline (Saved locally)"; 
        return false;
    }
    
    // ✅ 强行切断底层阻塞
    WiFiClient client;
    client.setTimeout(3);

    HTTPClient http;
    http.begin(client, SERVER_URL); // 传入 Client
    http.setTimeout(3000); 
    
    http.addHeader("Content-Type", "application/json");
    http.addHeader("device_id", DEVICE_ID);
    http.addHeader("token", DEVICE_TOKEN);

    String requestBody;
    serializeJson(payload, requestBody);

    int httpResponseCode = http.POST(requestBody);
    
    if (httpResponseCode > 0) {
        String responseStr = http.getString();
        StaticJsonDocument<512> resDoc;
        deserializeJson(resDoc, responseStr);
        
        outMsg = resDoc["msg"].as<String>();
        if (outServerTime != nullptr && resDoc["data"].containsKey("server_time")) {
            *outServerTime = resDoc["data"]["server_time"].as<uint32_t>();
        }
        
        http.end();
        return httpResponseCode == 200;
    } else {
        outMsg = "HTTP Error: " + String(httpResponseCode);
        http.end();
        return false;
    }
}