#include "GlowApp.h"

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

bool GlowApp::initialize()
{
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = fw::API::getCommandList();

    fw::Model model;
    loadModel(model);

    m_constantBufferOffset = 0;
    m_albedoTextureOffset = fw::API::getSwapChainBufferCount();
    m_singleColorTextureOffset = fw::API::getSwapChainBufferCount() + static_cast<int>(m_renderObjects.size());

    createDescriptorHeap();
    createConstantBuffer();
    createTextures(model, commandList);
    createVertexBuffers(model, commandList);

    createShaders();
    createRootSignature();

    createRenderPSO();

    m_singleColorRenderer.initialize(&m_renderObjects, m_cbvSrvDescriptorHeap.Get(), m_singleColorTextureOffset);
    m_blurRenderer.initialize(commandList);

    // Execute and wait initialization commands
    CHECK(commandList->Close());
    fw::API::completeInitialization();
    m_textureUploadBuffers.clear();
    m_vertexUploadBuffers.clear();
    m_blurRenderer.postInitialize();

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

void GlowApp::update()
{
    static fw::Transformation transformation;
    transformation.rotate(DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f), fw::API::getTimeDelta() * 0.1f);
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

    std::cout << fw::API::getTimeDelta() << "\n";
}

void GlowApp::fillCommandList()
{
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = fw::API::getCurrentFrameCommandAllocator();
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = fw::API::getCommandList();

    CHECK(commandAllocator->Reset());
    CHECK(commandList->Reset(commandAllocator.Get(), nullptr));

    ID3D12Resource* currentBackBuffer = fw::API::getCurrentBackBuffer();
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    commandList->RSSetScissorRects(1, &m_scissorRect);

    m_singleColorRenderer.render(commandList, m_cbvSrvDescriptorHeap.Get());

    CD3DX12_CPU_DESCRIPTOR_HANDLE currentBackBufferView = fw::API::getCurrentBackBufferView();
    const static float clearColor[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    commandList->ClearRenderTargetView(currentBackBufferView, clearColor, 0, nullptr);

    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = fw::API::getDepthStencilView();
    commandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    commandList->RSSetViewports(1, &m_screenViewport);
    commandList->OMSetRenderTargets(1, &currentBackBufferView, true, &depthStencilView);

    m_blurRenderer.render(commandList, m_cbvSrvDescriptorHeap.Get(), m_singleColorTextureOffset);

    commandList->SetPipelineState(m_finalRenderPSO.Get());

    std::vector<ID3D12DescriptorHeap*> descriptorHeaps{m_cbvSrvDescriptorHeap.Get()};
    commandList->SetDescriptorHeaps((UINT)descriptorHeaps.size(), descriptorHeaps.data());

    commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    int currentFrameIndex = fw::API::getCurrentFrameIndex();
    UINT cbvSrvIncrementSize = fw::API::getCbvSrvUavDescriptorIncrementSize();
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvHandleBegin = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_cbvSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

    CD3DX12_GPU_DESCRIPTOR_HANDLE constantBufferHandle = cbvSrvHandleBegin;
    constantBufferHandle.Offset(m_constantBufferOffset + currentFrameIndex, cbvSrvIncrementSize);
    commandList->SetGraphicsRootDescriptorTable(0, constantBufferHandle);

    commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    CD3DX12_GPU_DESCRIPTOR_HANDLE textureHandle = cbvSrvHandleBegin;
    textureHandle.Offset(m_albedoTextureOffset, cbvSrvIncrementSize);

    for (const RenderObject& ro : m_renderObjects)
    {
        commandList->SetGraphicsRootDescriptorTable(1, textureHandle);
        textureHandle.Offset(1, fw::API::getCbvSrvUavDescriptorIncrementSize());

        commandList->IASetVertexBuffers(0, 1, &ro.vertexBufferView);
        commandList->IASetIndexBuffer(&ro.indexBufferView);
        commandList->DrawIndexedInstanced(ro.numIndices, 1, 0, 0, 0);
    }

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    CHECK(commandList->Close());
}

void GlowApp::onGUI()
{
}

void GlowApp::loadModel(fw::Model& model)
{
    std::string modelFilepath = ASSET_PATH;
    modelFilepath += "attack_droid.obj";
    bool modelLoaded = model.loadModel(modelFilepath);
    assert(modelLoaded);
    const size_t numMeshes = model.getMeshes().size();
    m_renderObjects.resize(numMeshes);
}

void GlowApp::createDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc;
    size_t numMeshes = m_renderObjects.size();
    int numDescriptors = (2 * fw::API::getSwapChainBufferCount()) + static_cast<int>(numMeshes);
    // 0-1: CBV
    // 2-3: Albedo
    // 4-5: Single color input
    descriptorHeapDesc.NumDescriptors = numDescriptors;
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descriptorHeapDesc.NodeMask = 0;

    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();
    CHECK(d3dDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_cbvSrvDescriptorHeap)));
}

void GlowApp::createConstantBuffer()
{
    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();

    uint32_t constantBufferSize = fw::roundUpByteSize(sizeof(DirectX::XMFLOAT4X4));
    m_constantBuffers.resize(fw::API::getSwapChainBufferCount());

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cbvSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    for (int i = 0; i < m_constantBuffers.size(); ++i)
    {
        Microsoft::WRL::ComPtr<ID3D12Resource>& constantBuffer = m_constantBuffers[i];
        CHECK(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                                 D3D12_HEAP_FLAG_NONE,
                                                 &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
                                                 D3D12_RESOURCE_STATE_GENERIC_READ,
                                                 nullptr,
                                                 IID_PPV_ARGS(&constantBuffer)));

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = constantBufferSize;

        d3dDevice->CreateConstantBufferView(&cbvDesc, handle);
        handle.Offset(1, fw::API::getCbvSrvUavDescriptorIncrementSize());
    }
}

void GlowApp::createTextures(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList)
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

    CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cbvSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    handle.Offset(fw::API::getSwapChainBufferCount(), fw::API::getCbvSrvUavDescriptorIncrementSize());

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

void GlowApp::createVertexBuffers(const fw::Model& model, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList)
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

void GlowApp::createShaders()
{
    std::wstring shaderPath = fw::stringToWstring(std::string(SHADER_PATH));

    std::wstring finalRenderShaderPath = shaderPath;
    finalRenderShaderPath += L"finalRender.hlsl";
    m_finalRenderShaders.vertexShader = fw::compileShader(finalRenderShaderPath, nullptr, "VS", "vs_5_0");
    m_finalRenderShaders.pixelShader = fw::compileShader(finalRenderShaderPath, nullptr, "PS", "ps_5_0");
}

void GlowApp::createRootSignature()
{
    std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(2);

    std::vector<CD3DX12_DESCRIPTOR_RANGE> cbvDescriptorRanges(1);
    cbvDescriptorRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    rootParameters[0].InitAsDescriptorTable(fw::uintSize(cbvDescriptorRanges), cbvDescriptorRanges.data());

    std::vector<CD3DX12_DESCRIPTOR_RANGE> albedoTexture(1);
    albedoTexture[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    rootParameters[1].InitAsDescriptorTable(fw::uintSize(albedoTexture), albedoTexture.data());

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

void GlowApp::createRenderPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = {c_vertexInputLayout.data(), (UINT)c_vertexInputLayout.size()};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = {reinterpret_cast<BYTE*>(m_finalRenderShaders.vertexShader->GetBufferPointer()), m_finalRenderShaders.vertexShader->GetBufferSize()};
    psoDesc.PS = {reinterpret_cast<BYTE*>(m_finalRenderShaders.pixelShader->GetBufferPointer()), m_finalRenderShaders.pixelShader->GetBufferSize()};
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
    CHECK(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_finalRenderPSO)));
}
