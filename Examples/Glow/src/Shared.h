#pragma once

#include "d3dx12.h"
#include <wrl.h>

#include <vector>

const std::vector<D3D12_INPUT_ELEMENT_DESC> c_vertexInputLayout = {
    {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"NORMAL", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TANGENT", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

struct RenderObject
{
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    Microsoft::WRL::ComPtr<ID3D12Resource> texture;
    UINT numIndices;
};

// clang-format off
const std::vector<float> c_fullscreenTriangle = {
    -1.0f,  1.0f, 0.0f, 0.0f, 0.0f, 
	 3.0f,  1.0f, 0.0f, 2.0f, 0.0f, 
	-1.0f, -3.0f, 0.0f, 0.0f, 2.0f,
};

const std::vector<uint16_t> c_fullscreenTriangleIndices = {0, 1, 2};
// clang-format on