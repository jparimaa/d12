#pragma once

#include "API.h"
#include "Application.h"
#include "Window.h"

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>
#include "d3dx12.h"

#include <vector>
#include <chrono>

namespace fw
{
class Framework
{
    friend class API;

public:
    Framework();
    ~Framework();
    Framework(const Framework&) = delete;
    Framework(Framework&&) = delete;
    Framework& operator=(const Framework&) = delete;
    Framework& operator=(Framework&&) = delete;

    bool initialize();
    void setApplication(Application* application);
    void execute();

private:
    DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT m_depthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    int m_swapChainBufferCount = 2;

    Window m_window;
    Application* m_app = nullptr;

    Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;

    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_currentFenceId = 100;
    std::vector<UINT64> m_fenceIds;

    UINT m_rtvDescriptorIncrementSize;
    UINT m_dsvDescriptorIncrementSize;
    UINT m_cbvSrvUavDescriptorIncrementSize;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    std::vector<Microsoft::WRL::ComPtr<ID3D12CommandAllocator>> m_frameCommandAllocators;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_swapChainBuffers;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;
    D3D12_CPU_DESCRIPTOR_HANDLE m_depthStencilView;

    int m_currentFrameIndex = 0;

    std::chrono::steady_clock::time_point m_timeFromBegin;
    std::chrono::steady_clock::time_point m_timeLastUpdate;
    float m_timeDelta = 0.0f;

    bool m_running = true;

    void completeInitialization();
    void waitForFrame(int frameIndex);
    void render();
    ID3D12Resource* getCurrentBackBuffer();
    CD3DX12_CPU_DESCRIPTOR_HANDLE getCurrentBackBufferView();
};

} // namespace fw
