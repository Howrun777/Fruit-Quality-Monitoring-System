#include "ONNXInference.h"
#include "ImageProcessor.h"
#include <onnxruntime_cxx_api.h>
#include <iostream>
#include <sstream>

struct ONNXInference::Impl {
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING, "VisionAI"};
    Ort::SessionOptions sessionOptions;
    std::unique_ptr<Ort::Session> session;
    std::vector<const char*> inputNames;
    std::vector<const char*> outputNames;
    Ort::MemoryInfo memoryInfo;
    Ort::AllocatorWithDefaultOptions allocator;

    Impl() : memoryInfo(Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault)) {
        sessionOptions.SetIntraOpNumThreads(1);
        sessionOptions.SetInterOpNumThreads(1);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
    }
};

ONNXInference::ONNXInference() : m_impl(std::make_unique<Impl>()) {}

ONNXInference::~ONNXInference() = default;

ONNXInference& ONNXInference::getInstance() {
    static ONNXInference instance;
    return instance;
}

bool ONNXInference::initialize(const std::string& modelPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_available) {
        std::cout << "[ONNXInference] Already initialized" << std::endl;
        return true;
    }

    try {
        std::cout << "[ONNXInference] Loading model from: " << modelPath << std::endl;

        std::unique_ptr<Ort::Session> session = std::make_unique<Ort::Session>(
            m_impl->env, modelPath.c_str(), m_impl->sessionOptions);

        size_t numInputNodes = session->GetInputCount();
        size_t numOutputNodes = session->GetOutputCount();

        std::cout << "[ONNXInference] Input nodes: " << numInputNodes 
                  << ", Output nodes: " << numOutputNodes << std::endl;

        m_impl->inputNames.clear();
        m_impl->outputNames.clear();

        for (size_t i = 0; i < numInputNodes; ++i) {
            auto inputName = session->GetInputNameAllocated(i, m_impl->allocator);
            m_impl->inputNames.push_back(inputName.get());
            
            auto typeInfo = session->GetInputTypeInfo(i);
            auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
            std::vector<int64_t> inputShape = tensorInfo.GetShape();
            
            std::cout << "[ONNXInference] Input[" << i << "]: " << inputName.get() 
                      << " shape: [";
            for (size_t j = 0; j < inputShape.size(); ++j) {
                std::cout << inputShape[j];
                if (j < inputShape.size() - 1) std::cout << ", ";
            }
            std::cout << "]" << std::endl;
        }

        for (size_t i = 0; i < numOutputNodes; ++i) {
            auto outputName = session->GetOutputNameAllocated(i, m_impl->allocator);
            m_impl->outputNames.push_back(outputName.get());
            
            auto typeInfo = session->GetOutputTypeInfo(i);
            auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
            std::vector<int64_t> outputShape = tensorInfo.GetShape();
            
            std::cout << "[ONNXInference] Output[" << i << "]: " << outputName.get()
                      << " shape: [";
            for (size_t j = 0; j < outputShape.size(); ++j) {
                std::cout << outputShape[j];
                if (j < outputShape.size() - 1) std::cout << ", ";
            }
            std::cout << "]" << std::endl;
        }

        m_impl->session = std::move(session);
        m_available = true;
        
        std::cout << "[ONNXInference] Model loaded successfully!" << std::endl;
        return true;

    } catch (const Ort::Exception& e) {
        m_lastError = std::string("ONNX Exception: ") + e.what();
        std::cerr << "[ONNXInference] " << m_lastError << std::endl;
        m_available = false;
        return false;
    } catch (const std::exception& e) {
        m_lastError = std::string("Std Exception: ") + e.what();
        std::cerr << "[ONNXInference] " << m_lastError << std::endl;
        m_available = false;
        return false;
    }
}

int ONNXInference::infer(const std::vector<float>& inputTensor) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_available || !m_impl->session) {
        std::cerr << "[ONNXInference] Session not available" << std::endl;
        return -1;
    }

    try {
        const int64_t inputShape[] = {1, 3, ImageProcessor::INPUT_HEIGHT, ImageProcessor::INPUT_WIDTH};
        size_t tensorSize = inputTensor.size();

        auto inputTensorOrt = Ort::Value::CreateTensor<float>(
            m_impl->memoryInfo,
            const_cast<float*>(inputTensor.data()),
            tensorSize,
            inputShape,
            4
        );

        auto outputTensors = m_impl->session->Run(
            Ort::RunOptions{nullptr},
            m_impl->inputNames.data(),
            &inputTensorOrt,
            1,
            m_impl->outputNames.data(),
            m_impl->outputNames.size()
        );

        float* outputData = outputTensors[0].GetTensorMutableData<float>();
        
        int numClasses = outputTensors[0].GetTensorTypeAndShapeInfo().GetShape()[1];
        
        int predictedClass = 0;
        float maxProb = outputData[0];
        for (int i = 1; i < numClasses; ++i) {
            if (outputData[i] > maxProb) {
                maxProb = outputData[i];
                predictedClass = i;
            }
        }

        int qualityLevel = predictedClass + 1;
        
        std::cout << "[ONNXInference] Prediction: class=" << predictedClass 
                  << " quality_level=" << qualityLevel << " max_prob=" << maxProb << std::endl;

        return qualityLevel;

    } catch (const Ort::Exception& e) {
        std::cerr << "[ONNXInference] Inference error: " << e.what() << std::endl;
        return -1;
    } catch (const std::exception& e) {
        std::cerr << "[ONNXInference] Inference error: " << e.what() << std::endl;
        return -1;
    }
}
