#pragma once

#include <fw/Application.h>
#include <fw/Camera.h>
#include <fw/CameraController.h>
#include <fw/Model.h>

#include "d3dx12.h"
#include <wrl.h>
#include <d3d12.h>

#include <vector>
#include <cstdint>

class RWTextureApp : public fw::Application
{
public:
    RWTextureApp(){};
    virtual ~RWTextureApp(){};

    virtual bool initialize() final;
    virtual void update() final;
    virtual void fillCommandList() final;
    virtual void onGUI() final;

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

    struct Shaders
    {
        Microsoft::WRL::ComPtr<ID3DBlob> vertex = nullptr;
        Microsoft::WRL::ComPtr<ID3DBlob> pixel = nullptr;
    };

    D3D12_VIEWPORT m_screenViewport;
    D3D12_RECT m_scissorRect;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvHeap = nullptr;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_constantBuffers;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_textureUploadBuffers;
    std::vector<VertexUploadBuffers> m_vertexUploadBuffers;

    Shaders m_renderShaders;
    Shaders m_blitShaders;

    std::vector<RenderObject> m_renderObjects;
    RenderObject m_fullscreenTriangle;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_renderPSO = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_blitPSO = nullptr;

    fw::Camera m_camera;
    fw::CameraController m_cameraController;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_renderTextures;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_depthStencilBuffers;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap = nullptr;

    void loadModel(fw::Model& model);
    void createDescriptorHeap();
    void createConstantBuffer();
    void createTextures(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cl);
    void createVertexBuffers(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cl);
    void createRenderShaders();
    void createBlitShaders();
    void createRootSignature();
    void createRenderPSO();
    void createBlitPSO();
    void createRenderTexture(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cl);
    void createFullscreenTriangleVertexBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cl);
};
