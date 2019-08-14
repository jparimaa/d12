#include "MotionBlurApp.h"
#include "Shared.h"

#include <fw/Framework.h>
#include <fw/Common.h>
#include <fw/API.h>
#include <fw/Macros.h>
#include <fw/Transformation.h>

#include <DirectXMath.h>

#include <GLFW/glfw3.h>

#include <vector>
#include <string>

namespace
{
const int c_descriptorHeapSize = 2048;
const int c_albedoTextureOffset = 0;
const int c_renderTextureOffset = 512;
const int c_renderDepthOffset = 513;
const int c_motionVectorTextureOffset = 1024;
} // namespace

bool MotionBlurApp::initialize()
{
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = fw::API::getCommandList();

    createDescriptorHeap();
    createVertexBuffers(commandList);

    createShaders();
    createRootSignature();

    createRenderPSO();

    m_objectRender.initialize(commandList, m_descriptorHeap.Get(), c_albedoTextureOffset, c_renderTextureOffset, c_renderDepthOffset);
    m_motionVector.initialize(commandList, m_descriptorHeap.Get(), c_motionVectorTextureOffset);

    // Execute and wait initialization commands
    CHECK(commandList->Close());
    fw::API::completeInitialization();
    m_vertexUploadBuffer.Reset();
    m_indexUploadBuffer.Reset();
    m_objectRender.postInitialize();
    m_motionVector.postInitialize();

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

void MotionBlurApp::update()
{
    m_cameraController.update();
    m_camera.updateViewMatrix();

    m_objectRender.update(&m_camera);
    m_motionVector.update(&m_camera);

    if (fw::API::isKeyReleased(GLFW_KEY_ESCAPE))
    {
        fw::API::quit();
    }
}

void MotionBlurApp::fillCommandList()
{
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = fw::API::getCurrentFrameCommandAllocator();
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = fw::API::getCommandList();

    CHECK(commandAllocator->Reset());
    CHECK(commandList->Reset(commandAllocator.Get(), nullptr));

    commandList->RSSetViewports(1, &m_screenViewport);
    commandList->RSSetScissorRects(1, &m_scissorRect);

    m_objectRender.render(commandList);
    m_motionVector.render(commandList, m_descriptorHeap.Get(), c_renderDepthOffset);

    commandList->SetPipelineState(m_PSO.Get());

    ID3D12Resource* currentBackBuffer = fw::API::getCurrentBackBuffer();
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    CD3DX12_CPU_DESCRIPTOR_HANDLE currentBackBufferView = fw::API::getCurrentBackBufferView();
    const static float clearColor[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    commandList->ClearRenderTargetView(currentBackBufferView, clearColor, 0, nullptr);

    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = fw::API::getDepthStencilView();
    commandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->OMSetRenderTargets(1, &currentBackBufferView, true, &depthStencilView);

    commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    std::vector<ID3D12DescriptorHeap*> descriptorHeaps{m_descriptorHeap.Get()};
    commandList->SetDescriptorHeaps((UINT)descriptorHeaps.size(), descriptorHeaps.data());

    CD3DX12_GPU_DESCRIPTOR_HANDLE objectTextureHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    objectTextureHandle.Offset(c_renderTextureOffset, fw::API::getCbvSrvUavDescriptorIncrementSize());

    commandList->SetGraphicsRootDescriptorTable(0, objectTextureHandle);

    CD3DX12_GPU_DESCRIPTOR_HANDLE motionVectorTextureHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    motionVectorTextureHandle.Offset(c_motionVectorTextureOffset, fw::API::getCbvSrvUavDescriptorIncrementSize());

    commandList->SetGraphicsRootDescriptorTable(1, motionVectorTextureHandle);

    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->DrawIndexedInstanced(fw::uintSize(c_fullscreenTriangleIndices), 1, 0, 0, 0);

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    CHECK(commandList->Close());
}

void MotionBlurApp::onGUI()
{
}

void MotionBlurApp::createDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc;
    descriptorHeapDesc.NumDescriptors = c_descriptorHeapSize;
    descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    descriptorHeapDesc.NodeMask = 0;

    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();
    CHECK(d3dDevice->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&m_descriptorHeap)));
}

void MotionBlurApp::createVertexBuffers(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& commandList)
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

void MotionBlurApp::createShaders()
{
    std::wstring shaderFile = fw::stringToWstring(std::string(SHADER_PATH));
    shaderFile += L"motionBlur.hlsl";
    m_vertexShader = fw::compileShader(shaderFile, nullptr, "VS", "vs_5_0");
    m_pixelShader = fw::compileShader(shaderFile, nullptr, "PS", "ps_5_0");
}

void MotionBlurApp::createRootSignature()
{
    std::vector<CD3DX12_ROOT_PARAMETER> rootParameters(2);

    CD3DX12_DESCRIPTOR_RANGE objectRenderTextureDescriptorRange;
    objectRenderTextureDescriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    rootParameters[0].InitAsDescriptorTable(1, &objectRenderTextureDescriptorRange);

    CD3DX12_DESCRIPTOR_RANGE motionVectorTextureDescriptorRange;
    motionVectorTextureDescriptorRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
    rootParameters[1].InitAsDescriptorTable(1, &motionVectorTextureDescriptorRange);

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

void MotionBlurApp::createRenderPSO()
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
