#pragma once

#include <fw/Camera.h>

#include <DirectXMath.h>
#include <wrl.h>
#include <d3d12.h>

#include <vector>

class MotionVector
{
public:
    MotionVector(){};
    ~MotionVector(){};

    bool initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList,
                    ID3D12DescriptorHeap* srvHeap,
                    int srvOffset);
    void postInitialize();
    void update(const fw::Camera& camera);
    void render(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList, ID3D12DescriptorHeap* srvHeap, int depthOffset);

private:
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_constantBuffers;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexUploadBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexUploadBuffer;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_motionVectorRenderTexture;

    Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO = nullptr;

    DirectX::XMMATRIX m_previousVPMatrix;

    void createConstantBuffer();
    void createVertexBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);
    void createRenderTarget(ID3D12DescriptorHeap* srvHeap, int srvOffset);
    void createShaders();
    void createRootSignature();
    void createMotionVectorPSO();
};
