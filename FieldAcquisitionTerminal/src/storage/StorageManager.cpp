#include "StorageManager.h"
#include <LittleFS.h>

void StorageManager::begin() {
    // 尝试挂载 LittleFS，如果失败则强制格式化 (true)
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed!");
        return;
    }
    Serial.println("LittleFS Mounted Successfully.");

    // 👇✅ 临时加上这一行：开机强制删除积压的历史数据文件！
    //LittleFS.remove("/records.jsonl"); 
    //Serial.println("OLD DATA CLEARED!"); // 打印一个提示

    checkAndCleanCapacity(); // 开机先检查一下容量
}

void StorageManager::saveRecord(uint32_t ts, float brix, float temp, float hum, int light, float gas135, float gas137, const JsonObject& spec, bool is_uploaded) {
    File file = LittleFS.open(FILE_PATH, FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open file for appending");
        return;
    }

    // 序列化为单行 JSON
    StaticJsonDocument<512> doc;
    doc["ts"] = ts;
    doc["brix"] = brix;
    doc["t"] = temp;
    doc["h"] = hum;
    doc["l"] = light;
    doc["gas135"] = gas135;
    doc["gas137"] = gas137;
    doc["up"] = is_uploaded ? 1 : 0; // 1=Y, 0=N
    doc["spec"] = spec;

    serializeJson(doc, file);
    file.print("\n"); // 换行符极其重要 (JSON Lines 格式)
    file.close();
    
    Serial.printf("Record saved to disk. Uploaded: %s\n", is_uploaded ? "Y" : "N");
}

void StorageManager::checkAndCleanCapacity() {
    size_t total = LittleFS.totalBytes();
    size_t used = LittleFS.usedBytes();
    
    // 如果总容量为 0，说明文件系统有问题
    if (total == 0) return;

    // 如果已用空间大于 80% (即剩余不到 20%)
    if (used > total * 0.8) {
        Serial.println("Storage almost full! Triggering 50% cleanup...");
        
        File file = LittleFS.open(FILE_PATH, FILE_READ);
        if (!file) return;

        // 1. 先统计一共有多少行数据
        int totalLines = 0;
        while (file.available()) {
            if (file.read() == '\n') totalLines++;
        }
        
        // 2. 准备跳过前一半的老数据
        int linesToSkip = totalLines / 2;
        file.seek(0); // 回到文件头
        
        // 3. 打开一个临时文件准备写入后一半的新数据
        File tempFile = LittleFS.open("/temp.jsonl", FILE_WRITE);
        int currentLine = 0;
        
        while (file.available()) {
            String line = file.readStringUntil('\n');
            if (currentLine >= linesToSkip && line.length() > 0) {
                tempFile.print(line + "\n");
            }
            currentLine++;
        }
        
        file.close();
        tempFile.close();
        
        // 4. 删除旧文件，将临时文件重命名为正式文件
        LittleFS.remove(FILE_PATH);
        LittleFS.rename("/temp.jsonl", FILE_PATH);
        
        Serial.println("Cleanup complete. 50% of oldest history deleted.");
    }
}

bool StorageManager::getUnuploadedRecord(JsonObject& doc, uint32_t& out_ts) {
    File file = LittleFS.open(FILE_PATH, FILE_READ);
    if (!file) return false;

    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.length() < 10) continue;

        StaticJsonDocument<512> tempDoc;
        DeserializationError err = deserializeJson(tempDoc, line);
        if (err) continue;

        // 找到第一条未上传(up=0)的数据
        if (tempDoc["up"].as<int>() == 0) {
            out_ts = tempDoc["ts"].as<uint32_t>();
            doc.set(tempDoc.as<JsonObject>()); 
            file.close();
            return true;
        }
    }
    file.close();
    return false; // 全部都已上传了
}

void StorageManager::markAsUploaded(uint32_t target_ts) {
    File file = LittleFS.open(FILE_PATH, FILE_READ);
    if (!file) return;

    File tempFile = LittleFS.open("/temp.jsonl", FILE_WRITE);
    if (!tempFile) { file.close(); return; }

    bool found = false;
    while (file.available()) {
        String line = file.readStringUntil('\n');
        if (line.length() < 10) continue;

        if (!found) {
            StaticJsonDocument<512> tempDoc;
            if (!deserializeJson(tempDoc, line)) {
                // 👇✅ 核心修复：必须同时满足“时间戳匹配”且“原来状态是未上传(up == 0)”
                if (tempDoc["ts"].as<uint32_t>() == target_ts && tempDoc["up"].as<int>() == 0) {
                    tempDoc["up"] = 1;
                    String newLine;
                    serializeJson(tempDoc, newLine);
                    tempFile.print(newLine + "\n");
                    found = true;
                    continue; // 修改完这一行，直接跳过原样写入
                }
            }
        }
        // 如果没匹配上，或者是已经找到过了，就原样写回
        tempFile.print(line + "\n");
    }

    file.close();
    tempFile.close();
    
    // 如果修改成功，替换文件
    if (found) {
        LittleFS.remove(FILE_PATH);
        LittleFS.rename("/temp.jsonl", FILE_PATH);
        Serial.printf("Record %u marked as uploaded.\n", target_ts);
    } else {
        LittleFS.remove("/temp.jsonl"); // 没找到就删掉临时文件
    }
}