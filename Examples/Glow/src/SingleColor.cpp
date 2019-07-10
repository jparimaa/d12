#include "SingleColor.h"

#include <fw/API.h>
#include <fw/Common.h>

bool SingleColor::initialize(const std::vector<RenderObject>* renderObjects)
{
    m_renderObjects = renderObjects;

    createRenderTarget();
    createShaders();
    createRootSignature();
    createSingleColorPSO();
    return true;
}

void SingleColor::render(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList, ID3D12DescriptorHeap* constantBufferHeap)
{
    commandList->SetPipelineState(m_PSO.Get());
    int currentFrameIndex = fw::API::getCurrentFrameIndex();

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_singleColorTextures[currentFrameIndex].Get(), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), currentFrameIndex, fw::API::getRtvDescriptorIncrementSize());

    const static float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    commandList->OMSetRenderTargets(1, &rtvHandle, true, nullptr);

    std::vector<ID3D12DescriptorHeap*> descriptorHeaps{constantBufferHeap};
    commandList->SetDescriptorHeaps((UINT)descriptorHeaps.size(), descriptorHeaps.data());

    commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    CD3DX12_GPU_DESCRIPTOR_HANDLE constantBufferHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(constantBufferHeap->GetGPUDescriptorHandleForHeapStart());
    constantBufferHandle.Offset(currentFrameIndex, fw::API::getCbvSrvUavDescriptorIncrementSize());
    commandList->SetGraphicsRootDescriptorTable(0, constantBufferHandle);

    commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (const RenderObject& ro : *m_renderObjects)
    {
        commandList->IASetVertexBuffers(0, 1, &ro.vertexBufferView);
        commandList->IASetIndexBuffer(&ro.indexBufferView);
        commandList->DrawIndexedInstanced(ro.numIndices, 1, 0, 0, 0);
    }

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_singleColorTextures[currentFrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));
}

void SingleColor::createRenderTarget()
{
    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();
    int swapChainSize = fw::API::getSwapChainBufferCount();

    // Create texture resources
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Alignment = 0;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Width = (UINT)fw::API::getWindowWidth();
    resourceDesc.Height = (UINT)fw::API::getWindowHeight();
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    resourceDesc.Format = fw::API::getBackBufferFormat();

    float clearColor[4] = {0.0, 0.0f, 0.0f, 1.0f};

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Color[0] = clearColor[0];
    clearValue.Color[1] = clearColor[1];
    clearValue.Color[2] = clearColor[2];
    clearValue.Color[3] = clearColor[3];
    clearValue.Format = fw::API::getBackBufferFormat();

    CD3DX12_HEAP_PROPERTIES heapProperty(D3D12_HEAP_TYPE_DEFAULT);
    m_singleColorTextures.resize(swapChainSize);

    for (int i = 0; i < swapChainSize; ++i)
    {
        CHECK(d3dDevice->CreateCommittedResource(&heapProperty,
                                                 D3D12_HEAP_FLAG_NONE,
                                                 &resourceDesc,
                                                 D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                                 &clearValue,
                                                 IID_PPV_ARGS(m_singleColorTextures[i].GetAddressOf())));
    }

    // Create RTV heap and render target views
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
    renderTargetViewDesc.Format = fw::API::getBackBufferFormat();

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < swapChainSize; ++i)
    {
        d3dDevice->CreateRenderTargetView(m_singleColorTextures[i].Get(), &renderTargetViewDesc, rtvHandle);
        rtvHandle.Offset(1, fw::API::getRtvDescriptorIncrementSize());
    }

    // Create SRV heap and shared resource views
    D3D12_DESCRIPTOR_HEAP_DESC srvDescriptorHeapDesc;
    srvDescriptorHeapDesc.NumDescriptors = swapChainSize;
    srvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvDescriptorHeapDesc.NodeMask = 0;

    CHECK(d3dDevice->CreateDescriptorHeap(&srvDescriptorHeapDesc, IID_PPV_ARGS(&m_srvHeap)));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Texture2D.MipLevels = resourceDesc.MipLevels;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Format = resourceDesc.Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = fw::API::getBackBufferFormat();

    CD3DX12_CPU_DESCRIPTOR_HANDLE srvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_srvHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < swapChainSize; ++i)
    {
        d3dDevice->CreateShaderResourceView(m_singleColorTextures[i].Get(), &srvDesc, srvHandle);
        srvHandle.Offset(1, fw::API::getCbvSrvUavDescriptorIncrementSize());
    }
}

void SingleColor::createShaders()
{
    std::wstring shaderPath = fw::stringToWstring(std::string(SHADER_PATH));

    std::wstring singleColorShaderPath = shaderPath;
    singleColorShaderPath += L"singleColor.hlsl";
    m_vertexShader = fw::compileShader(singleColorShaderPath, nullptr, "VS", "vs_5_0");
    m_pixelShader = fw::compileShader(singleColorShaderPath, nullptr, "PS", "ps_5_0");
}

void SingleColor::createRootSignature()
{
    std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(1);

    std::vector<CD3DX12_DESCRIPTOR_RANGE> cbvDescriptorRanges(1);
    cbvDescriptorRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    rootParameters[0].InitAsDescriptorTable(fw::uintSize(cbvDescriptorRanges), cbvDescriptorRanges.data());

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

void SingleColor::createSingleColorPSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = {c_vertexInputLayout.data(), (UINT)c_vertexInputLayout.size()};
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
