#include "MinimalApp.h"

#include <fw/Framework.h>
#include <fw/Common.h>
#include <fw/API.h>
#include <fw/Macros.h>
#include <fw/Transformation.h>

#include "d3dx12.h"
#include <DirectXMath.h>
#include <DirectXColors.h>

#include <GLFW/glfw3.h>

#include <vector>
#include <string>

namespace
{
struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
};

std::vector<Vertex> vertices = {
    Vertex({DirectX::XMFLOAT3(-1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::White)}),
    Vertex({DirectX::XMFLOAT3(-1.0f, +1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Black)}),
    Vertex({DirectX::XMFLOAT3(+1.0f, +1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Red)}),
    Vertex({DirectX::XMFLOAT3(+1.0f, -1.0f, -1.0f), DirectX::XMFLOAT4(DirectX::Colors::Green)}),
    Vertex({DirectX::XMFLOAT3(-1.0f, -1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Blue)}),
    Vertex({DirectX::XMFLOAT3(-1.0f, +1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Yellow)}),
    Vertex({DirectX::XMFLOAT3(+1.0f, +1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Cyan)}),
    Vertex({DirectX::XMFLOAT3(+1.0f, -1.0f, +1.0f), DirectX::XMFLOAT4(DirectX::Colors::Magenta)})};

std::vector<uint16_t> indices = {0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 4, 5, 1, 4, 1, 0, 3, 2, 6, 3, 6, 7, 1, 5, 6, 1, 6, 2, 4, 0, 3, 4, 3, 7};

bool running = true;
} // namespace

MinimalApp::~MinimalApp()
{
}

bool MinimalApp::initialize()
{
    // Setup viewprt and scissor
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

    // Create descriptor heap
    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice = fw::API::getD3dDevice();
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = fw::API::getSwapChainBufferCount();
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    CHECK(d3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));

    // Create constant buffer
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

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = constantBufferSize;

        CD3DX12_CPU_DESCRIPTOR_HANDLE handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(i, fw::API::getCbvSrvUavDescriptorIncrementSize());

        d3dDevice->CreateConstantBufferView(&cbvDesc, handle);
    }

    // Create root signature
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    CD3DX12_DESCRIPTOR_RANGE cbvTable;
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(1, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> serializedRootSig = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;
    CHECK(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));

    if (errorBlob != nullptr)
    {
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }

    CHECK(d3dDevice->CreateRootSignature(0,
                                         serializedRootSig->GetBufferPointer(),
                                         serializedRootSig->GetBufferSize(),
                                         IID_PPV_ARGS(&m_rootSignature)));

    // Create shaders
    std::wstring shaderFile = fw::stringToWstring(std::string(SHADER_PATH));
    shaderFile += L"simple.hlsl";
    m_vertexShader = fw::compileShader(shaderFile, nullptr, "VS", "vs_5_0");
    m_pixelShader = fw::compileShader(shaderFile, nullptr, "PS", "ps_5_0");

    // Set input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> vertexInputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    // Create vertex input buffers
    const size_t vbByteSize = vertices.size() * sizeof(Vertex);
    const size_t ibByteSize = indices.size() * sizeof(uint16_t);

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexUploadBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexUploadBuffer = nullptr;

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = fw::API::getCommandList();
    m_vertexBufferGPU = fw::createGPUBuffer(d3dDevice.Get(),
                                            commandList.Get(),
                                            vertices.data(),
                                            vbByteSize,
                                            vertexUploadBuffer);

    m_indexBufferGPU = fw::createGPUBuffer(d3dDevice.Get(),
                                           commandList.Get(),
                                           indices.data(),
                                           ibByteSize,
                                           indexUploadBuffer);

    // Create views of buffers
    m_vertexBufferView.BufferLocation = m_vertexBufferGPU->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = (UINT)vbByteSize;

    m_indexBufferView.BufferLocation = m_indexBufferGPU->GetGPUVirtualAddress();
    m_indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_indexBufferView.SizeInBytes = (UINT)ibByteSize;

    // Create PSO
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
    CHECK(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PSO)));

    // Execute and wait initialization commands
    CHECK(commandList->Close());
    fw::API::completeInitialization();

    // Camera
    m_cameraController.setCamera(&m_camera);
    m_camera.getTransformation().setPosition(0.0f, 1.0f, -10.0f);

    return true;
}

void MinimalApp::update()
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

    // Rendering
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = fw::API::getCurrentFrameCommandAllocator();
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = fw::API::getCommandList();

    CHECK(commandAllocator->Reset());
    CHECK(commandList->Reset(commandAllocator.Get(), m_PSO.Get()));

    ID3D12Resource* currentBackBuffer = fw::API::getCurrentBackBuffer();
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    commandList->RSSetViewports(1, &m_screenViewport);
    commandList->RSSetScissorRects(1, &m_scissorRect);

    CD3DX12_CPU_DESCRIPTOR_HANDLE currentBackBufferView = fw::API::getCurrentBackBufferView();
    const static float clearColor[4] = {0.2f, 0.4f, 0.6f, 1.0f};
    commandList->ClearRenderTargetView(currentBackBufferView, clearColor, 0, nullptr);

    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = fw::API::getDepthStencilView();
    commandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    commandList->OMSetRenderTargets(1, &currentBackBufferView, true, &depthStencilView);

    // Draw commands
    std::vector<ID3D12DescriptorHeap*> descriptorHeaps{m_cbvHeap.Get()};
    commandList->SetDescriptorHeaps((UINT)descriptorHeaps.size(), descriptorHeaps.data());

    commandList->SetGraphicsRootSignature(m_rootSignature.Get());

    commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
    commandList->IASetIndexBuffer(&m_indexBufferView);
    commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    CD3DX12_GPU_DESCRIPTOR_HANDLE handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetGPUDescriptorHandleForHeapStart());
    handle.Offset(currentFrameIndex, fw::API::getCbvSrvUavDescriptorIncrementSize());
    commandList->SetGraphicsRootDescriptorTable(0, handle);

    commandList->DrawIndexedInstanced((UINT)indices.size(), 1, 0, 0, 0);

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    CHECK(commandList->Close());

    if (fw::API::isKeyReleased(GLFW_KEY_ESCAPE))
    {
        fw::API::quit();
    }
}

void MinimalApp::onGUI()
{
}
