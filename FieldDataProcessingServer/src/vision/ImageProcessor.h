#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace ImageProcessor {

struct ImageData {
    int width;
    int height;
    int channels;
    uint8_t* data;
};

constexpr int INPUT_WIDTH = 224;
constexpr int INPUT_HEIGHT = 224;
constexpr int INPUT_CHANNELS = 3;

constexpr float IMAGENET_MEAN_R = 0.485f;
constexpr float IMAGENET_MEAN_G = 0.456f;
constexpr float IMAGENET_MEAN_B = 0.406f;

constexpr float IMAGENET_STD_R = 0.229f;
constexpr float IMAGENET_STD_G = 0.224f;
constexpr float IMAGENET_STD_B = 0.225f;

bool loadImage(const std::string& path, ImageData& img);

std::vector<float> preprocessImage(const ImageData& img);

void freeImage(ImageData& img);

int centerCrop(int width, int height, int& cropX, int& cropY, int& cropW, int& cropH);

void resizeBilinear(const uint8_t* src, int srcW, int srcH, int srcC,
                   uint8_t* dst, int dstW, int dstH);

}
