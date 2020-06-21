#pragma once

#include <fw/Application.h>
#include <fw/Model.h>

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

    void loadModel(fw::Model& model);
    void createVertexBuffers(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& commandList, std::vector<VertexUploadBuffers>& vertexUploadBuffers);
    void createBLAS(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& commandList);
    void createTLAS(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& commandList);
    void createShaders();
    void createRayGenRootSignature();
    void createEmptyRootSignature(Microsoft::WRL::ComPtr<ID3D12RootSignature>& rootSignature);
    void createPSO();
};
