#include "header.h"

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>

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
const int c_swapChainBufferCount = 2;

int main()
{
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
    /*
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
    swapChain.Reset();

    DXGI_SWAP_CHAIN_DESC swapChainDesc;
    swapChainDesc.BufferDesc.Width = 1200;
    swapChainDesc.BufferDesc.Height = 900;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferDesc.Format = c_backBufferFormat;
    swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = c_swapChainBufferCount;
    swapChainDesc.OutputWindow = mhMainWnd;
    swapChainDesc.Windowed = true;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    DX_CHECK(dxgiFactory->CreateSwapChain(commandQueue.Get(), &swapChainDesc, swapChain.GetAddressOf()));
	*/

    return 0;
}