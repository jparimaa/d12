#pragma once

#include <fw/Application.h>
#include <fw/Camera.h>
#include <fw/CameraController.h>

#include <wrl.h>
#include <d3d12.h>

#include <vector>

class MinimalApp : public fw::Application
{
public:
    MinimalApp(){};
    virtual ~MinimalApp();
    MinimalApp(const MinimalApp&) = delete;
    MinimalApp(MinimalApp&&) = delete;
    MinimalApp& operator=(const MinimalApp&) = delete;
    MinimalApp& operator=(MinimalApp&&) = delete;

    virtual bool initialize() final;
    virtual void update() final;
    virtual void onGUI() final;

private:
    D3D12_VIEWPORT m_screenViewport;
    D3D12_RECT m_scissorRect;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_cbvHeap = nullptr;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_constantBuffers;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBufferGPU;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBufferGPU;

    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO = nullptr;

    fw::Camera m_camera;
    fw::CameraController m_cameraController;
};
