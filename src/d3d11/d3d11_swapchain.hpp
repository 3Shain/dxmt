#pragma once

#include "dxgi1_2.h"
#include "../dxgi/dxgi_interfaces.h"

namespace dxmt {


HRESULT
CreateSwapChain(IDXGIFactory1 *pFactory, IMTLDXGIDevice *pDevice, HWND hWnd,
                IDXGIMetalLayerFactory *pMetalLayerFactory,
                const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
                IDXGISwapChain1 **ppSwapChain);

} // namespace dxmt