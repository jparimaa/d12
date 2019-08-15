#pragma once

#include "Shared.h"

#include <fw/Camera.h>
#include <fw/Model.h>

#include <wrl.h>
#include <d3d12.h>

#include <vector>

class ObjectRender
{
public:
    ObjectRender(){};
    ~ObjectRender(){};

    bool initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList,
                    ID3D12DescriptorHeap* srvHeap,
                    int textureOffset,
                    int renderTextureOffset,
                    int renderDepthOffset);
    void postInitialize();
    void update(const fw::Camera& camera);
    void render(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);

private:
    struct RenderObject
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
        D3D12_INDEX_BUFFER_VIEW indexBufferView;
        Microsoft::WRL::ComPtr<ID3D12Resource> texture;
        UINT numIndices;
    };

    struct VertexUploadBuffers
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> vertexUploadBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> indexUploadBuffer;
    };

    std::vector<RenderObject> m_renderObjects;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_constantBuffers;

    ID3D12DescriptorHeap* m_srvHeap = nullptr;
    int m_textureOffset = -1;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_textureUploadBuffers;
    std::vector<VertexUploadBuffers> m_vertexUploadBuffers;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_objectRenderTexture;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilTexture;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap = nullptr;

    Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO = nullptr;

    void loadModel(fw::Model& model);
    void createConstantBuffer();
    void createTextures(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList, ID3D12DescriptorHeap* srvHeap, int textureOffset);
    void createVertexBuffers(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);
    void createRenderTarget(ID3D12DescriptorHeap* srvHeap, int renderTextureOffset, int renderDepthOffset);
    void createShaders();
    void createRootSignature();
    void createObjectRenderPSO();
};
