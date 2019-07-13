#pragma once

#include <wrl.h>
#include <d3d12.h>

#include <vector>

class Blur
{
public:
    Blur(){};
    ~Blur(){};

    bool initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);
    void postInitialize();
    void render(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexUploadBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexUploadBuffer;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO = nullptr;

    void createVertexBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);
    void createShaders();
    void createRootSignature();
    void createBlurPSO();
};
