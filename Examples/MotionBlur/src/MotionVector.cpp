#include "MotionVector.h"
#include "Shared.h"

#include <fw/API.h>
#include <fw/Common.h>

bool MotionVector::initialize(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList)
{
    createVertexBuffer(commandList);
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

void MotionVector::render(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList, ID3D12DescriptorHeap* srvHeap, int srvHeapOffset)
{
    commandList->SetPipelineState(m_PSO.Get());

    std::vector<ID3D12DescriptorHeap*> descriptorHeaps{srvHeap};
    commandList->SetDescriptorHeaps((UINT)descriptorHeaps.size(), descriptorHeaps.data());

    commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    UINT cbvSrvIncrementSize = fw::API::getCbvSrvUavDescriptorIncrementSize();
    CD3DX12_GPU_DESCRIPTOR_HANDLE cbvSrvHandleBegin = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvHeap->GetGPUDescriptorHandleForHeapStart());

    CD3DX12_GPU_DESCRIPTOR_HANDLE singleColorTextureHandle = cbvSrvHandleBegin;
    int currentFrameIndex = fw::API::getCurrentFrameIndex();
    singleColorTextureHandle.Offset(srvHeapOffset + currentFrameIndex, cbvSrvIncrementSize);
    commandList->SetGraphicsRootDescriptorTable(0, singleColorTextureHandle);

    commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->DrawIndexedInstanced(fw::uintSize(c_fullscreenTriangleIndices), 1, 0, 0, 0);
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

void MotionVector::createShaders()
{
    std::wstring shaderPath = fw::stringToWstring(std::string(SHADER_PATH));

    std::wstring motionVectorShaderPath = shaderPath;
    motionVectorShaderPath += L"singlePassMotionVector.hlsl";
    m_vertexShader = fw::compileShader(motionVectorShaderPath, nullptr, "VS", "vs_5_0");
    m_pixelShader = fw::compileShader(motionVectorShaderPath, nullptr, "PS", "ps_5_0");
}

void MotionVector::createRootSignature()
{
    std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(1);

    std::vector<CD3DX12_DESCRIPTOR_RANGE> singleColorTexture(1);
    singleColorTexture[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    rootParameters[0].InitAsDescriptorTable(fw::uintSize(singleColorTexture), singleColorTexture.data());

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
