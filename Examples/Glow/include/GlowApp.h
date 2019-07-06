#pragma once

#include <fw/Application.h>
#include <fw/Camera.h>
#include <fw/CameraController.h>

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

    struct UploadBuffers
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> vertexUploadBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> indexUploadBuffer;
    };

    D3D12_VIEWPORT m_screenViewport;
    D3D12_RECT m_scissorRect;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap = nullptr;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_constantBuffers;

    Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader = nullptr;

    std::vector<RenderObject> m_renderObjects;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO = nullptr;

    fw::Camera m_camera;
    fw::CameraController m_cameraController;
};
