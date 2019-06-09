#pragma once

#include "Framework.h"

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include "d3dx12.h"

namespace fw
{
class API
{
public:
    friend class Framework;

    API() = delete;

    static void quit();

    static int getWindowWidth();
    static int getWindowHeight();
    static float getAspectRatio();

    static Microsoft::WRL::ComPtr<ID3D12Device> getD3dDevice();
    static Microsoft::WRL::ComPtr<ID3D12CommandQueue> getCommandQueue();
    static Microsoft::WRL::ComPtr<ID3D12CommandAllocator> getCommandAllocator();
    static Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> getCommandList();

    static DXGI_FORMAT getBackBufferFormat();
    static DXGI_FORMAT getDepthStencilFormat();

    static ID3D12Resource* getCurrentBackBuffer();
    static CD3DX12_CPU_DESCRIPTOR_HANDLE getCurrentBackBufferView();
    static D3D12_CPU_DESCRIPTOR_HANDLE getDepthStencilView();

    static bool isKeyPressed(int key);
    static bool isKeyDown(int key);
    static bool isKeyReleased(int key);
    static float getMouseDeltaX();
    static float getMouseDeltaY();

    static float getTimeDelta();

private:
    static Framework* s_framework;
};

} // namespace fw
