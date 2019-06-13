#include "API.h"

namespace fw
{
Framework* API::s_framework = nullptr;

void API::quit()
{
    s_framework->m_running = false;
}

int API::getWindowWidth()
{
    return s_framework->m_window.getWidth();
}

int API::getWindowHeight()
{
    return s_framework->m_window.getHeight();
}

float API::getAspectRatio()
{
    return static_cast<float>(getWindowWidth()) / static_cast<float>(getWindowHeight());
}

Microsoft::WRL::ComPtr<ID3D12Device> API::getD3dDevice()
{
    return s_framework->m_d3dDevice;
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> API::getCommandList()
{
    return s_framework->m_commandList;
}

int API::getCurrentFrameIndex()
{
    return s_framework->m_currentFrameIndex;
}

int API::getSwapChainBufferCount()
{
    return s_framework->m_swapChainBufferCount;
}

UINT API::getCbvSrvUavDescriptorIncrementSize()
{
    return s_framework->m_cbvSrvUavDescriptorIncrementSize;
}

Microsoft::WRL::ComPtr<ID3D12CommandQueue> API::getCommandQueue()
{
    return s_framework->m_commandQueue;
}

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> API::getCommandAllocator()
{
    return s_framework->m_commandAllocator;
}

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> API::getCurrentFrameCommandAllocator()
{
    return s_framework->m_frameCommandAllocators[s_framework->m_currentFrameIndex];
}

DXGI_FORMAT API::getBackBufferFormat()
{
    return s_framework->m_backBufferFormat;
}

DXGI_FORMAT API::getDepthStencilFormat()
{
    return s_framework->m_depthStencilFormat;
}

ID3D12Resource* API::getCurrentBackBuffer()
{
    return s_framework->getCurrentBackBuffer();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE API::getCurrentBackBufferView()
{
    return s_framework->getCurrentBackBufferView();
}

D3D12_CPU_DESCRIPTOR_HANDLE API::getDepthStencilView()
{
    return s_framework->m_depthStencilView;
}

bool API::isKeyPressed(int key)
{
    return s_framework->m_window.isKeyPressed(key);
}

bool API::isKeyDown(int key)
{
    return s_framework->m_window.isKeyDown(key);
}

bool API::isKeyReleased(int key)
{
    return s_framework->m_window.isKeyReleased(key);
}

float API::getMouseDeltaX()
{
    return s_framework->m_window.getDeltaX();
}

float API::getMouseDeltaY()
{
    return s_framework->m_window.getDeltaY();
}

float API::getTimeDelta()
{
    return s_framework->m_timeDelta;
}

} // namespace fw
