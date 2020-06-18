#pragma once

#include <windows.h>

#include <iostream>

// clang-format off
#define CHECK(f)                                                                                        \
{                                                                                                       \
    HRESULT res = (f);                                                                                  \
    if (FAILED(res))                                                                                    \
    {                                                                                                   \
        std::cerr << "Error : HRESULT is \"" << res << "\" in " << __FILE__ << ":" << __LINE__ << "\n"  \
                    << #f << "\n";                                                                      \
        abort();                                                                                        \
    }                                                                                                   \
}
// clang-format on

#define ROUND_UP(v, powerOf2Alignment) (((v) + (powerOf2Alignment)-1) & ~((powerOf2Alignment)-1))
