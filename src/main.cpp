#include "header.h"

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>
#include "d3dx12.h"
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <d3dcompiler.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <iostream>
#include <cassert>
#include <vector>

#define CHECK(f)                                                                                           \
    {                                                                                                      \
        HRESULT res = (f);                                                                                 \
        if (FAILED(res))                                                                                   \
        {                                                                                                  \
            std::cerr << "Error : HRESULT is \"" << res << "\" in " << __FILE__ << ":" << __LINE__ << "\n" \
                      << #f << "\n";                                                                       \
            abort();                                                                                       \
        }                                                                                                  \
    }

const DXGI_FORMAT c_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
const DXGI_FORMAT c_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
const int c_swapChainBufferCount = 2;
const int c_windowWidth = 1200;
const int c_windowHeight = 900;
const float c_aspectRatio = static_cast<float>(c_windowWidth) / static_cast<float>(c_windowHeight);
bool running = true;

struct Vertex
{
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 color;
};

uint32_t roundUpByteSize(uint32_t byteSize)
{
    return (byteSize + 255) & ~255;
}

Microsoft::WRL::ComPtr<ID3DBlob> compileShader(const std::wstring& filename,
                                               const D3D_SHADER_MACRO* defines,
                                               const std::string& entrypoint,
                                               const std::string& target)
{
    uint32_t compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> byteCode = nullptr;
    Microsoft::WRL::ComPtr<ID3DBlob> errors;
    CHECK(D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors));

    if (errors != nullptr)
    {
        OutputDebugStringA((char*)errors->GetBufferPointer());
    }

    return byteCode;
}

Microsoft::WRL::ComPtr<ID3D12Resource> createGPUBuffer(ID3D12Device* device,
                                                       ID3D12GraphicsCommandList* cmdList,
                                                       const void* initData,
                                                       UINT64 byteSize,
                                                       Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer)
{
    Microsoft::WRL::ComPtr<ID3D12Resource> gpuBuffer;

    CHECK(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                          D3D12_HEAP_FLAG_NONE,
                                          &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
                                          D3D12_RESOURCE_STATE_COMMON,
                                          nullptr,
                                          IID_PPV_ARGS(gpuBuffer.GetAddressOf())));

    CHECK(device->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                          D3D12_HEAP_FLAG_NONE,
                                          &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
                                          D3D12_RESOURCE_STATE_GENERIC_READ,
                                          nullptr,
                                          IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    D3D12_SUBRESOURCE_DATA subResourceData{};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(gpuBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));
    UpdateSubresources<1>(cmdList, gpuBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(gpuBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));

    // Need to be kept alive until the data has been copied to GPU.
    return gpuBuffer;
}

int main()
{
    // Create window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(c_windowWidth, c_windowHeight, "DX12", nullptr, nullptr);
    glfwSetWindowPos(window, 1200, 200);

    auto keyCallback = [](GLFWwindow* win, int key, int scancode, int action, int mods) {
        if (key == GLFW_KEY_ESCAPE)
        {
            running = false;
        }
    };
    glfwSetKeyCallback(window, keyCallback);

    // Enable GBV
    Microsoft::WRL::ComPtr<ID3D12Debug> spDebugController0;
    Microsoft::WRL::ComPtr<ID3D12Debug1> spDebugController1;
    CHECK(D3D12GetDebugInterface(IID_PPV_ARGS(&spDebugController0)));
    CHECK(spDebugController0->QueryInterface(IID_PPV_ARGS(&spDebugController1)));
    spDebugController1->SetEnableGPUBasedValidation(true);

    // Enable debug layer
    Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
    CHECK(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
    debugController->EnableDebugLayer();

    // Create dxgi factory and d3d device
    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory;
    CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));

    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice;
    CHECK(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3dDevice)));

    // Check feature support
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
    msQualityLevels.Format = c_backBufferFormat;
    msQualityLevels.SampleCount = 4;
    msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msQualityLevels.NumQualityLevels = 0;
    CHECK(d3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels)));
    assert(msQualityLevels.NumQualityLevels > 0 && "Unexpected MSAA quality level");

    // Create fence
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

    // Get handle increment sizes
    UINT rtvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    UINT dsvDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    UINT cbvSrvUavDescriptorSize = d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Create command list
    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
    CHECK(d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> directCmdListAlloc;
    CHECK(d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(directCmdListAlloc.GetAddressOf())));

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    CHECK(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, directCmdListAlloc.Get(), nullptr, IID_PPV_ARGS(commandList.GetAddressOf())));

    // Create descriptor heaps
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = c_swapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    CHECK(d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;
    CHECK(d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHeap.GetAddressOf())));

    // Create swap chain
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    swapChain.Reset();

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    swapChainDesc.BufferDesc.Width = c_windowWidth;
    swapChainDesc.BufferDesc.Height = c_windowHeight;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.Format = c_backBufferFormat;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = c_swapChainBufferCount;
    swapChainDesc.OutputWindow = glfwGetWin32Window(window);
    swapChainDesc.Windowed = true;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    CHECK(dxgiFactory->CreateSwapChain(commandQueue.Get(), &swapChainDesc, swapChain.GetAddressOf()));

    // Create render targets
    Microsoft::WRL::ComPtr<ID3D12Resource> swapChainBuffer[c_swapChainBufferCount];

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < c_swapChainBufferCount; ++i)
    {
        CHECK(swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainBuffer[i])));
        d3dDevice->CreateRenderTargetView(swapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
        rtvHeapHandle.Offset(1, rtvDescriptorSize);
    }

    // Create the depth/stencil buffer and view.
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = c_windowWidth;
    depthStencilDesc.Height = c_windowHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = c_depthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilBuffer;
    CHECK(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                             D3D12_HEAP_FLAG_NONE,
                                             &depthStencilDesc,
                                             D3D12_RESOURCE_STATE_COMMON,
                                             &optClear,
                                             IID_PPV_ARGS(depthStencilBuffer.GetAddressOf())));

    // Create descriptor to mip level 0 of entire resource using the format of the resource.
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = c_depthStencilFormat;
    dsvDesc.Texture2D.MipSlice = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    d3dDevice->CreateDepthStencilView(depthStencilBuffer.Get(), &dsvDesc, depthStencilView);

    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    // Setup viewprt and scissor
    D3D12_VIEWPORT screenViewport{};
    screenViewport.TopLeftX = 0;
    screenViewport.TopLeftY = 0;
    screenViewport.Width = static_cast<float>(c_windowWidth);
    screenViewport.Height = static_cast<float>(c_windowHeight);
    screenViewport.MinDepth = 0.0f;
    screenViewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect{0, 0, c_windowWidth, c_windowHeight};

    // Create descriptor heap
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> cbvHeap = nullptr;
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask = 0;
    CHECK(d3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&cbvHeap)));

    // Create constant buffer
    Microsoft::WRL::ComPtr<ID3D12Resource> constantBuffer;
    uint32_t constantBufferSize = roundUpByteSize(sizeof(DirectX::XMFLOAT4X4));
    CHECK(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                                             D3D12_HEAP_FLAG_NONE,
                                             &CD3DX12_RESOURCE_DESC::Buffer(constantBufferSize),
                                             D3D12_RESOURCE_STATE_GENERIC_READ,
                                             nullptr,
                                             IID_PPV_ARGS(&constantBuffer)));

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
    cbvDesc.BufferLocation = constantBuffer->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = constantBufferSize;

    d3dDevice->CreateConstantBufferView(&cbvDesc, cbvHeap->GetCPUDescriptorHandleForHeapStart());

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

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = nullptr;
    CHECK(d3dDevice->CreateRootSignature(0,
                                         serializedRootSig->GetBufferPointer(),
                                         serializedRootSig->GetBufferSize(),
                                         IID_PPV_ARGS(&rootSignature)));

    // Create shaders
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShader = compileShader(L"..\\shaders\\simple.hlsl", nullptr, "VS", "vs_5_0");
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShader = compileShader(L"..\\shaders\\simple.hlsl", nullptr, "PS", "ps_5_0");

    // Set input layout
    std::vector<D3D12_INPUT_ELEMENT_DESC> vertexInputLayout = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}};

    // Create vertex input buffers

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

    const size_t vbByteSize = vertices.size() * sizeof(Vertex);
    const size_t ibByteSize = indices.size() * sizeof(uint16_t);

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexUploadBuffer = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexUploadBuffer = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferGPU = createGPUBuffer(d3dDevice.Get(),
                                                                             commandList.Get(),
                                                                             vertices.data(),
                                                                             vbByteSize,
                                                                             vertexUploadBuffer);

    Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferGPU = createGPUBuffer(d3dDevice.Get(),
                                                                            commandList.Get(),
                                                                            indices.data(),
                                                                            ibByteSize,
                                                                            indexUploadBuffer);

    // Create views of buffers
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    vertexBufferView.BufferLocation = vertexBufferGPU->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = sizeof(Vertex);
    vertexBufferView.SizeInBytes = (UINT)vbByteSize;

    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    indexBufferView.BufferLocation = indexBufferGPU->GetGPUVirtualAddress();
    indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    indexBufferView.SizeInBytes = (UINT)ibByteSize;

    // Create PSO
    Microsoft::WRL::ComPtr<ID3D12PipelineState> PSO = nullptr;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.InputLayout = {vertexInputLayout.data(), (UINT)vertexInputLayout.size()};
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.VS = {reinterpret_cast<BYTE*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize()};
    psoDesc.PS = {reinterpret_cast<BYTE*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize()};
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = c_backBufferFormat;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;
    psoDesc.DSVFormat = c_depthStencilFormat;
    CHECK(d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PSO)));

    // Execute initialization commands
    CHECK(commandList->Close());
    std::vector<ID3D12CommandList*> cmdsLists{commandList.Get()};
    commandQueue->ExecuteCommandLists((UINT)cmdsLists.size(), cmdsLists.data());

    // Render loop
    int currentBackBufferIndex = 0;
    UINT64 currentFence = 0;

    while (!glfwWindowShouldClose(window) && running)
    {
        glfwPollEvents();

        // Update
        DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();

        DirectX::XMVECTOR pos = DirectX::XMVectorSet(0.0f, 3.0f, -10.0f, 1.0f);
        DirectX::XMVECTOR target = DirectX::XMVectorZero();
        DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        DirectX::XMMATRIX view = DirectX::XMMatrixLookAtLH(pos, target, up);

        DirectX::XMMATRIX proj = DirectX::XMMatrixPerspectiveFovLH(0.785f, c_aspectRatio, 0.1f, 100.0f);
        DirectX::XMMATRIX worldViewProj = world * view * proj;

        DirectX::XMMATRIX wvp = DirectX::XMMatrixTranspose(worldViewProj);

        char* mappedData = nullptr;
        CHECK(constantBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));
        memcpy(&mappedData[0], &wvp, sizeof(DirectX::XMMATRIX));
        constantBuffer->Unmap(0, nullptr);

        // Rendering
        CHECK(directCmdListAlloc->Reset());
        CHECK(commandList->Reset(directCmdListAlloc.Get(), PSO.Get()));

        ID3D12Resource* currentBackBuffer = swapChainBuffer[currentBackBufferIndex].Get();
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        commandList->RSSetViewports(1, &screenViewport);
        commandList->RSSetScissorRects(1, &scissorRect);

        CD3DX12_CPU_DESCRIPTOR_HANDLE currentBackBufferView(rtvHeap->GetCPUDescriptorHandleForHeapStart(), currentBackBufferIndex, rtvDescriptorSize);
        const static float clearColor[4] = {0.2f, 0.4f, 0.6f, 1.0f};
        commandList->ClearRenderTargetView(currentBackBufferView, clearColor, 0, nullptr);

        commandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

        commandList->OMSetRenderTargets(1, &currentBackBufferView, true, &depthStencilView);

        // Draw commands
        std::vector<ID3D12DescriptorHeap*> descriptorHeaps{cbvHeap.Get()};
        commandList->SetDescriptorHeaps((UINT)descriptorHeaps.size(), descriptorHeaps.data());

        commandList->SetGraphicsRootSignature(rootSignature.Get());

        commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
        commandList->IASetIndexBuffer(&indexBufferView);
        commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        commandList->SetGraphicsRootDescriptorTable(0, cbvHeap->GetGPUDescriptorHandleForHeapStart());

        commandList->DrawIndexedInstanced((UINT)indices.size(), 1, 0, 0, 0);

        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        CHECK(commandList->Close());

        std::vector<ID3D12CommandList*> cmdLists{commandList.Get()};
        commandQueue->ExecuteCommandLists(static_cast<UINT>(cmdLists.size()), cmdLists.data());

        CHECK(swapChain->Present(0, 0));
        currentBackBufferIndex = (currentBackBufferIndex + 1) % c_swapChainBufferCount;

        // Wait
        currentFence++;
        CHECK(commandQueue->Signal(fence.Get(), currentFence));
        assert(fence->GetCompletedValue() != UINT64_MAX);
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        CHECK(fence->SetEventOnCompletion(currentFence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}