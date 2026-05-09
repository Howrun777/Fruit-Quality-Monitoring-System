#pragma once
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

struct VisionTask {
    std::string device_id;
    int64_t collected_at;
    std::string image_path;
    std::string image_relative_path;
};

class VisionTaskProcessor {
public:
    static VisionTaskProcessor& getInstance();

    void start();
    void stop();

    void enqueue(const VisionTask& task);

    size_t getQueueSize() const;

    bool isRunning() const { return m_running.load(); }

private:
    VisionTaskProcessor();
    ~VisionTaskProcessor();

    VisionTaskProcessor(const VisionTaskProcessor&) = delete;
    VisionTaskProcessor& operator=(const VisionTaskProcessor&) = delete;

    void workerLoop();

    void processTask(const VisionTask& task);

    std::queue<VisionTask> m_taskQueue;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;
    std::thread m_workerThread;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopRequested{false};
};
