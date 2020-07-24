#pragma once

#include <fw/Application.h>
#include <fw/Model.h>
#include <fw/Camera.h>
#include <fw/CameraController.h>

#include <wrl.h>
#include "d3dx12.h"
#include <d3d12.h>
#include <dxcapi.h>

#include <vector>

class DXRApp : public fw::Application
{
public:
    DXRApp(){};
    virtual ~DXRApp(){};

    virtual bool initialize() override final;
    virtual void update() override final;
    virtual void fillCommandList() override final;
    virtual void onGUI() override final;

private:
    struct RenderObject
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
        D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
        D3D12_INDEX_BUFFER_VIEW indexBufferView;
        UINT vertexCount;
        UINT indexCount;
    };

    struct VertexUploadBuffers
    {
        Microsoft::WRL::ComPtr<ID3D12Resource> vertexUploadBuffer;
        Microsoft::WRL::ComPtr<ID3D12Resource> indexUploadBuffer;
    };

    std::vector<RenderObject> m_renderObjects;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_blasBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_tlasBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_tlasInstanceDescsBuffer;

    Microsoft::WRL::ComPtr<IDxcBlob> m_rayGenShader;
    Microsoft::WRL::ComPtr<IDxcBlob> m_missShader;
    Microsoft::WRL::ComPtr<IDxcBlob> m_hitShader;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rayGenRootSignature;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_missRootSignature;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_hitRootSignature;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_globalRootSignature;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_dummyLocalRootSignature;

    Microsoft::WRL::ComPtr<ID3D12StateObject> m_stateObject;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_outputBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_cameraBuffer;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_srvUavHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_sbtBuffer;

    UINT m_rayGenEntrySize = 0;
    UINT m_rayGenSectionSize = 0;
    UINT m_missEntrySize = 0;
    UINT m_missSectionSize = 0;
    UINT m_hitEntrySize = 0;
    UINT m_hitSectionSize = 0;

    D3D12_VIEWPORT m_viewport;
    D3D12_RECT m_scissorRect;

    fw::Camera m_camera;
    fw::CameraController m_cameraController;

    void loadModel(fw::Model& model);
    void createVertexBuffers(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& commandList, std::vector<VertexUploadBuffers>& vertexUploadBuffers);
    void createBLAS(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& commandList);
    void createTLAS(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& commandList);
    void createShaders();
    void createRayGenRootSignature();
    void createMissRootSignature();
    void createHitRootSignature();
    void createGlobalRootSignature();
    void createDummyRootSignature();
    void createStateObject();
    void createOutputBuffer();
    void createCameraBuffer();
    void createDescriptorHeap();
    void createShaderBindingTable();
};
