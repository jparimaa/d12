#pragma once

#include "API.h"
#include "Application.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>
#include "d3dx12.h"

#include <vector>

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
    int m_windowWidth = 1200;
    int m_windowHeight = 900;
    float m_aspectRatio = static_cast<float>(m_windowWidth) / static_cast<float>(m_windowHeight);

    Application* m_app = nullptr;
    GLFWwindow* m_window = nullptr;

    Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;

    UINT m_rtvDescriptorSize;
    UINT m_dsvDescriptorSize;
    UINT m_cbvSrvUavDescriptorSize;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    Microsoft::WRL::ComPtr<IDXGISwapChain> m_swapChain;
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_swapChainBuffers;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthStencilBuffer;
    D3D12_CPU_DESCRIPTOR_HANDLE m_depthStencilView;

    int m_currentBackBufferIndex = 0;
    UINT64 m_currentFence = 0;

    bool m_running = true;

    void render();
    ID3D12Resource* getCurrentBackBuffer();
    CD3DX12_CPU_DESCRIPTOR_HANDLE getCurrentBackBufferView();
};

} // namespace fw
