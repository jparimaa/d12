#include "ObjectRender.h"

#include <fw/Transformation.h>
#include <fw/API.h>
#include <fw/Common.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace
{
const float c_clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
}

bool ObjectRender::initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList, ID3D12DescriptorHeap* srvHeap, int textureOffset, int srvOffset)
{
    m_srvHeap = srvHeap;
    m_textureOffset = textureOffset;

    fw::Model model;
    loadModel(model);

    createConstantBuffer();
    createTextures(model, commandList, srvHeap, textureOffset);
    createVertexBuffers(model, commandList);

    createRenderTarget(srvHeap, srvOffset);
    createShaders();
    createRootSignature();
    createObjectRenderPSO();

    return true;
}

void ObjectRender::postInitialize()
{
    m_textureUploadBuffers.clear();
    m_vertexUploadBuffers.clear();
}

void ObjectRender::update(fw::Camera* camera)
{
    static fw::Transformation transformation;
    transformation.rotate(DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), 0.4f * fw::API::getTimeDelta());
    transformation.updateWorldMatrix();

    DirectX::XMMATRIX worldViewProj = transformation.getWorldMatrix() * camera->getViewMatrix() * camera->getProjectionMatrix();
    DirectX::XMMATRIX wvp = DirectX::XMMatrixTranspose(worldViewProj);

    int currentFrameIndex = fw::API::getCurrentFrameIndex();
    char* mappedData = nullptr;
    CHECK(m_constantBuffers[currentFrameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));
    memcpy(&mappedData[0], &wvp, sizeof(DirectX::XMMATRIX));
    m_constantBuffers[currentFrameIndex]->Unmap(0, nullptr);
}

void ObjectRender::render(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    commandList->SetPipelineState(m_PSO.Get());
    int currentFrameIndex = fw::API::getCurrentFrameIndex();

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_objectRenderTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    commandList->ClearRenderTargetView(rtvHandle, c_clearColor, 0, nullptr);

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffers[fw::API::getCurrentFrameIndex()]->GetGPUVirtualAddress());

    std::vector<ID3D12DescriptorHeap*> descriptorHeaps{m_srvHeap};
    commandList->SetDescriptorHeaps((UINT)descriptorHeaps.size(), descriptorHeaps.data());

    CD3DX12_GPU_DESCRIPTOR_HANDLE textureHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_srvHeap->GetGPUDescriptorHandleForHeapStart());
    textureHandle.Offset(m_textureOffset, fw::API::getCbvSrvUavDescriptorIncrementSize());

    for (const RenderObject& ro : m_renderObjects)
    {
        commandList->SetGraphicsRootDescriptorTable(1, textureHandle);
        textureHandle.Offset(1, fw::API::getCbvSrvUavDescriptorIncrementSize());

        commandList->IASetVertexBuffers(0, 1, &ro.vertexBufferView);
        commandList->IASetIndexBuffer(&ro.indexBufferView);
        commandList->DrawIndexedInstanced(ro.numIndices, 1, 0, 0, 0);
    }

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_objectRenderTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
}

void ObjectRender::loadModel(fw::Model& model)
{
    std::string modelFilepath = ASSET_PATH;
    modelFilepath += "attack_droid.obj";
    bool modelLoaded = model.loadModel(modelFilepath);
    assert(modelLoaded);
    const size_t numMeshes = model.getMeshes().size();
    m_renderObjects.resize(numMeshes);
}

void ObjectRender::createConstantBuffer()
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

void ObjectRender::createTextures(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList, ID3D12DescriptorHeap* srvHeap, int textureOffset)
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

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvHeap->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(textureOffset, fw::API::getCbvSrvUavDescriptorIncrementSize());

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
        ro.texture = fw::createGPUTexture(d3dDevice.Get(), commandList.Get(), pixels, numBytes, textureDesc, 4, m_textureUploadBuffers[i]);

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

void ObjectRender::createVertexBuffers(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList)
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

        ro.vertexBuffer = fw::createGPUBuffer(d3dDevice.Get(), commandList.Get(), vertices.data(), vertexBufferSize, m_vertexUploadBuffers[i].vertexUploadBuffer);
        ro.indexBuffer = fw::createGPUBuffer(d3dDevice.Get(), commandList.Get(), mesh.indices.data(), indexBufferSize, m_vertexUploadBuffers[i].indexUploadBuffer);

        ro.vertexBufferView.BufferLocation = ro.vertexBuffer->GetGPUVirtualAddress();
        ro.vertexBufferView.StrideInBytes = sizeof(fw::Mesh::Vertex);
        ro.vertexBufferView.SizeInBytes = (UINT)vertexBufferSize;

        ro.indexBufferView.BufferLocation = ro.indexBuffer->GetGPUVirtualAddress();
        ro.indexBufferView.Format = DXGI_FORMAT_R16_UINT;
        ro.indexBufferView.SizeInBytes = (UINT)indexBufferSize;

        ro.numIndices = static_cast<UINT>(mesh.indices.size());
    }
}

void ObjectRender::createRenderTarget(ID3D12DescriptorHeap* srvHeap, int srvOffset)
{
    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();

    // Create render texture resources
    UINT windowWidth = static_cast<UINT>(fw::API::getWindowWidth());
    UINT windowHeight = static_cast<UINT>(fw::API::getWindowHeight());

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Width = windowWidth;
    resourceDesc.Height = windowHeight;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    resourceDesc.Format = fw::API::getBackBufferFormat();

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Color[0] = c_clearColor[0];
    clearValue.Color[1] = c_clearColor[1];
    clearValue.Color[2] = c_clearColor[2];
    clearValue.Color[3] = c_clearColor[3];
    clearValue.Format = fw::API::getBackBufferFormat();

    CD3DX12_HEAP_PROPERTIES heapProperty(D3D12_HEAP_TYPE_DEFAULT);

    CHECK(d3dDevice->CreateCommittedResource(&heapProperty,
                                             D3D12_HEAP_FLAG_NONE,
                                             &resourceDesc,
                                             D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                             &clearValue,
                                             IID_PPV_ARGS(m_objectRenderTexture.GetAddressOf())));

    // Create depth texture resource
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    resourceDesc.Format = fw::API::getDepthStencilFormat();
    clearValue.Format = fw::API::getDepthStencilFormat();
    clearValue.DepthStencil.Depth = 1.0f;

    CHECK(d3dDevice->CreateCommittedResource(&heapProperty,
                                             D3D12_HEAP_FLAG_NONE,
                                             &resourceDesc,
                                             D3D12_RESOURCE_STATE_DEPTH_WRITE,
                                             &clearValue,
                                             IID_PPV_ARGS(m_depthStencilTexture.GetAddressOf())));

    // Create RTV and DSV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
    rtvDescriptorHeapDesc.NumDescriptors = 1;
    rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvDescriptorHeapDesc.NodeMask = 0;

    CHECK(d3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc{};
    dsvDescriptorHeapDesc.NumDescriptors = 1;
    dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvDescriptorHeapDesc.NodeMask = 0;

    CHECK(d3dDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&m_dsvHeap)));

    // Create views
    D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
    renderTargetViewDesc.Texture2D.MipSlice = 0;
    renderTargetViewDesc.Texture2D.PlaneSlice = 0;
    renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    renderTargetViewDesc.Format = fw::API::getBackBufferFormat();

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    d3dDevice->CreateRenderTargetView(m_objectRenderTexture.Get(), &renderTargetViewDesc, rtvHandle);

    D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilViewDesc{};
    depthStencilViewDesc.Texture2D.MipSlice = 0;
    depthStencilViewDesc.Format = fw::API::getDepthStencilFormat();
    depthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    depthStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    d3dDevice->CreateDepthStencilView(m_depthStencilTexture.Get(), &depthStencilViewDesc, dsvHandle);

    // Create shader resource views
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Format = fw::API::getBackBufferFormat();
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = fw::API::getBackBufferFormat();

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvHeap->GetCPUDescriptorHandleForHeapStart());
    srvHandle.Offset(srvOffset, fw::API::getCbvSrvUavDescriptorIncrementSize());

    d3dDevice->CreateShaderResourceView(m_objectRenderTexture.Get(), &srvDesc, srvHandle);
    srvHandle.Offset(1, fw::API::getCbvSrvUavDescriptorIncrementSize());

    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    d3dDevice->CreateShaderResourceView(m_depthStencilTexture.Get(), &srvDesc, srvHandle);
}

void ObjectRender::createShaders()
{
    std::wstring shaderPath = fw::stringToWstring(std::string(SHADER_PATH));

    std::wstring singleColorShaderPath = shaderPath;
    singleColorShaderPath += L"objectRender.hlsl";
    m_vertexShader = fw::compileShader(singleColorShaderPath, nullptr, "VS", "vs_5_0");
    m_pixelShader = fw::compileShader(singleColorShaderPath, nullptr, "PS", "ps_5_0");
}

void ObjectRender::createRootSignature()
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

    Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    CHECK(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));

    if (errorBlob != nullptr)
    {
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }

    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();
    CHECK(d3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));
}

void ObjectRender::createObjectRenderPSO()
{
    const std::vector<D3D12_INPUT_ELEMENT_DESC> vertexInputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = {vertexInputLayout.data(), (UINT)vertexInputLayout.size()};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = {reinterpret_cast<BYTE*>(m_vertexShader->GetBufferPointer()), m_vertexShader->GetBufferSize()};
    psoDesc.PS = {reinterpret_cast<BYTE*>(m_pixelShader->GetBufferPointer()), m_pixelShader->GetBufferSize()};
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
    CHECK(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));
}
