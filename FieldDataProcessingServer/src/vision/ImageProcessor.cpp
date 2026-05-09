#include "ImageProcessor.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../../third_party/stb/stb_image.h"
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace ImageProcessor {

bool loadImage(const std::string& path, ImageData& img) {
    img.data = stbi_load(path.c_str(), &img.width, &img.height, &img.channels, 3);
    if (!img.data) {
        std::cerr << "[ImageProcessor] Failed to load image: " << path 
                  << " - " << stbi_failure_reason() << std::endl;
        img.width = 0;
        img.height = 0;
        img.channels = 0;
        return false;
    }
    img.channels = 3;
    std::cout << "[ImageProcessor] Loaded image: " << img.width << "x" << img.height 
              << " channels=" << img.channels << std::endl;
    return true;
}

void freeImage(ImageData& img) {
    if (img.data) {
        stbi_image_free(img.data);
        img.data = nullptr;
    }
}

int centerCrop(int width, int height, int& cropX, int& cropY, int& cropW, int& cropH) {
    cropW = std::min(width, height);
    cropH = cropW;
    cropX = (width - cropW) / 2;
    cropY = (height - cropH) / 2;
    return 0;
}

void resizeBilinear(const uint8_t* src, int srcW, int srcH, int srcC,
                   uint8_t* dst, int dstW, int dstH) {
    float scaleX = static_cast<float>(srcW) / dstW;
    float scaleY = static_cast<float>(srcH) / dstH;

    for (int dy = 0; dy < dstH; ++dy) {
        for (int dx = 0; dx < dstW; ++dx) {
            float sx = (dx + 0.5f) * scaleX - 0.5f;
            float sy = (dy + 0.5f) * scaleY - 0.5f;

            int x0 = static_cast<int>(std::floor(sx));
            int y0 = static_cast<int>(std::floor(sy));
            int x1 = std::min(x0 + 1, srcW - 1);
            int y1 = std::min(y0 + 1, srcH - 1);

            float fx = sx - x0;
            float fy = sy - y0;
            float fx1 = 1.0f - fx;
            float fy1 = 1.0f - fy;

            x0 = std::max(0, x0);
            y0 = std::max(0, y0);

            for (int c = 0; c < srcC; ++c) {
                float v00 = src[(y0 * srcW + x0) * srcC + c];
                float v01 = src[(y0 * srcW + x1) * srcC + c];
                float v10 = src[(y1 * srcW + x0) * srcC + c];
                float v11 = src[(y1 * srcW + x1) * srcC + c];

                float value = v00 * fx1 * fy1 + v01 * fx * fy1 + v10 * fx1 * fy + v11 * fx * fy;
                dst[(dy * dstW + dx) * srcC + c] = static_cast<uint8_t>(std::clamp(value, 0.0f, 255.0f));
            }
        }
    }
}

std::vector<float> preprocessImage(const ImageData& img) {
    int cropX, cropY, cropW, cropH;
    centerCrop(img.width, img.height, cropX, cropY, cropW, cropH);

    int croppedSize = cropW * cropH * 3;
    uint8_t* cropped = new uint8_t[croppedSize];

    for (int y = 0; y < cropH; ++y) {
        for (int x = 0; x < cropW; ++x) {
            int srcIdx = ((cropY + y) * img.width + (cropX + x)) * 3;
            int dstIdx = (y * cropW + x) * 3;
            std::memcpy(&cropped[dstIdx], &img.data[srcIdx], 3);
        }
    }

    uint8_t* resized = new uint8_t[INPUT_WIDTH * INPUT_HEIGHT * 3];
    resizeBilinear(cropped, cropW, cropH, 3, resized, INPUT_WIDTH, INPUT_HEIGHT);

    delete[] cropped;

    std::vector<float> tensor(1 * 3 * INPUT_WIDTH * INPUT_HEIGHT);

    for (int y = 0; y < INPUT_HEIGHT; ++y) {
        for (int x = 0; x < INPUT_WIDTH; ++x) {
            int idx = (y * INPUT_WIDTH + x) * 3;
            float r = static_cast<float>(resized[idx + 0]) / 255.0f;
            float g = static_cast<float>(resized[idx + 1]) / 255.0f;
            float b = static_cast<float>(resized[idx + 2]) / 255.0f;

            tensor[0 * INPUT_WIDTH * INPUT_HEIGHT + y * INPUT_WIDTH + x] = (r - IMAGENET_MEAN_R) / IMAGENET_STD_R;
            tensor[1 * INPUT_WIDTH * INPUT_HEIGHT + y * INPUT_WIDTH + x] = (g - IMAGENET_MEAN_G) / IMAGENET_STD_G;
            tensor[2 * INPUT_WIDTH * INPUT_HEIGHT + y * INPUT_WIDTH + x] = (b - IMAGENET_MEAN_B) / IMAGENET_STD_B;
        }
    }

    delete[] resized;

    return tensor;
}

}
