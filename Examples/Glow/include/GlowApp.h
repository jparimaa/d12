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

class GlowApp : public fw::Application
{
public:
    GlowApp(){};
    virtual ~GlowApp();
    GlowApp(const GlowApp&) = delete;
    GlowApp(GlowApp&&) = delete;
    GlowApp& operator=(const GlowApp&) = delete;
    GlowApp& operator=(GlowApp&&) = delete;

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
        Microsoft::WRL::ComPtr<ID3DBlob> vertexShader = nullptr;
        Microsoft::WRL::ComPtr<ID3DBlob> pixelShader = nullptr;
    };

    D3D12_VIEWPORT m_screenViewport;
    D3D12_RECT m_scissorRect;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvSrvDescriptorHeap = nullptr;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_constantBuffers;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvDescriptorHeap = nullptr;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_singleColorTextures;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_singleColorSrvHeap = nullptr;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_textureUploadBuffers;
    std::vector<VertexUploadBuffers> m_vertexUploadBuffers;

    Shaders m_singleColorShaders;
    Shaders m_finalRenderShaders;

    std::vector<RenderObject> m_renderObjects;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_singleColorPSO = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_finalRenderPSO = nullptr;

    fw::Camera m_camera;
    fw::CameraController m_cameraController;

    void loadModel(fw::Model& model);
    void createDescriptorHeap();
    void createConstantBuffer();
    void createTextures(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);
    void createVertexBuffers(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);
    void createShaders();
    void createRootSignature();
    void createRenderTarget();
    void createSingleColorPSO();
    void createBlurPSO();
    void createRenderPSO();
};
