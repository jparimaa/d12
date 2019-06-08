#pragma once

#include "Macros.h"

#include <d3d12.h>
#include <d3dcompiler.h>
#include "d3dx12.h"
#include <wrl.h>
#include <windows.h>

#include <cstdint>
#include <string>

namespace fw
{
uint32_t roundUpByteSize(uint32_t byteSize);
Microsoft::WRL::ComPtr<ID3DBlob> compileShader(const std::wstring& filename,
                                               const D3D_SHADER_MACRO* defines,
                                               const std::string& entrypoint,
                                               const std::string& target);

std::wstring stringToWstring(const std::string& str);

Microsoft::WRL::ComPtr<ID3D12Resource> createGPUBuffer(ID3D12Device* device,
                                                       ID3D12GraphicsCommandList* cmdList,
                                                       const void* initData,
                                                       UINT64 byteSize,
                                                       Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);
} // namespace fw