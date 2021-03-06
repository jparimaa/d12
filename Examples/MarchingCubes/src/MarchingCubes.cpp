﻿#include "MarchingCubes.h"
#include "FastNoise.h"

#include <DirectXMath.h>

#include <functional>
#include <chrono>
#include <iostream>
#include <string>

namespace
{
const std::vector<DirectX::XMFLOAT3> c_vertexOffset{
    {0.0f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.0f},
    {1.0f, 1.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
    {1.0f, 0.0f, 1.0f},
    {1.0f, 1.0f, 1.0f},
    {0.0f, 1.0f, 1.0f}};

// clang-format off
const uint8_t c_edgeConnections[12][2]
{
	{0,1}, {1,2}, {2,3}, {3,0},
	{4,5}, {5,6}, {6,7}, {7,4},
	{0,4}, {1,5}, {2,6}, {3,7}
};

const std::vector<std::vector<int8_t>> c_triangleConnections
{
	{},
	{0, 8, 3},
	{0, 1, 9},
	{1, 8, 3, 9, 8, 1},
	{1, 2, 10},
	{0, 8, 3, 1, 2, 10},
	{9, 2, 10, 0, 2, 9},
	{2, 8, 3, 2, 10, 8, 10, 9, 8},
	{3, 11, 2},
	{0, 11, 2, 8, 11, 0},
	{1, 9, 0, 2, 3, 11},
	{1, 11, 2, 1, 9, 11, 9, 8, 11},
	{3, 10, 1, 11, 10, 3},
	{0, 10, 1, 0, 8, 10, 8, 11, 10},
	{3, 9, 0, 3, 11, 9, 11, 10, 9},
	{9, 8, 10, 10, 8, 11},
	{4, 7, 8},
	{4, 3, 0, 7, 3, 4},
	{0, 1, 9, 8, 4, 7},
	{4, 1, 9, 4, 7, 1, 7, 3, 1},
	{1, 2, 10, 8, 4, 7},
	{3, 4, 7, 3, 0, 4, 1, 2, 10},
	{9, 2, 10, 9, 0, 2, 8, 4, 7},
	{2, 10, 9, 2, 9, 7, 2, 7, 3, 7, 9, 4},
	{8, 4, 7, 3, 11, 2},
	{11, 4, 7, 11, 2, 4, 2, 0, 4},
	{9, 0, 1, 8, 4, 7, 2, 3, 11},
	{4, 7, 11, 9, 4, 11, 9, 11, 2, 9, 2, 1},
	{3, 10, 1, 3, 11, 10, 7, 8, 4},
	{1, 11, 10, 1, 4, 11, 1, 0, 4, 7, 11, 4},
	{4, 7, 8, 9, 0, 11, 9, 11, 10, 11, 0, 3},
	{4, 7, 11, 4, 11, 9, 9, 11, 10},
	{9, 5, 4},
	{9, 5, 4, 0, 8, 3},
	{0, 5, 4, 1, 5, 0},
	{8, 5, 4, 8, 3, 5, 3, 1, 5},
	{1, 2, 10, 9, 5, 4},
	{3, 0, 8, 1, 2, 10, 4, 9, 5},
	{5, 2, 10, 5, 4, 2, 4, 0, 2},
	{2, 10, 5, 3, 2, 5, 3, 5, 4, 3, 4, 8},
	{9, 5, 4, 2, 3, 11},
	{0, 11, 2, 0, 8, 11, 4, 9, 5},
	{0, 5, 4, 0, 1, 5, 2, 3, 11},
	{2, 1, 5, 2, 5, 8, 2, 8, 11, 4, 8, 5},
	{10, 3, 11, 10, 1, 3, 9, 5, 4},
	{4, 9, 5, 0, 8, 1, 8, 10, 1, 8, 11, 10},
	{5, 4, 0, 5, 0, 11, 5, 11, 10, 11, 0, 3},
	{5, 4, 8, 5, 8, 10, 10, 8, 11},
	{9, 7, 8, 5, 7, 9},
	{9, 3, 0, 9, 5, 3, 5, 7, 3},
	{0, 7, 8, 0, 1, 7, 1, 5, 7},
	{1, 5, 3, 3, 5, 7},
	{9, 7, 8, 9, 5, 7, 10, 1, 2},
	{10, 1, 2, 9, 5, 0, 5, 3, 0, 5, 7, 3},
	{8, 0, 2, 8, 2, 5, 8, 5, 7, 10, 5, 2},
	{2, 10, 5, 2, 5, 3, 3, 5, 7},
	{7, 9, 5, 7, 8, 9, 3, 11, 2},
	{9, 5, 7, 9, 7, 2, 9, 2, 0, 2, 7, 11},
	{2, 3, 11, 0, 1, 8, 1, 7, 8, 1, 5, 7},
	{11, 2, 1, 11, 1, 7, 7, 1, 5},
	{9, 5, 8, 8, 5, 7, 10, 1, 3, 10, 3, 11},
	{5, 7, 0, 5, 0, 9, 7, 11, 0, 1, 0, 10, 11, 10, 0},
	{11, 10, 0, 11, 0, 3, 10, 5, 0, 8, 0, 7, 5, 7, 0},
	{11, 10, 5, 7, 11, 5},
	{10, 6, 5},
	{0, 8, 3, 5, 10, 6},
	{9, 0, 1, 5, 10, 6},
	{1, 8, 3, 1, 9, 8, 5, 10, 6},
	{1, 6, 5, 2, 6, 1},
	{1, 6, 5, 1, 2, 6, 3, 0, 8},
	{9, 6, 5, 9, 0, 6, 0, 2, 6},
	{5, 9, 8, 5, 8, 2, 5, 2, 6, 3, 2, 8},
	{2, 3, 11, 10, 6, 5},
	{11, 0, 8, 11, 2, 0, 10, 6, 5},
	{0, 1, 9, 2, 3, 11, 5, 10, 6},
	{5, 10, 6, 1, 9, 2, 9, 11, 2, 9, 8, 11},
	{6, 3, 11, 6, 5, 3, 5, 1, 3},
	{0, 8, 11, 0, 11, 5, 0, 5, 1, 5, 11, 6},
	{3, 11, 6, 0, 3, 6, 0, 6, 5, 0, 5, 9},
	{6, 5, 9, 6, 9, 11, 11, 9, 8},
	{5, 10, 6, 4, 7, 8},
	{4, 3, 0, 4, 7, 3, 6, 5, 10},
	{1, 9, 0, 5, 10, 6, 8, 4, 7},
	{10, 6, 5, 1, 9, 7, 1, 7, 3, 7, 9, 4},
	{6, 1, 2, 6, 5, 1, 4, 7, 8},
	{1, 2, 5, 5, 2, 6, 3, 0, 4, 3, 4, 7},
	{8, 4, 7, 9, 0, 5, 0, 6, 5, 0, 2, 6},
	{7, 3, 9, 7, 9, 4, 3, 2, 9, 5, 9, 6, 2, 6, 9},
	{3, 11, 2, 7, 8, 4, 10, 6, 5},
	{5, 10, 6, 4, 7, 2, 4, 2, 0, 2, 7, 11},
	{0, 1, 9, 4, 7, 8, 2, 3, 11, 5, 10, 6},
	{9, 2, 1, 9, 11, 2, 9, 4, 11, 7, 11, 4, 5, 10, 6},
	{8, 4, 7, 3, 11, 5, 3, 5, 1, 5, 11, 6},
	{5, 1, 11, 5, 11, 6, 1, 0, 11, 7, 11, 4, 0, 4, 11},
	{0, 5, 9, 0, 6, 5, 0, 3, 6, 11, 6, 3, 8, 4, 7},
	{6, 5, 9, 6, 9, 11, 4, 7, 9, 7, 11, 9},
	{10, 4, 9, 6, 4, 10},
	{4, 10, 6, 4, 9, 10, 0, 8, 3},
	{10, 0, 1, 10, 6, 0, 6, 4, 0},
	{8, 3, 1, 8, 1, 6, 8, 6, 4, 6, 1, 10},
	{1, 4, 9, 1, 2, 4, 2, 6, 4},
	{3, 0, 8, 1, 2, 9, 2, 4, 9, 2, 6, 4},
	{0, 2, 4, 4, 2, 6},
	{8, 3, 2, 8, 2, 4, 4, 2, 6},
	{10, 4, 9, 10, 6, 4, 11, 2, 3},
	{0, 8, 2, 2, 8, 11, 4, 9, 10, 4, 10, 6},
	{3, 11, 2, 0, 1, 6, 0, 6, 4, 6, 1, 10},
	{6, 4, 1, 6, 1, 10, 4, 8, 1, 2, 1, 11, 8, 11, 1},
	{9, 6, 4, 9, 3, 6, 9, 1, 3, 11, 6, 3},
	{8, 11, 1, 8, 1, 0, 11, 6, 1, 9, 1, 4, 6, 4, 1},
	{3, 11, 6, 3, 6, 0, 0, 6, 4},
	{6, 4, 8, 11, 6, 8},
	{7, 10, 6, 7, 8, 10, 8, 9, 10},
	{0, 7, 3, 0, 10, 7, 0, 9, 10, 6, 7, 10},
	{10, 6, 7, 1, 10, 7, 1, 7, 8, 1, 8, 0},
	{10, 6, 7, 10, 7, 1, 1, 7, 3},
	{1, 2, 6, 1, 6, 8, 1, 8, 9, 8, 6, 7},
	{2, 6, 9, 2, 9, 1, 6, 7, 9, 0, 9, 3, 7, 3, 9},
	{7, 8, 0, 7, 0, 6, 6, 0, 2},
	{7, 3, 2, 6, 7, 2},
	{2, 3, 11, 10, 6, 8, 10, 8, 9, 8, 6, 7},
	{2, 0, 7, 2, 7, 11, 0, 9, 7, 6, 7, 10, 9, 10, 7},
	{1, 8, 0, 1, 7, 8, 1, 10, 7, 6, 7, 10, 2, 3, 11},
	{11, 2, 1, 11, 1, 7, 10, 6, 1, 6, 7, 1},
	{8, 9, 6, 8, 6, 7, 9, 1, 6, 11, 6, 3, 1, 3, 6},
	{0, 9, 1, 11, 6, 7},
	{7, 8, 0, 7, 0, 6, 3, 11, 0, 11, 6, 0},
	{7, 11, 6},
	{7, 6, 11},
	{3, 0, 8, 11, 7, 6},
	{0, 1, 9, 11, 7, 6},
	{8, 1, 9, 8, 3, 1, 11, 7, 6},
	{10, 1, 2, 6, 11, 7},
	{1, 2, 10, 3, 0, 8, 6, 11, 7},
	{2, 9, 0, 2, 10, 9, 6, 11, 7},
	{6, 11, 7, 2, 10, 3, 10, 8, 3, 10, 9, 8},
	{7, 2, 3, 6, 2, 7},
	{7, 0, 8, 7, 6, 0, 6, 2, 0},
	{2, 7, 6, 2, 3, 7, 0, 1, 9},
	{1, 6, 2, 1, 8, 6, 1, 9, 8, 8, 7, 6},
	{10, 7, 6, 10, 1, 7, 1, 3, 7},
	{10, 7, 6, 1, 7, 10, 1, 8, 7, 1, 0, 8},
	{0, 3, 7, 0, 7, 10, 0, 10, 9, 6, 10, 7},
	{7, 6, 10, 7, 10, 8, 8, 10, 9},
	{6, 8, 4, 11, 8, 6},
	{3, 6, 11, 3, 0, 6, 0, 4, 6},
	{8, 6, 11, 8, 4, 6, 9, 0, 1},
	{9, 4, 6, 9, 6, 3, 9, 3, 1, 11, 3, 6},
	{6, 8, 4, 6, 11, 8, 2, 10, 1},
	{1, 2, 10, 3, 0, 11, 0, 6, 11, 0, 4, 6},
	{4, 11, 8, 4, 6, 11, 0, 2, 9, 2, 10, 9},
	{10, 9, 3, 10, 3, 2, 9, 4, 3, 11, 3, 6, 4, 6, 3},
	{8, 2, 3, 8, 4, 2, 4, 6, 2},
	{0, 4, 2, 4, 6, 2},
	{1, 9, 0, 2, 3, 4, 2, 4, 6, 4, 3, 8},
	{1, 9, 4, 1, 4, 2, 2, 4, 6},
	{8, 1, 3, 8, 6, 1, 8, 4, 6, 6, 10, 1},
	{10, 1, 0, 10, 0, 6, 6, 0, 4},
	{4, 6, 3, 4, 3, 8, 6, 10, 3, 0, 3, 9, 10, 9, 3},
	{10, 9, 4, 6, 10, 4},
	{4, 9, 5, 7, 6, 11},
	{0, 8, 3, 4, 9, 5, 11, 7, 6},
	{5, 0, 1, 5, 4, 0, 7, 6, 11},
	{11, 7, 6, 8, 3, 4, 3, 5, 4, 3, 1, 5},
	{9, 5, 4, 10, 1, 2, 7, 6, 11},
	{6, 11, 7, 1, 2, 10, 0, 8, 3, 4, 9, 5},
	{7, 6, 11, 5, 4, 10, 4, 2, 10, 4, 0, 2},
	{3, 4, 8, 3, 5, 4, 3, 2, 5, 10, 5, 2, 11, 7, 6},
	{7, 2, 3, 7, 6, 2, 5, 4, 9},
	{9, 5, 4, 0, 8, 6, 0, 6, 2, 6, 8, 7},
	{3, 6, 2, 3, 7, 6, 1, 5, 0, 5, 4, 0},
	{6, 2, 8, 6, 8, 7, 2, 1, 8, 4, 8, 5, 1, 5, 8},
	{9, 5, 4, 10, 1, 6, 1, 7, 6, 1, 3, 7},
	{1, 6, 10, 1, 7, 6, 1, 0, 7, 8, 7, 0, 9, 5, 4},
	{4, 0, 10, 4, 10, 5, 0, 3, 10, 6, 10, 7, 3, 7, 10},
	{7, 6, 10, 7, 10, 8, 5, 4, 10, 4, 8, 10},
	{6, 9, 5, 6, 11, 9, 11, 8, 9},
	{3, 6, 11, 0, 6, 3, 0, 5, 6, 0, 9, 5},
	{0, 11, 8, 0, 5, 11, 0, 1, 5, 5, 6, 11},
	{6, 11, 3, 6, 3, 5, 5, 3, 1},
	{1, 2, 10, 9, 5, 11, 9, 11, 8, 11, 5, 6},
	{0, 11, 3, 0, 6, 11, 0, 9, 6, 5, 6, 9, 1, 2, 10},
	{11, 8, 5, 11, 5, 6, 8, 0, 5, 10, 5, 2, 0, 2, 5},
	{6, 11, 3, 6, 3, 5, 2, 10, 3, 10, 5, 3},
	{5, 8, 9, 5, 2, 8, 5, 6, 2, 3, 8, 2},
	{9, 5, 6, 9, 6, 0, 0, 6, 2},
	{1, 5, 8, 1, 8, 0, 5, 6, 8, 3, 8, 2, 6, 2, 8},
	{1, 5, 6, 2, 1, 6},
	{1, 3, 6, 1, 6, 10, 3, 8, 6, 5, 6, 9, 8, 9, 6},
	{10, 1, 0, 10, 0, 6, 9, 5, 0, 5, 6, 0},
	{0, 3, 8, 5, 6, 10},
	{10, 5, 6},
	{11, 5, 10, 7, 5, 11},
	{11, 5, 10, 11, 7, 5, 8, 3, 0},
	{5, 11, 7, 5, 10, 11, 1, 9, 0},
	{10, 7, 5, 10, 11, 7, 9, 8, 1, 8, 3, 1},
	{11, 1, 2, 11, 7, 1, 7, 5, 1},
	{0, 8, 3, 1, 2, 7, 1, 7, 5, 7, 2, 11},
	{9, 7, 5, 9, 2, 7, 9, 0, 2, 2, 11, 7},
	{7, 5, 2, 7, 2, 11, 5, 9, 2, 3, 2, 8, 9, 8, 2},
	{2, 5, 10, 2, 3, 5, 3, 7, 5},
	{8, 2, 0, 8, 5, 2, 8, 7, 5, 10, 2, 5},
	{9, 0, 1, 5, 10, 3, 5, 3, 7, 3, 10, 2},
	{9, 8, 2, 9, 2, 1, 8, 7, 2, 10, 2, 5, 7, 5, 2},
	{1, 3, 5, 3, 7, 5},
	{0, 8, 7, 0, 7, 1, 1, 7, 5},
	{9, 0, 3, 9, 3, 5, 5, 3, 7},
	{9, 8, 7, 5, 9, 7},
	{5, 8, 4, 5, 10, 8, 10, 11, 8},
	{5, 0, 4, 5, 11, 0, 5, 10, 11, 11, 3, 0},
	{0, 1, 9, 8, 4, 10, 8, 10, 11, 10, 4, 5},
	{10, 11, 4, 10, 4, 5, 11, 3, 4, 9, 4, 1, 3, 1, 4},
	{2, 5, 1, 2, 8, 5, 2, 11, 8, 4, 5, 8},
	{0, 4, 11, 0, 11, 3, 4, 5, 11, 2, 11, 1, 5, 1, 11},
	{0, 2, 5, 0, 5, 9, 2, 11, 5, 4, 5, 8, 11, 8, 5},
	{9, 4, 5, 2, 11, 3},
	{2, 5, 10, 3, 5, 2, 3, 4, 5, 3, 8, 4},
	{5, 10, 2, 5, 2, 4, 4, 2, 0},
	{3, 10, 2, 3, 5, 10, 3, 8, 5, 4, 5, 8, 0, 1, 9},
	{5, 10, 2, 5, 2, 4, 1, 9, 2, 9, 4, 2},
	{8, 4, 5, 8, 5, 3, 3, 5, 1},
	{0, 4, 5, 1, 0, 5},
	{8, 4, 5, 8, 5, 3, 9, 0, 5, 0, 3, 5},
	{9, 4, 5},
	{4, 11, 7, 4, 9, 11, 9, 10, 11},
	{0, 8, 3, 4, 9, 7, 9, 11, 7, 9, 10, 11},
	{1, 10, 11, 1, 11, 4, 1, 4, 0, 7, 4, 11},
	{3, 1, 4, 3, 4, 8, 1, 10, 4, 7, 4, 11, 10, 11, 4},
	{4, 11, 7, 9, 11, 4, 9, 2, 11, 9, 1, 2},
	{9, 7, 4, 9, 11, 7, 9, 1, 11, 2, 11, 1, 0, 8, 3},
	{11, 7, 4, 11, 4, 2, 2, 4, 0},
	{11, 7, 4, 11, 4, 2, 8, 3, 4, 3, 2, 4},
	{2, 9, 10, 2, 7, 9, 2, 3, 7, 7, 4, 9},
	{9, 10, 7, 9, 7, 4, 10, 2, 7, 8, 7, 0, 2, 0, 7},
	{3, 7, 10, 3, 10, 2, 7, 4, 10, 1, 10, 0, 4, 0, 10},
	{1, 10, 2, 8, 7, 4},
	{4, 9, 1, 4, 1, 7, 7, 1, 3},
	{4, 9, 1, 4, 1, 7, 0, 8, 1, 8, 7, 1},
	{4, 0, 3, 7, 4, 3},
	{4, 8, 7},
	{9, 10, 8, 10, 11, 8},
	{3, 0, 9, 3, 9, 11, 11, 9, 10},
	{0, 1, 10, 0, 10, 8, 8, 10, 11},
	{3, 1, 10, 11, 3, 10},
	{1, 2, 11, 1, 11, 9, 9, 11, 8},
	{3, 0, 9, 3, 9, 11, 1, 2, 9, 2, 11, 9},
	{0, 2, 11, 8, 0, 11},
	{3, 2, 11},
	{2, 3, 8, 2, 8, 10, 10, 8, 9},
	{9, 10, 2, 0, 9, 2},
	{2, 3, 8, 2, 8, 10, 0, 1, 8, 1, 10, 8},
	{1, 10, 2},
	{1, 3, 8, 9, 1, 8},
	{0, 9, 1},
	{0, 3, 8},
	{}
};
// clang-format on

template<typename T>
void executeAndMeasureTime(const T& func, const std::string& name)
{
    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    func();
    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    std::cout << name << ", " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << " ms" << std::endl;
}
} // namespace

void MarchingCubes::generateVertices(size_t size)
{
    auto generateData = std::bind(&MarchingCubes::generateData, this, size);
    executeAndMeasureTime(generateData, "generateData");
    auto generateMesh = std::bind(&MarchingCubes::generateMesh, this);
    executeAndMeasureTime(generateMesh, "generateMesh");
    m_dataSet.clear();
    auto generateIndicesAndShadingNormals = std::bind(&MarchingCubes::generateIndicesAndShadingNormals, this);
    executeAndMeasureTime(generateIndicesAndShadingNormals, "generateIndicesAndShadingNormals");
    auto generateVertexDataForRendering = std::bind(&MarchingCubes::generateVertexDataForRendering, this);
    executeAndMeasureTime(generateVertexDataForRendering, "generateVertexDataForRendering");
    m_cubeData.clear();
}

const std::vector<MarchingCubes::Vertex>& MarchingCubes::getVertices() const
{
    return m_vertices;
}

const std::vector<MarchingCubes::IndexType>& MarchingCubes::getIndices() const
{
    return m_indices;
}

void MarchingCubes::generateData(size_t size)
{
    FastNoise fastNoise;
    fastNoise.SetFrequency(0.04f);
    fastNoise.SetInterp(FastNoise::Quintic);
    m_dataSet.resize(size);
    for (size_t z = 0; z < size; ++z)
    {
        auto& layer = m_dataSet[z];
        layer.resize(size);
        for (size_t y = 0; y < size; ++y)
        {
            auto& row = layer[y];
            row.resize(size);
            for (size_t x = 0; x < size; ++x)
            {
                row[x] = fastNoise.GetPerlin(float(x), float(y), float(z));
            }
        }
    }
}

void MarchingCubes::generateMesh()
{
    const size_t size = m_dataSet.size() - 1;
    m_cubeData.resize(size);
    for (size_t z = 0; z < size; ++z)
    {
        m_cubeData[z].resize(size);
        for (size_t y = 0; y < size; ++y)
        {
            m_cubeData[z][y].resize(size);
            for (size_t x = 0; x < size; ++x)
            {
                std::array<float, 8> values{
                    m_dataSet[z][y][x],
                    m_dataSet[z][y][x + 1],
                    m_dataSet[z][y + 1][x + 1],
                    m_dataSet[z][y + 1][x],
                    m_dataSet[z + 1][y][x],
                    m_dataSet[z + 1][y][x + 1],
                    m_dataSet[z + 1][y + 1][x + 1],
                    m_dataSet[z + 1][y + 1][x]};
                generateCubeTriangles(m_cubeData[z][y][x], x, y, z, values);
            }
        }
    }
}

void MarchingCubes::generateCubeTriangles(CubeInfo& cubeInfo, size_t x, size_t y, size_t z, const std::array<float, 8>& values)
{
    using namespace DirectX;

    int triangleIndex = 0;
    for (size_t i = 0; i < values.size(); ++i)
    {
        if (values[i] > m_limit)
        {
            triangleIndex |= 1 << i;
        }
    }

    const std::vector<int8_t>& edges = c_triangleConnections[triangleIndex];

    const float xf = static_cast<float>(x);
    const float yf = static_cast<float>(y);
    const float zf = static_cast<float>(z);

    for (const int8_t edge : edges)
    {
        const uint8_t& v0Index = c_edgeConnections[edge][0];
        const uint8_t& v1Index = c_edgeConnections[edge][1];
        const float& v0Value = values[v0Index];
        const float& v1Value = values[v1Index];
        const float diff = v1Value - v0Value;
        assert(diff != 0.0f);
        const float t = v1Value / diff;
        assert(t >= 0.0f && t <= 1.0f);
        const XMVECTOR v0 = XMLoadFloat3(&c_vertexOffset[v0Index]);
        const XMVECTOR v1 = XMLoadFloat3(&c_vertexOffset[v1Index]);
        const XMVECTOR offset = XMVectorLerp(v0, v1, t);
        Vertex v;
        v.position = XMVectorSet(xf, yf, zf, 0.0f) + offset;
        cubeInfo.vertices.push_back(v);
    }

    const size_t vertexCount = cubeInfo.vertices.size();

    for (size_t i = 0; i < vertexCount; i += 3)
    {
        const XMVECTOR& v0 = cubeInfo.vertices[i + 0].position;
        const XMVECTOR& v1 = cubeInfo.vertices[i + 1].position;
        const XMVECTOR& v2 = cubeInfo.vertices[i + 2].position;
        const XMVECTOR n = XMVector3Normalize(XMVector3Cross(v1 - v0, v2 - v0));
        cubeInfo.vertices[i + 0].normal = n;
        cubeInfo.vertices[i + 1].normal = n;
        cubeInfo.vertices[i + 2].normal = n;
    }

    cubeInfo.indices = std::vector<IndexType>(vertexCount, -1);
}

void MarchingCubes::generateIndicesAndShadingNormals()
{
    int indexCounter = 0;
    const int size = static_cast<int>(m_cubeData.size());
    for (int z = 0; z < size; ++z)
    {
        for (int y = 0; y < size; ++y)
        {
            for (int x = 0; x < size; ++x)
            {
                CubeInfo& cubeInfo1 = m_cubeData[z][y][x];
                std::vector<CubeInfo*> adjacentInfos = getAdjacentCubeInfos(x, y, z);
                for (size_t i = 0; i < cubeInfo1.vertices.size(); ++i)
                {
                    if (cubeInfo1.indices[i] != -1)
                    {
                        continue;
                    }
                    cubeInfo1.indices[i] = indexCounter++;
                    const DirectX::XMVECTOR& p = cubeInfo1.vertices[i].position;
                    for (CubeInfo* cubeInfo2 : adjacentInfos)
                    {
                        const std::vector<int> equalIndices = getEqualVertexIndex(cubeInfo2, p);
                        if (!equalIndices.empty())
                        {
                            mergeVertices(cubeInfo1, *cubeInfo2, static_cast<int>(i), equalIndices);
                        }
                    }
                }
            }
        }
    }
}

std::vector<MarchingCubes::CubeInfo*> MarchingCubes::getAdjacentCubeInfos(int x, int y, int z)
{
    std::vector<CubeInfo*> infos;
    const int maxSize = static_cast<int>(m_cubeData.size());
    for (int zz = 0; zz <= 1; ++zz)
    {
        for (int yy = 0; yy <= 1; ++yy)
        {
            for (int xx = 0; xx <= 1; ++xx)
            {
                const int zPos = z + zz;
                const int yPos = y + yy;
                const int xPos = x + xx;
                if (zPos >= maxSize || yPos >= maxSize || xPos >= maxSize)
                {
                    continue;
                }

                CubeInfo* ci = &m_cubeData[zPos][yPos][xPos];

                if (ci->vertices.empty())
                {
                    continue;
                }

                infos.push_back(ci);
            }
        }
    }
    return infos;
}

std::vector<int> MarchingCubes::getEqualVertexIndex(CubeInfo* cubeInfo, const DirectX::XMVECTOR& p)
{
    std::vector<int> equals;
    for (size_t i = 0; i < cubeInfo->vertices.size(); ++i)
    {
        if (DirectX::XMVector3Equal(cubeInfo->vertices[i].position, p))
        {
            equals.push_back(static_cast<int>(i));
        }
    }
    return equals;
}

void MarchingCubes::mergeVertices(CubeInfo& c1, CubeInfo& c2, int i1, const std::vector<int>& equalIndices)
{
    bool same = &c1 == &c2;
    for (int index : equalIndices)
    {
        if (same && index == i1)
        {
            continue;
        }
        assert(c2.indices[index] == -1);
        c2.indices[index] = c1.indices[i1];
        c1.vertices[i1].normal = DirectX::XMVectorAdd(c1.vertices[i1].normal, c2.vertices[index].normal);
    }
}

void MarchingCubes::generateVertexDataForRendering()
{
    const size_t size = m_cubeData.size();
    int indexCounter = 0;
    for (size_t z = 0; z < size; ++z)
    {
        for (size_t y = 0; y < size; ++y)
        {
            for (size_t x = 0; x < size; ++x)
            {
                CubeInfo& cubeInfo = m_cubeData[z][y][x];
                for (size_t i = 0; i < cubeInfo.vertices.size(); ++i)
                {
                    assert(cubeInfo.indices[i] >= 0);
                    m_indices.push_back(cubeInfo.indices[i]);
                    if (cubeInfo.indices[i] == indexCounter)
                    {
                        cubeInfo.vertices[i].normal = DirectX::XMVector3Normalize(cubeInfo.vertices[i].normal);
                        m_vertices.push_back(cubeInfo.vertices[i]);
                        ++indexCounter;
                    }
                }
            }
        }
    }
}
