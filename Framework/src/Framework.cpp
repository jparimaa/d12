#include "Framework.h"
#include "Macros.h"

#include <cassert>

namespace fw
{
Framework::Framework()
{
    API::s_framework = this;
}

Framework::~Framework()
{
}

bool Framework::initialize()
{
    m_window.initialize();

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
    CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)));
    CHECK(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_d3dDevice)));

    // Check feature support
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
    msQualityLevels.Format = m_backBufferFormat;
    msQualityLevels.SampleCount = 4;
    msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msQualityLevels.NumQualityLevels = 0;
    CHECK(m_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels)));
    assert(msQualityLevels.NumQualityLevels > 0 && "Unexpected MSAA quality level");

    // Create fence
    m_fenceIds.resize(m_swapChainBufferCount);
    m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence));

    // Get handle increment sizes
    m_rtvDescriptorIncrementSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_dsvDescriptorIncrementSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_cbvSrvUavDescriptorIncrementSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Create command list
    CHECK(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(m_commandAllocator.GetAddressOf())));

    D3D12_COMMAND_QUEUE_DESC queueDesc{};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    CHECK(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
    CHECK(m_d3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(m_commandList.GetAddressOf())));

    m_frameCommandAllocators.resize(m_swapChainBufferCount);
    for (Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& frameCommandAllocator : m_frameCommandAllocators)
    {
        CHECK(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(frameCommandAllocator.GetAddressOf())));
    }

    // Create descriptor heaps
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = m_swapChainBufferCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    CHECK(m_d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    CHECK(m_d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf())));

    // Create swap chain
    m_swapChain.Reset();

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    swapChainDesc.BufferDesc.Width = m_window.getWidth();
    swapChainDesc.BufferDesc.Height = m_window.getHeight();
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.Format = m_backBufferFormat;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = m_swapChainBufferCount;
    swapChainDesc.OutputWindow = m_window.getNativeHandle();
    swapChainDesc.Windowed = true;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    m_swapChain.Reset();
    CHECK(m_dxgiFactory->CreateSwapChain(m_commandQueue.Get(), &swapChainDesc, m_swapChain.GetAddressOf()));

    // Create render targets
    m_swapChainBuffers.resize(m_swapChainBufferCount);
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (int i = 0; i < m_swapChainBufferCount; ++i)
    {
        CHECK(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_swapChainBuffers[i])));
        m_d3dDevice->CreateRenderTargetView(m_swapChainBuffers[i].Get(), nullptr, rtvHeapHandle);
        rtvHeapHandle.Offset(1, m_rtvDescriptorIncrementSize);
    }

    // Create the depth/stencil buffer
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment = 0;
    depthStencilDesc.Width = m_window.getWidth();
    depthStencilDesc.Height = m_window.getHeight();
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.SampleDesc.Quality = 0;
    depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = m_depthStencilFormat;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    CHECK(m_d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                               D3D12_HEAP_FLAG_NONE,
                                               &depthStencilDesc,
                                               D3D12_RESOURCE_STATE_COMMON,
                                               &optClear,
                                               IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

    // Create depth stencil view
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format = m_depthStencilFormat;
    dsvDesc.Texture2D.MipSlice = 0;
    m_depthStencilView = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    m_d3dDevice->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, m_depthStencilView);

    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    m_timeFromBegin = std::chrono::steady_clock::now();
    m_timeLastUpdate = std::chrono::steady_clock::now();

    return true;
}

void Framework::setApplication(Application* application)
{
    m_app = application;
}

void Framework::execute()
{
    while (m_running && !m_window.shouldClose())
    {
        m_window.update();
        long long delta = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - m_timeLastUpdate).count();
        m_timeDelta = static_cast<float>(delta) / 1000000.0f;
        m_timeLastUpdate = std::chrono::steady_clock::now();
        waitForFrame(m_currentFrameIndex);
        m_app->update();
        m_app->fillCommandList();
        render();
        m_window.clearKeyStatus();
    }

    for (int i = 0; i < m_swapChainBufferCount; ++i)
    {
        waitForFrame(i);
    }
}

void Framework::completeInitialization()
{
    std::vector<ID3D12CommandList*> cmdsLists{m_commandList.Get()};
    m_commandQueue->ExecuteCommandLists((UINT)cmdsLists.size(), cmdsLists.data());
    CHECK(m_commandQueue->Signal(m_fence.Get(), 1));
    HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
    CHECK(m_fence->SetEventOnCompletion(1, eventHandle));
    WaitForSingleObject(eventHandle, INFINITE);
    CloseHandle(eventHandle);
}

void Framework::waitForFrame(int frameIndex)
{
    HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
    CHECK(m_fence->SetEventOnCompletion(m_fenceIds[frameIndex], eventHandle));
    WaitForSingleObject(eventHandle, INFINITE);
    CloseHandle(eventHandle);
}

void Framework::render()
{
    std::vector<ID3D12CommandList*> cmdLists{m_commandList.Get()};
    m_commandQueue->ExecuteCommandLists(static_cast<UINT>(cmdLists.size()), cmdLists.data());

    CHECK(m_swapChain->Present(0, 0));

    m_fenceIds[m_currentFrameIndex] = m_currentFenceId;
    CHECK(m_commandQueue->Signal(m_fence.Get(), m_currentFenceId));

    m_currentFrameIndex = (m_currentFrameIndex + 1) % m_swapChainBufferCount;
    ++m_currentFenceId;
}

ID3D12Resource* Framework::getCurrentBackBuffer()
{
    return m_swapChainBuffers[m_currentFrameIndex].Get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE Framework::getCurrentBackBufferView()
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_currentFrameIndex, m_rtvDescriptorIncrementSize);
}

} // namespace fw
