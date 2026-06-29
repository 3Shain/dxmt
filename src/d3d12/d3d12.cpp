/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "d3d12.h"
#include "com/com_pointer.hpp"
#include "d3d12_device.hpp"
#include "dxgi_interfaces.h"
#include "log/log.hpp"

namespace dxmt {

Logger Logger::s_instance("d3d12.log");

extern "C" HRESULT WINAPI
D3D12CreateDevice(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel, REFIID riid, void **ppDevice) {

  Com<IDXGIAdapter> dxgi_adapter = nullptr;
  Com<IDXGIFactory> dxgi_factory = nullptr;
  Com<IMTLDXGIAdapter> dxgi_adapter_mtl = nullptr;

  if (MinimumFeatureLevel < D3D_FEATURE_LEVEL_11_0)
    return E_INVALIDARG;

  HRESULT hr;

  if (!pAdapter) {
    hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgi_factory));

    if (FAILED(hr)) {
      ERR("D3D12CreateDevice: Failed to create a DXGI factory");
      return hr;
    }

    if (FAILED(hr = dxgi_factory->EnumAdapters(0, &dxgi_adapter))) {
      ERR("D3D12CreateDevice: No default adapter available");
      return hr;
    }
  }

  if (FAILED(hr = dxgi_adapter->QueryInterface(IID_PPV_ARGS(&dxgi_adapter_mtl)))) {
    ERR("D3D12CreateDevice: Not a DXMT adapter");
    return hr;
  }

  if (!ppDevice)
    return S_FALSE;

  return dxmt::CreateD3D12Device(dxgi_adapter_mtl.ptr(), riid, ppDevice);
}

extern "C" HRESULT WINAPI
D3D12GetInterface(REFCLSID rcslid, REFIID iid, void **debug) {
  return E_NOINTERFACE;
}

BOOL WINAPI
DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  if (reason != DLL_PROCESS_ATTACH)
    return TRUE;

  DisableThreadLibraryCalls(instance);
  return TRUE;
}

} // namespace dxmt