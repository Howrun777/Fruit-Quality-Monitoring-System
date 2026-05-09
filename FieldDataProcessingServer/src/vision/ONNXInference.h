#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>

class ONNXInference {
public:
    static ONNXInference& getInstance();

    bool initialize(const std::string& modelPath);

    int infer(const std::vector<float>& inputTensor);

    bool isAvailable() const { return m_available; }

    std::string getLastError() const { return m_lastError; }

private:
    ONNXInference();
    ~ONNXInference();

    ONNXInference(const ONNXInference&) = delete;
    ONNXInference& operator=(const ONNXInference&) = delete;

    struct Impl;
    std::unique_ptr<Impl> m_impl;

    bool m_available = false;
    std::string m_lastError;
    std::mutex m_mutex;
};
