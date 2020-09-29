#include "RWTextureApp.h"

#include <fw/Framework.h>
#include <fw/Common.h>
#include <fw/API.h>
#include <fw/Macros.h>
#include <fw/Transformation.h>

#include <DirectXMath.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <GLFW/glfw3.h>

#include <vector>
#include <string>

namespace
{
const float c_clearColor[4] = {0.0, 0.0f, 0.1f, 1.0f};
const DXGI_FORMAT c_dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
const DXGI_FORMAT c_rtvFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;

// clang-format off
const std::vector<float> c_fullscreenTriangleVertices = {
    -1.0f,  1.0f, 0.0f, 0.0f, 0.0f,
     3.0f,  1.0f, 0.0f, 2.0f, 0.0f,
    -1.0f, -3.0f, 0.0f, 0.0f, 2.0f,
};

const std::vector<uint16_t> c_fullscreenTriangleIndices = {0, 1, 2};
// clang-format on
} // namespace

bool RWTextureApp::initialize()
{
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = fw::API::getCommandList();

    fw::Model model;
    loadModel(model);

    createDescriptorHeap();
    createConstantBuffer();
    createTextures(model, commandList);
    createVertexBuffers(model, commandList);

    createRenderShaders();
    createBlitShaders();
    createComputeShader();
    createRootSignature();
    createComputeRootSignature();

    createRenderPSO();
    createComputePSO();
    createBlitPSO();
    createRenderTexture(commandList);
    createFullscreenTriangleVertexBuffer(commandList);

    // Execute and wait initialization commands
    CHECK(commandList->Close());
    fw::API::completeInitialization();
    m_textureUploadBuffers.clear();
    m_vertexUploadBuffers.clear();

    // Camera
    m_cameraController.setCamera(&m_camera);
    m_camera.getTransformation().setPosition(0.0f, 10.0f, -50.0f);

    // Setup viewport and scissor
    int windowWidth = fw::API::getWindowWidth();
    int windowHeight = fw::API::getWindowHeight();

    m_screenViewport = {};
    m_screenViewport.TopLeftX = 0;
    m_screenViewport.TopLeftY = 0;
    m_screenViewport.Width = static_cast<float>(windowWidth);
    m_screenViewport.Height = static_cast<float>(windowHeight);
    m_screenViewport.MinDepth = 0.0f;
    m_screenViewport.MaxDepth = 1.0f;

    m_scissorRect = {0, 0, windowWidth, windowHeight};

    return true;
}

void RWTextureApp::update()
{
    static fw::Transformation transformation;
    transformation.rotate(DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), 0.0004f);
    transformation.updateWorldMatrix();

    m_cameraController.update();

    m_camera.updateViewMatrix();

    DirectX::XMMATRIX worldViewProj = transformation.getWorldMatrix() * m_camera.getViewMatrix() * m_camera.getProjectionMatrix();
    DirectX::XMMATRIX wvp = DirectX::XMMatrixTranspose(worldViewProj);

    int currentFrameIndex = fw::API::getCurrentFrameIndex();
    char* mappedData = nullptr;
    CHECK(m_constantBuffers[currentFrameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));
    memcpy(&mappedData[0], &wvp, sizeof(DirectX::XMMATRIX));
    m_constantBuffers[currentFrameIndex]->Unmap(0, nullptr);

    if (fw::API::isKeyReleased(GLFW_KEY_ESCAPE))
    {
        fw::API::quit();
    }
}

void RWTextureApp::fillCommandList()
{
    const int currentFrameIndex = fw::API::getCurrentFrameIndex();
    const int swapchainLength = fw::API::getSwapChainBufferCount();
    const int renderObjectCount = static_cast<int>(m_renderObjects.size());

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = fw::API::getCurrentFrameCommandAllocator();
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> cl = fw::API::getCommandList();

    CHECK(commandAllocator->Reset());
    CHECK(cl->Reset(commandAllocator.Get(), m_renderPSO.Get()));

    cl->RSSetViewports(1, &m_screenViewport);
    cl->RSSetScissorRects(1, &m_scissorRect);

    // Render
    {
        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
            rtvHandle.Offset(currentFrameIndex, fw::API::getRtvDescriptorIncrementSize());
            cl->ClearRenderTargetView(rtvHandle, c_clearColor, 0, nullptr);

            CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
            dsvHandle.Offset(currentFrameIndex, fw::API::getDsvDescriptorIncrementSize());
            cl->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

            cl->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);
        }

        cl->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        cl->SetGraphicsRootSignature(m_rootSignature.Get());
        cl->SetGraphicsRootConstantBufferView(0, m_constantBuffers[fw::API::getCurrentFrameIndex()]->GetGPUVirtualAddress());

        std::vector<ID3D12DescriptorHeap*> descriptorHeaps{m_srvHeap.Get()};
        cl->SetDescriptorHeaps((UINT)descriptorHeaps.size(), descriptorHeaps.data());

        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_srvHeap->GetGPUDescriptorHandleForHeapStart());

        for (const RenderObject& ro : m_renderObjects)
        {
            cl->SetGraphicsRootDescriptorTable(1, srvHandle);
            srvHandle.Offset(1, fw::API::getCbvSrvUavDescriptorIncrementSize());

            cl->IASetVertexBuffers(0, 1, &ro.vertexBufferView);
            cl->IASetIndexBuffer(&ro.indexBufferView);
            cl->DrawIndexedInstanced(ro.numIndices, 1, 0, 0, 0);
        }
    }

    ID3D12Resource* renderTexture = m_renderTextures[currentFrameIndex].Get();

    // Compute
    {
        cl->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(renderTexture, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

        cl->SetPipelineState(m_computePSO.Get());
        cl->SetComputeRootSignature(m_computeRootSignature.Get());
        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_srvHeap->GetGPUDescriptorHandleForHeapStart());
        srvHandle.Offset(renderObjectCount + swapchainLength + currentFrameIndex, fw::API::getCbvSrvUavDescriptorIncrementSize());

        cl->SetComputeRootDescriptorTable(0, srvHandle);
        cl->Dispatch(fw::API::getWindowWidth() / 8, fw::API::getWindowHeight() / 8, 1);
    }

    // Backbuffer blit
    {
        ID3D12Resource* backBuffer = fw::API::getCurrentBackBuffer();

        {
            std::vector<D3D12_RESOURCE_BARRIER> barriers{
                CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
                CD3DX12_RESOURCE_BARRIER::Transition(renderTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)};
            cl->ResourceBarrier(fw::uintSize(barriers), barriers.data());
        }

        {
            CD3DX12_CPU_DESCRIPTOR_HANDLE currentBackBufferView = fw::API::getCurrentBackBufferView();
            const static float clearColor[4] = {0.2f, 0.4f, 0.6f, 1.0f};
            cl->ClearRenderTargetView(currentBackBufferView, clearColor, 0, nullptr);

            D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = fw::API::getDepthStencilView();
            cl->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
            cl->OMSetRenderTargets(1, &currentBackBufferView, true, &depthStencilView);
        }

        cl->SetPipelineState(m_blitPSO.Get());
        cl->SetGraphicsRootSignature(m_rootSignature.Get());

        CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_srvHeap->GetGPUDescriptorHandleForHeapStart());
        srvHandle.Offset(renderObjectCount + currentFrameIndex, fw::API::getCbvSrvUavDescriptorIncrementSize());
        cl->SetGraphicsRootDescriptorTable(1, srvHandle);

        cl->IASetVertexBuffers(0, 1, &m_fullscreenTriangle.vertexBufferView);
        cl->IASetIndexBuffer(&m_fullscreenTriangle.indexBufferView);
        cl->DrawIndexedInstanced(m_fullscreenTriangle.numIndices, 1, 0, 0, 0);

        {
            std::vector<D3D12_RESOURCE_BARRIER> barriers{
                CD3DX12_RESOURCE_BARRIER::Transition(backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT),
                CD3DX12_RESOURCE_BARRIER::Transition(renderTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET)};
            cl->ResourceBarrier(fw::uintSize(barriers), barriers.data());
        }
    }

    CHECK(cl->Close());
}

void RWTextureApp::onGUI()
{
}

void RWTextureApp::loadModel(fw::Model& model)
{
    std::string modelFilepath = ASSET_PATH;
    modelFilepath += "attack_droid.obj";
    bool modelLoaded = model.loadModel(modelFilepath);
    assert(modelLoaded);
    const size_t numMeshes = model.getMeshes().size();
    m_renderObjects.resize(numMeshes);
}

void RWTextureApp::createDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc;
    size_t numMeshes = m_renderObjects.size() + (2 * fw::API::getSwapChainBufferCount());
    descriptorHeapDesc.NumDescriptors = static_cast<int>(numMeshes);
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descriptorHeapDesc.NodeMask = 0;

    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();
    CHECK(d3dDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_srvHeap)));
}

void RWTextureApp::createConstantBuffer()
{
    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();

    uint32_t constantBufferSize = fw::roundUpByteSize(sizeof(DirectX::XMFLOAT4X4));
    m_constantBuffers.resize(fw::API::getSwapChainBufferCount());

    for (int i = 0; i < m_constantBuffers.size(); ++i)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource>& constantBuffer = m_constantBuffers[i];
        CHECK(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                                 D3D12_HEAP_FLAG_NONE,
                                                 &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
                                                 D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 nullptr,
                                                 IID_PPV_ARGS(&constantBuffer)));
    }
}

void RWTextureApp::createTextures(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cl)
{
    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();

    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
    textureDesc.Width = 0;
    textureDesc.Height = 0;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    const fw::Model::Meshes& meshes = model.getMeshes();
    size_t numMeshes = meshes.size();
    m_textureUploadBuffers.resize(numMeshes);

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_srvHeap->GetCPUDescriptorHandleForHeapStart());

    for (size_t i = 0; i < numMeshes; ++i)
    {
        std::string textureName = meshes[i].getFirstTextureOfType(aiTextureType::aiTextureType_DIFFUSE);
        std::string filepath = ASSET_PATH;
        filepath += textureName;
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(filepath.c_str(), &texWidth, &texHeight, &texChannels, 4);
        assert(pixels != nullptr);

        textureDesc.Width = texWidth;
        textureDesc.Height = texHeight;
        const int numBytes = texWidth * texHeight * texChannels;

        RenderObject& ro = m_renderObjects[i];
        ro.texture = fw::createGPUTexture(d3dDevice.Get(), cl.Get(), pixels, numBytes, textureDesc, 4, m_textureUploadBuffers[i]);

        stbi_image_free(pixels);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = textureDesc.Format;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        d3dDevice->CreateShaderResourceView(ro.texture.Get(), &srvDesc, handle);

        handle.Offset(1, fw::API::getCbvSrvUavDescriptorIncrementSize());
    }
}

void RWTextureApp::createVertexBuffers(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cl)
{
    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();

    const fw::Model::Meshes& meshes = model.getMeshes();
    size_t numMeshes = meshes.size();
    m_vertexUploadBuffers.resize(numMeshes);

    for (size_t i = 0; i < numMeshes; ++i)
    {
        RenderObject& ro = m_renderObjects[i];
        const fw::Mesh& mesh = meshes[i];
        std::vector<fw::Mesh::Vertex> vertices = mesh.getVertices();
        const size_t vertexBufferSize = vertices.size() * sizeof(fw::Mesh::Vertex);
        const size_t indexBufferSize = mesh.indices.size() * sizeof(uint16_t);

        ro.vertexBuffer = fw::createGPUBuffer(d3dDevice.Get(), cl.Get(), vertices.data(), vertexBufferSize, m_vertexUploadBuffers[i].vertexUploadBuffer);
        ro.indexBuffer = fw::createGPUBuffer(d3dDevice.Get(), cl.Get(), mesh.indices.data(), indexBufferSize, m_vertexUploadBuffers[i].indexUploadBuffer);

        ro.vertexBufferView.BufferLocation = ro.vertexBuffer->GetGPUVirtualAddress();
        ro.vertexBufferView.StrideInBytes = sizeof(fw::Mesh::Vertex);
        ro.vertexBufferView.SizeInBytes = (UINT)vertexBufferSize;

        ro.indexBufferView.BufferLocation = ro.indexBuffer->GetGPUVirtualAddress();
        ro.indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        ro.indexBufferView.SizeInBytes = (UINT)indexBufferSize;

        ro.numIndices = static_cast<UINT>(mesh.indices.size());
    }
}

void RWTextureApp::createRenderShaders()
{
    std::wstring shaderFile = fw::stringToWstring(std::string(SHADER_PATH));
    shaderFile += L"simple.hlsl";
    m_renderShaders.vertex = fw::compileShader(shaderFile, nullptr, "VS", "vs_5_0");
    m_renderShaders.pixel = fw::compileShader(shaderFile, nullptr, "PS", "ps_5_0");
}

void RWTextureApp::createComputeShader()
{
    std::wstring shaderFile = fw::stringToWstring(std::string(SHADER_PATH));
    shaderFile += L"block_greyscale.hlsl";
    m_computeShader = fw::compileShader(shaderFile, nullptr, "main", "cs_5_0");
}

void RWTextureApp::createBlitShaders()
{
    std::wstring shaderFile = fw::stringToWstring(std::string(SHADER_PATH));
    shaderFile += L"blit.hlsl";
    m_blitShaders.vertex = fw::compileShader(shaderFile, nullptr, "VS", "vs_5_0");
    m_blitShaders.pixel = fw::compileShader(shaderFile, nullptr, "PS", "ps_5_0");
}

void RWTextureApp::createRootSignature()
{
    std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(2);
    rootParameters[0].InitAsConstantBufferView(0);

    std::vector<CD3DX12_DESCRIPTOR_RANGE> srvDescriptorRanges(1);
    srvDescriptorRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    rootParameters[1].InitAsDescriptorTable(fw::uintSize(srvDescriptorRanges), srvDescriptorRanges.data());

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 0;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(fw::uintSize(rootParameters), rootParameters.data(), 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    fw::serializeAndCreateRootSignature(fw::API::getD3dDevice().Get(), rootSigDesc, m_rootSignature);
}

void RWTextureApp::createComputeRootSignature()
{
    std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(1);

    std::vector<CD3DX12_DESCRIPTOR_RANGE> srvDescriptorRanges(1);
    srvDescriptorRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
    rootParameters[0].InitAsDescriptorTable(fw::uintSize(srvDescriptorRanges), srvDescriptorRanges.data());

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(fw::uintSize(rootParameters), rootParameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    fw::serializeAndCreateRootSignature(fw::API::getD3dDevice().Get(), rootSigDesc, m_computeRootSignature);
}

void RWTextureApp::createRenderPSO()
{
    const std::vector<D3D12_INPUT_ELEMENT_DESC> vertexInputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = {vertexInputLayout.data(), (UINT)vertexInputLayout.size()};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = {reinterpret_cast<BYTE*>(m_renderShaders.vertex->GetBufferPointer()), m_renderShaders.vertex->GetBufferSize()};
    psoDesc.PS = {reinterpret_cast<BYTE*>(m_renderShaders.pixel->GetBufferPointer()), m_renderShaders.pixel->GetBufferSize()};
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = c_rtvFormat;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.DSVFormat = c_dsvFormat;

    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();
    CHECK(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_renderPSO)));
}

void RWTextureApp::createComputePSO()
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = m_computeRootSignature.Get();
    psoDesc.CS = {reinterpret_cast<BYTE*>(m_computeShader->GetBufferPointer()), m_computeShader->GetBufferSize()};

    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();
    CHECK(d3dDevice->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_computePSO)));
}

void RWTextureApp::createBlitPSO()
{
    const std::vector<D3D12_INPUT_ELEMENT_DESC> vertexInputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = {vertexInputLayout.data(), (UINT)vertexInputLayout.size()};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = {reinterpret_cast<BYTE*>(m_blitShaders.vertex->GetBufferPointer()), m_blitShaders.vertex->GetBufferSize()};
    psoDesc.PS = {reinterpret_cast<BYTE*>(m_blitShaders.pixel->GetBufferPointer()), m_blitShaders.pixel->GetBufferSize()};
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = fw::API::getBackBufferFormat();
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.DSVFormat = fw::API::getDepthStencilFormat();

    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();
    CHECK(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_blitPSO)));
}

void RWTextureApp::createRenderTexture(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cl)
{
    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();
    const int swapChainSize = fw::API::getSwapChainBufferCount();

    // Create texture resources
    D3D12_RESOURCE_DESC resourceDesc{};
    {
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Alignment = 0;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.MipLevels = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.Width = fw::API::getWindowWidth();
        resourceDesc.Height = fw::API::getWindowHeight();
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        resourceDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

        D3D12_CLEAR_VALUE clearValue{};
        clearValue.Color[0] = c_clearColor[0];
        clearValue.Color[1] = c_clearColor[1];
        clearValue.Color[2] = c_clearColor[2];
        clearValue.Color[3] = c_clearColor[3];
        clearValue.Format = resourceDesc.Format;

        CD3DX12_HEAP_PROPERTIES heapProperty(D3D12_HEAP_TYPE_DEFAULT);
        m_renderTextures.resize(swapChainSize);

        for (int i = 0; i < swapChainSize; ++i)
        {
            CHECK(d3dDevice->CreateCommittedResource(&heapProperty,
                                                     D3D12_HEAP_FLAG_NONE,
                                                     &resourceDesc,
                                                     D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                     &clearValue,
                                                     IID_PPV_ARGS(m_renderTextures[i].GetAddressOf())));
        }
    }

    // Create RTV heap and render target views
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
        rtvDescriptorHeapDesc.NumDescriptors = swapChainSize;
        rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtvDescriptorHeapDesc.NodeMask = 0;

        CHECK(d3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

        D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
        renderTargetViewDesc.Texture2D.MipSlice = 0;
        renderTargetViewDesc.Texture2D.PlaneSlice = 0;
        renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        renderTargetViewDesc.Format = resourceDesc.Format;

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

        for (int i = 0; i < swapChainSize; ++i)
        {
            d3dDevice->CreateRenderTargetView(m_renderTextures[i].Get(), &renderTargetViewDesc, rtvHandle);
            rtvHandle.Offset(1, fw::API::getRtvDescriptorIncrementSize());
        }
    }

    // Create SRV and UAV
    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
        srvDesc.Texture2D.MostDetailedMip = 0;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = resourceDesc.Format;

        const int offset = static_cast<int>(m_renderObjects.size());
        CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_srvHeap->GetCPUDescriptorHandleForHeapStart());
        srvHandle.Offset(offset, fw::API::getCbvSrvUavDescriptorIncrementSize());

        for (int i = 0; i < swapChainSize; ++i)
        {
            d3dDevice->CreateShaderResourceView(m_renderTextures[i].Get(), &srvDesc, srvHandle);
            srvHandle.Offset(1, fw::API::getCbvSrvUavDescriptorIncrementSize());
        }

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Texture2D.MipSlice = 0;
        uavDesc.Texture2D.PlaneSlice = 0;
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        uavDesc.Format = resourceDesc.Format;

        for (int i = 0; i < swapChainSize; ++i)
        {
            d3dDevice->CreateUnorderedAccessView(m_renderTextures[i].Get(), nullptr, &uavDesc, srvHandle);
            srvHandle.Offset(1, fw::API::getCbvSrvUavDescriptorIncrementSize());
        }
    }

    // Create the depth stencil buffers
    {
        D3D12_RESOURCE_DESC depthStencilDesc{};
        depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthStencilDesc.Alignment = 0;
        depthStencilDesc.Width = resourceDesc.Width;
        depthStencilDesc.Height = resourceDesc.Height;
        depthStencilDesc.DepthOrArraySize = 1;
        depthStencilDesc.MipLevels = 1;
        depthStencilDesc.Format = c_dsvFormat;
        depthStencilDesc.SampleDesc.Count = 1;
        depthStencilDesc.SampleDesc.Quality = 0;
        depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_CLEAR_VALUE optClear;
        optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        optClear.DepthStencil.Depth = 1.0f;
        optClear.DepthStencil.Stencil = 0;

        m_depthStencilBuffers.resize(swapChainSize);
        for (int i = 0; i < swapChainSize; ++i)
        {
            CHECK(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                     D3D12_HEAP_FLAG_NONE,
                                                     &depthStencilDesc,
                                                     D3D12_RESOURCE_STATE_COMMON,
                                                     &optClear,
                                                     IID_PPV_ARGS(m_depthStencilBuffers[i].GetAddressOf())));
        }
    }

    // Create dsv heap
    {
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
        dsvHeapDesc.NumDescriptors = swapChainSize;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dsvHeapDesc.NodeMask = 0;
        CHECK(d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf())));
    }

    // Create depth stencil views
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Format = c_dsvFormat;
        dsvDesc.Texture2D.MipSlice = 0;

        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

        for (int i = 0; i < swapChainSize; ++i)
        {
            d3dDevice->CreateDepthStencilView(m_depthStencilBuffers[i].Get(), &dsvDesc, dsvHandle);
            dsvHandle.Offset(1, fw::API::getDsvDescriptorIncrementSize());
            cl->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencilBuffers[i].Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
        }
    }
}

void RWTextureApp::createFullscreenTriangleVertexBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& cl)
{
    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();

    const size_t vertexBufferSize = c_fullscreenTriangleVertices.size() * sizeof(float);
    const size_t indexBufferSize = c_fullscreenTriangleIndices.size() * sizeof(uint16_t);

    m_vertexUploadBuffers.resize(m_vertexUploadBuffers.size() + 1);
    VertexUploadBuffers& uploadBuffer = m_vertexUploadBuffers.back();
    ID3D12Device* device = d3dDevice.Get();

    RenderObject& fst = m_fullscreenTriangle;
    fst.vertexBuffer = fw::createGPUBuffer(device, cl.Get(), c_fullscreenTriangleVertices.data(), vertexBufferSize, uploadBuffer.vertexUploadBuffer);
    fst.indexBuffer = fw::createGPUBuffer(device, cl.Get(), c_fullscreenTriangleIndices.data(), indexBufferSize, uploadBuffer.indexUploadBuffer);

    fst.vertexBufferView.BufferLocation = fst.vertexBuffer->GetGPUVirtualAddress();
    fst.vertexBufferView.StrideInBytes = sizeof(float) * 5;
    fst.vertexBufferView.SizeInBytes = (UINT)vertexBufferSize;

    fst.indexBufferView.BufferLocation = fst.indexBuffer->GetGPUVirtualAddress();
    fst.indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    fst.indexBufferView.SizeInBytes = (UINT)indexBufferSize;

    fst.numIndices = fw::uintSize(c_fullscreenTriangleIndices);
}
