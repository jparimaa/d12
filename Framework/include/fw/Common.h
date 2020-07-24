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
constexpr uint32_t roundUpByteSize(uint32_t byteSize)
{
    return (byteSize + 255) & ~255;
};

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

Microsoft::WRL::ComPtr<ID3D12Resource> createGPUTexture(ID3D12Device* device,
                                                        ID3D12GraphicsCommandList* cmdList,
                                                        const void* data,
                                                        UINT64 size,
                                                        const D3D12_RESOURCE_DESC& textureDesc,
                                                        int pixelSize,
                                                        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer,
                                                        std::wstring name = L"Texture");

template<typename T>
UINT uintSize(const T& container)
{
    return static_cast<UINT>(container.size());
}
} // namespace fw
