#pragma once

#include <fw/Application.h>
#include <fw/Model.h>

#include "d3dx12.h"
#include <wrl.h>
#include <d3d12.h>

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

    void loadModel(fw::Model& model);
    void createVertexBuffers(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& commandList, std::vector<VertexUploadBuffers>& vertexUploadBuffers);
    void createBLAS(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>& commandList);
    void createTLAS();
};
