#pragma once

#include "Shared.h"
#include "SingleColor.h"

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
    virtual ~GlowApp(){};
    GlowApp(const GlowApp&) = delete;
    GlowApp(GlowApp&&) = delete;
    GlowApp& operator=(const GlowApp&) = delete;
    GlowApp& operator=(GlowApp&&) = delete;

    virtual bool initialize() final;
    virtual void update() final;
    virtual void fillCommandList() final;
    virtual void onGUI() final;

private:
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

    std::vector<RenderObject> m_renderObjects;

    D3D12_VIEWPORT m_screenViewport;
    D3D12_RECT m_scissorRect;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvSrvDescriptorHeap = nullptr;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_constantBuffers;

    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_textureUploadBuffers;
    std::vector<VertexUploadBuffers> m_vertexUploadBuffers;

    Shaders m_finalRenderShaders;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_finalRenderPSO = nullptr;

    int m_constantBufferOffset = 0;
    int m_albedoTextureOffset = 0;
    int m_singleColorTextureOffset = 0;

    SingleColor m_singleColorRenderer;

    fw::Camera m_camera;
    fw::CameraController m_cameraController;

    void loadModel(fw::Model& model);
    void createDescriptorHeap();
    void createConstantBuffer();
    void createTextures(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);
    void createVertexBuffers(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);
    void createShaders();
    void createRootSignature();
    void createRenderPSO();
};
