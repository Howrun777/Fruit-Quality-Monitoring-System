#ifndef FAST_DECAY_QUEUE_H
#define FAST_DECAY_QUEUE_H

#include <string>
#include <vector>
#include <mutex>
#include <chrono>

struct FastDecayItem {
    std::string deviceId;
    std::string timestamp;  // ISO 8601 format
    double decayRate;       // 腐败程度百分比 (0-100)
    long long addedAt;     // 入队时间戳（秒）
};

class FastDecayQueue {
public:
    static FastDecayQueue& getInstance();

    // 添加快腐败樱桃到队列
    void addItem(const std::string& deviceId, double decayRate);
    
    // 移除指定设备的最旧记录
    void removeItem(const std::string& deviceId);
    
    // 获取当前队列中所有快腐败樱桃
    std::vector<FastDecayItem> getQueue() const;
    
    // 检查设备是否已在队列中
    bool hasDevice(const std::string& deviceId) const;
    
    // 获取队列大小
    size_t size() const;
    
    // 清理过期项（超过指定秒数，默认为24小时）
    void cleanupExpired(long long maxAgeSeconds = 86400);
    
    // 更新已存在设备的腐败率
    void updateItem(const std::string& deviceId, double decayRate);

private:
    FastDecayQueue() = default;
    ~FastDecayQueue() = default;
    FastDecayQueue(const FastDecayQueue&) = delete;
    FastDecayQueue& operator=(const FastDecayQueue&) = delete;
    
    mutable std::mutex mutex_;
    std::vector<FastDecayItem> queue_;
    
    // 生成 ISO 8601 格式时间戳
    std::string generateISO8601Timestamp() const;
};

#endif // FAST_DECAY_QUEUE_H
