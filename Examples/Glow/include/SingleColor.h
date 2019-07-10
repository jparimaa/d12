#pragma once

#include "Shared.h"

#include <wrl.h>
#include <d3d12.h>

#include <vector>

class SingleColor
{
public:
    SingleColor(){};
    ~SingleColor(){};

    bool initialize(const std::vector<RenderObject>* renderObjects);
    void render(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList, ID3D12DescriptorHeap* constantBufferHeap);

private:
    const std::vector<RenderObject>* m_renderObjects = nullptr;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_singleColorTextures;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap = nullptr;

    Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO = nullptr;

    void createRenderTarget();
    void createShaders();
    void createRootSignature();
    void createSingleColorPSO();
};
