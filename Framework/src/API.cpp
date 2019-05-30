#include "API.h"

namespace fw
{
Framework* API::s_framework = nullptr;

int API::getWindowWidth()
{
    return s_framework->m_windowWidth;
}

int API::getWindowHeight()
{
    return s_framework->m_windowHeight;
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

Microsoft::WRL::ComPtr<ID3D12CommandQueue> API::getCommandQueue()
{
    return s_framework->m_commandQueue;
}

Microsoft::WRL::ComPtr<ID3D12CommandAllocator> API::getCommandAllocator()
{
    return s_framework->m_commandAllocator;
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
} // namespace fw
