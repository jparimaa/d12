#pragma once

#include <vector>

// clang-format off
const std::vector<float> c_fullscreenTriangle = {
    -1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 
	 3.0f,  1.0f, 0.0f, 2.0f, 0.0f, 
	-1.0f, -3.0f, 0.0f, 0.0f, 2.0f,
};

const std::vector<uint16_t> c_fullscreenTriangleIndices = {0, 1, 2};
// clang-format on