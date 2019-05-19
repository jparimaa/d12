#include "header.h"

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>
#include "d3dx12.h"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <iostream>
#include <cassert>

#define DX_CHECK(f)                                                                                        \
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

int main()
{
    // Create window
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(c_windowWidth, c_windowHeight, "DX12", nullptr, nullptr);
    glfwSetWindowPos(window, 1200, 200);

    // Create dxgi factory and d3d device
    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory;
    DX_CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));

    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice;
    DX_CHECK(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3dDevice)));

    // Check feature support
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
    msQualityLevels.Format = c_backBufferFormat;
    msQualityLevels.SampleCount = 4;
    msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msQualityLevels.NumQualityLevels = 0;
    DX_CHECK(d3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels)));
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
    DX_CHECK(d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)));

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> directCmdListAlloc;
    DX_CHECK(d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(directCmdListAlloc.GetAddressOf())));

    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;
    DX_CHECK(d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, directCmdListAlloc.Get(), nullptr, IID_PPV_ARGS(commandList.GetAddressOf())));
    commandList->Close();

    // Create descriptor heaps
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = c_swapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
    DX_CHECK(d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;
    DX_CHECK(d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(dsvHeap.GetAddressOf())));

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

    DX_CHECK(dxgiFactory->CreateSwapChain(commandQueue.Get(), &swapChainDesc, swapChain.GetAddressOf()));

    // Create render targets
    Microsoft::WRL::ComPtr<ID3D12Resource> swapChainBuffer[c_swapChainBufferCount];

    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < c_swapChainBufferCount; ++i)
    {
        DX_CHECK(swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainBuffer[i])));
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
    DX_CHECK(d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                D3D12_HEAP_FLAG_NONE,
                                                &depthStencilDesc,
                                                D3D12_RESOURCE_STATE_COMMON,
                                                &optClear,
                                                IID_PPV_ARGS(depthStencilBuffer.GetAddressOf())));

    // Setup viewprt and scissor
    D3D12_VIEWPORT screenViewport{};
    screenViewport.TopLeftX = 0;
    screenViewport.TopLeftY = 0;
    screenViewport.Width = static_cast<float>(c_windowWidth);
    screenViewport.Height = static_cast<float>(c_windowHeight);
    screenViewport.MinDepth = 0.0f;
    screenViewport.MaxDepth = 1.0f;

    D3D12_RECT scissorRect{0, 0, c_windowWidth, c_windowHeight};

    // Render loop
    int currentBackBufferIndex = 0;
    UINT64 currentFence = 0;

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        DX_CHECK(directCmdListAlloc->Reset());
        DX_CHECK(commandList->Reset(directCmdListAlloc.Get(), nullptr));

        ID3D12Resource* currentBackBuffer = swapChainBuffer[currentBackBufferIndex].Get();
        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        commandList->RSSetViewports(1, &screenViewport);
        commandList->RSSetScissorRects(1, &scissorRect);

        CD3DX12_CPU_DESCRIPTOR_HANDLE currentBackBufferView(rtvHeap->GetCPUDescriptorHandleForHeapStart(), currentBackBufferIndex, rtvDescriptorSize);
        const static float clearColor[4] = {0.2f, 0.4f, 0.6f, 1.0f};
        commandList->ClearRenderTargetView(currentBackBufferView, clearColor, 0, nullptr);

        D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = dsvHeap->GetCPUDescriptorHandleForHeapStart();
        commandList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

        commandList->OMSetRenderTargets(1, &currentBackBufferView, true, &depthStencilView);

        commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(currentBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        DX_CHECK(commandList->Close());

        std::vector<ID3D12CommandList*> cmdLists{commandList.Get()};
        commandQueue->ExecuteCommandLists(static_cast<UINT>(cmdLists.size()), cmdLists.data());

        DX_CHECK(swapChain->Present(0, 0));
        currentBackBufferIndex = (currentBackBufferIndex + 1) % c_swapChainBufferCount;

        // Wait
        currentFence++;
        DX_CHECK(commandQueue->Signal(fence.Get(), currentFence));
        assert(fence->GetCompletedValue() != UINT64_MAX);
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        DX_CHECK(fence->SetEventOnCompletion(currentFence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    return 0;
}