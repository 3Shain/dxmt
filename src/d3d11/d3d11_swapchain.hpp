#pragma once

#include "d3d11_device.hpp"
#include "mtld11_resource.hpp"
#include "../dxgi/dxgi_object.h"
#include "d3d11_view.hpp"

namespace dxmt {


HRESULT
CreateSwapChain(IDXGIFactory1 *pFactory, IMTLDXGIDevice *pDevice, HWND hWnd,
                IDXGIMetalLayerFactory *pMetalLayerFactory,
                const DXGI_SWAP_CHAIN_DESC1 *pDesc,
                const DXGI_SWAP_CHAIN_FULLSCREEN_DESC *pFullscreenDesc,
                IDXGISwapChain1 **ppSwapChain);

} // namespace dxmt