#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

class StorageManager {
public:
    static StorageManager& getInstance() {
        static StorageManager instance;
        return instance;
    }

    void begin();
    
    // 1. 存入一条新数据（is_uploaded: Y/N）
    void saveRecord(uint32_t ts, float brix, float temp, float hum, int light, float gas135, float gas137, const JsonObject& spec, bool is_uploaded);
    
    // 2. 检查存储容量，如果剩余空间不足 20%，删除最早的一半数据
    void checkAndCleanCapacity();

    // 3. 获取一条未上传的数据，返回成功与否，并将 JSON 对象填充
    bool getUnuploadedRecord(JsonObject& doc, uint32_t& out_ts);

    // 4. 将指定时间戳的数据标记为 "已上传"
    void markAsUploaded(uint32_t target_ts);

private:
    StorageManager() = default;
    const char* FILE_PATH = "/records.jsonl";
};