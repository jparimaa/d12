#pragma once

#include "ObjectRender.h"
#include "MotionVector.h"

#include <fw/Application.h>
#include <fw/Camera.h>
#include <fw/CameraController.h>
#include <fw/Model.h>

#include "d3dx12.h"
#include <wrl.h>
#include <d3d12.h>

#include <vector>
#include <cstdint>

class MotionBlurApp : public fw::Application
{
public:
    MotionBlurApp(){};
    virtual ~MotionBlurApp(){};

    virtual bool initialize() final;
    virtual void update() final;
    virtual void fillCommandList() final;
    virtual void onGUI() final;

private:
    D3D12_VIEWPORT m_screenViewport;
    D3D12_RECT m_scissorRect;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature = nullptr;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_descriptorHeap = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexUploadBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexUploadBuffer;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

    Microsoft::WRL::ComPtr<ID3DBlob> m_vertexShader = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> m_pixelShader = nullptr;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PSO = nullptr;

    ObjectRender m_objectRender;
    MotionVector m_motionVector;

    fw::Camera m_camera;
    fw::CameraController m_cameraController;

    void createDescriptorHeap();
    void createVertexBuffers(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList);
    void createShaders();
    void createRootSignature();
    void createRenderPSO();
};
