#include "VisionTask.h"
#include "ImageProcessor.h"
#include "ONNXInference.h"
#include "../db/MySQLDriver.h"
#include "../utils/Logger.h"
#include <iostream>
#include <chrono>

VisionTaskProcessor& VisionTaskProcessor::getInstance() {
    static VisionTaskProcessor instance;
    return instance;
}

VisionTaskProcessor::VisionTaskProcessor() = default;

VisionTaskProcessor::~VisionTaskProcessor() {
    stop();
}

void VisionTaskProcessor::start() {
    if (m_running.load()) {
        LOGW("Vision", "Already running");
        return;
    }

    m_running = true;
    m_stopRequested = false;
    m_workerThread = std::thread(&VisionTaskProcessor::workerLoop, this);
    LOGI("Vision", "Worker thread started");
}

void VisionTaskProcessor::stop() {
    if (!m_running.load()) {
        return;
    }

    m_stopRequested = true;
    m_cv.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_running = false;
    LOGI("Vision", "Worker thread stopped");
}

void VisionTaskProcessor::enqueue(const VisionTask& task) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_taskQueue.push(task);
    }
    m_cv.notify_one();
    
    LOGD("Vision", "Task enqueued for device: " + task.device_id + 
         ", queue size: " + std::to_string(m_taskQueue.size()));
}

size_t VisionTaskProcessor::getQueueSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_taskQueue.size();
}

void VisionTaskProcessor::workerLoop() {
    LOGI("Vision", "Worker loop started, waiting for tasks...");

    while (!m_stopRequested.load()) {
        VisionTask task;
        bool hasTask = false;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait_for(lock, std::chrono::seconds(1), [this] {
                return !m_taskQueue.empty() || m_stopRequested.load();
            });

            if (m_stopRequested.load()) {
                break;
            }

            if (!m_taskQueue.empty()) {
                task = m_taskQueue.front();
                m_taskQueue.pop();
                hasTask = true;
            }
        }

        if (hasTask) {
            processTask(task);
        }
    }

    LOGI("Vision", "Worker loop exiting");
}

void VisionTaskProcessor::processTask(const VisionTask& task) {
    LOGI("Vision", "Processing task: device_id=" + task.device_id + 
         ", timestamp=" + std::to_string(task.collected_at));

    ImageProcessor::ImageData img;
    int qualityLevel = -1;

    if (!ImageProcessor::loadImage(task.image_path, img)) {
        LOGE("Vision", "Failed to load image: " + task.image_path);
    } else {
        std::vector<float> tensor = ImageProcessor::preprocessImage(img);
        ImageProcessor::freeImage(img);

        auto& onnx = ONNXInference::getInstance();
        if (onnx.isAvailable()) {
            qualityLevel = onnx.infer(tensor);
            if (qualityLevel < 1) {
                qualityLevel = -1;
            }
        } else {
            LOGW("Vision", "ONNX not available, setting quality_level = -1");
            qualityLevel = -1;
        }
    }

    std::string safeDeviceId = MySQLDriver::getInstance().escapeString(task.device_id);
    std::string safeImageUrl = MySQLDriver::getInstance().escapeString(task.image_relative_path);

    std::string sql = "INSERT INTO fruit_vision_data (device_id, collected_at, quality_level, image_url) VALUES ('"
        + safeDeviceId + "', " + std::to_string(task.collected_at) + ", " 
        + std::to_string(qualityLevel) + ", '" + safeImageUrl + "') "
        + "ON DUPLICATE KEY UPDATE quality_level = VALUES(quality_level), image_url = VALUES(image_url)";

    bool success = MySQLDriver::getInstance().execute(sql);
    if (success) {
        LOGI("Vision", "Vision data saved: device=" + task.device_id + 
             ", quality_level=" + std::to_string(qualityLevel));
    } else {
        // 检查是否是主键冲突（数据已存在），这种情况不算错误
        LOGW("Vision", "Vision data update skipped (possibly duplicate key): device=" + task.device_id);
    }
}
