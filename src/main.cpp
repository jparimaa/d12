#include "header.h"

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <windows.h>

#include <iostream>

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

int main()
{
    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory;
    DX_CHECK(CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory)));

    Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice;
    HRESULT hardwareResult = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3dDevice));

    SUCCEEDED(hardwareResult);
    return hardwareResult;
}