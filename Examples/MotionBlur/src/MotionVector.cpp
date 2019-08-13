#include "MotionVector.h"
#include "Shared.h"

#include <fw/API.h>
#include <fw/Common.h>

namespace
{
const float c_clearColor[4] = {1.0f, 0.0f, 0.0f, 1.0f};
}

bool MotionVector::initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList,
                              ID3D12DescriptorHeap* srvHeap,
                              int srvOffset)
{
    createConstantBuffer();
    createVertexBuffer(commandList);
    createRenderTarget(srvHeap, srvOffset);
    createShaders();
    createRootSignature();
    createMotionVectorPSO();
    return true;
}

void MotionVector::postInitialize()
{
    m_vertexUploadBuffer.Reset();
    m_indexUploadBuffer.Reset();
}

void MotionVector::update(fw::Camera* camera)
{
    m_previousMatrix = m_currentMatrix;
    m_currentMatrix = camera->getViewMatrix() * camera->getProjectionMatrix();

    int currentFrameIndex = fw::API::getCurrentFrameIndex();
    char* mappedData = nullptr;
    CHECK(m_constantBuffers[currentFrameIndex]->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));
    memcpy(&mappedData[0], &m_previousMatrix, sizeof(DirectX::XMMATRIX));
    memcpy(&mappedData[sizeof(DirectX::XMMATRIX)], &m_currentMatrix, sizeof(DirectX::XMMATRIX));
    m_constantBuffers[currentFrameIndex]->Unmap(0, nullptr);
}

void MotionVector::render(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_motionVectorRenderTexture.Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

    commandList->SetPipelineState(m_PSO.Get());

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    commandList->ClearRenderTargetView(rtvHandle, c_clearColor, 0, nullptr);

    commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->OMSetRenderTargets(1, &rtvHandle, true, nullptr);

    commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    commandList->SetGraphicsRootConstantBufferView(0, m_constantBuffers[fw::API::getCurrentFrameIndex()]->GetGPUVirtualAddress());

    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->DrawIndexedInstanced(fw::uintSize(c_fullscreenTriangleIndices), 1, 0, 0, 0);

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_motionVectorRenderTexture.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
}

void MotionVector::createConstantBuffer()
{
    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();

    uint32_t constantBufferSize = fw::roundUpByteSize(2 * sizeof(DirectX::XMFLOAT4X4));
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

void MotionVector::createVertexBuffer(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();

    const size_t vertexBufferSize = c_fullscreenTriangle.size() * sizeof(float);
    const size_t indexBufferSize = c_fullscreenTriangleIndices.size() * sizeof(uint16_t);

    m_vertexBuffer = fw::createGPUBuffer(d3dDevice.Get(), commandList.Get(), c_fullscreenTriangle.data(), vertexBufferSize, m_vertexUploadBuffer);
    m_indexBuffer = fw::createGPUBuffer(d3dDevice.Get(), commandList.Get(), c_fullscreenTriangleIndices.data(), indexBufferSize, m_indexUploadBuffer);

    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(float) * 5;
    m_vertexBufferView.SizeInBytes = (UINT)vertexBufferSize;

    m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_indexBufferView.SizeInBytes = (UINT)indexBufferSize;
}

void MotionVector::createRenderTarget(ID3D12DescriptorHeap* srvHeap, int srvOffset)
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
                                             IID_PPV_ARGS(m_motionVectorRenderTexture.GetAddressOf())));

    // Create RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
    rtvDescriptorHeapDesc.NumDescriptors = 1;
    rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvDescriptorHeapDesc.NodeMask = 0;

    CHECK(d3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    // Create views
    D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
    renderTargetViewDesc.Texture2D.MipSlice = 0;
    renderTargetViewDesc.Texture2D.PlaneSlice = 0;
    renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    renderTargetViewDesc.Format = fw::API::getBackBufferFormat();

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    d3dDevice->CreateRenderTargetView(m_motionVectorRenderTexture.Get(), &renderTargetViewDesc, rtvHandle);

    // Create shader resource views
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Format = fw::API::getBackBufferFormat();
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvHeap->GetCPUDescriptorHandleForHeapStart());
    srvHandle.Offset(srvOffset, fw::API::getCbvSrvUavDescriptorIncrementSize());

    d3dDevice->CreateShaderResourceView(m_motionVectorRenderTexture.Get(), &srvDesc, srvHandle);
}

void MotionVector::createShaders()
{
    std::wstring shaderPath = fw::stringToWstring(std::string(SHADER_PATH));

    std::wstring motionVectorShaderPath = shaderPath;
    motionVectorShaderPath += L"motionVector.hlsl";
    m_vertexShader = fw::compileShader(motionVectorShaderPath, nullptr, "VS", "vs_5_0");
    m_pixelShader = fw::compileShader(motionVectorShaderPath, nullptr, "PS", "ps_5_0");
}

void MotionVector::createRootSignature()
{
    std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(1);
    rootParameters[0].InitAsConstantBufferView(0);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(fw::uintSize(rootParameters), rootParameters.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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

void MotionVector::createMotionVectorPSO()
{
    const std::vector<D3D12_INPUT_ELEMENT_DESC> vertexInputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = {vertexInputLayout.data(), (UINT)vertexInputLayout.size()};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = {reinterpret_cast<BYTE*>(m_vertexShader->GetBufferPointer()), m_vertexShader->GetBufferSize()};
    psoDesc.PS = {reinterpret_cast<BYTE*>(m_pixelShader->GetBufferPointer()), m_pixelShader->GetBufferSize()};
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = false;
    psoDesc.DepthStencilState.StencilEnable = false;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = fw::API::getBackBufferFormat();
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();
    CHECK(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));
}
