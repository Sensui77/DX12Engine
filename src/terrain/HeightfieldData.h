#pragma once

#include <vector>

struct HeightfieldData {
    int sampleCountX = 0;
    int sampleCountZ = 0;
    float cellSize = 1.0f;
    float verticalScale = 1.0f;
    std::vector<float> heights;

    bool IsValid() const {
        return sampleCountX > 0
            && sampleCountZ > 0
            && heights.size() == static_cast<size_t>(sampleCountX) * static_cast<size_t>(sampleCountZ);
    }
};
