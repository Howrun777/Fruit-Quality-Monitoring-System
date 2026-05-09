#include "FastDecayQueue.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <chrono>

FastDecayQueue& FastDecayQueue::getInstance() {
    static FastDecayQueue instance;
    return instance;
}

std::string FastDecayQueue::generateISO8601Timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm tm_buf;
    gmtime_r(&time_t, &tm_buf);
    
    std::ostringstream oss;
    oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    
    return oss.str();
}

void FastDecayQueue::addItem(const std::string& deviceId, double decayRate) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查是否已存在
    auto it = std::find_if(queue_.begin(), queue_.end(),
        [&deviceId](const FastDecayItem& item) {
            return item.deviceId == deviceId;
        });
    
    if (it != queue_.end()) {
        // 更新已存在的记录
        it->decayRate = decayRate;
        it->timestamp = generateISO8601Timestamp();
        it->addedAt = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        return;
    }
    
    // 添加新记录
    FastDecayItem item;
    item.deviceId = deviceId;
    item.decayRate = decayRate;
    item.timestamp = generateISO8601Timestamp();
    item.addedAt = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    queue_.push_back(item);
}

void FastDecayQueue::removeItem(const std::string& deviceId) {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.erase(
        std::remove_if(queue_.begin(), queue_.end(),
            [&deviceId](const FastDecayItem& item) {
                return item.deviceId == deviceId;
            }),
        queue_.end()
    );
}

void FastDecayQueue::updateItem(const std::string& deviceId, double decayRate) {
    addItem(deviceId, decayRate);  // addItem 已经包含更新逻辑
}

bool FastDecayQueue::hasDevice(const std::string& deviceId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::any_of(queue_.begin(), queue_.end(),
        [&deviceId](const FastDecayItem& item) {
            return item.deviceId == deviceId;
        });
}

std::vector<FastDecayItem> FastDecayQueue::getQueue() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_;
}

size_t FastDecayQueue::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

void FastDecayQueue::cleanupExpired(long long maxAgeSeconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    queue_.erase(
        std::remove_if(queue_.begin(), queue_.end(),
            [now, maxAgeSeconds](const FastDecayItem& item) {
                return (now - item.addedAt) > maxAgeSeconds;
            }),
        queue_.end()
    );
}
