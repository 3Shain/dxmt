#define INITGUID
#include "d3d12_device.hpp"
#include "com/com_pointer.hpp"
#include "dxmt_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include <d3d12.h>
#include <exception>

using namespace dxmt;

extern "C" HRESULT WINAPI
D3D12CreateDevice(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel,
                  REFIID riid, void **ppDevice) {
  if (!ppDevice)
    return E_POINTER;
  *ppDevice = nullptr;

  Com<IMTLDXGIAdapter> dxgi_adapter;

  if (pAdapter) {
    if (FAILED(pAdapter->QueryInterface(IID_PPV_ARGS(&dxgi_adapter)))) {
      ERR("D3D12CreateDevice: adapter is not a DXMT adapter");
      return E_INVALIDARG;
    }
  } else {
    Com<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
      ERR("D3D12CreateDevice: failed to create DXGI factory");
      return E_FAIL;
    }
    Com<IDXGIAdapter> adapter;
    if (FAILED(factory->EnumAdapters(0, &adapter))) {
      ERR("D3D12CreateDevice: no adapters available");
      return E_FAIL;
    }
    if (FAILED(adapter->QueryInterface(IID_PPV_ARGS(&dxgi_adapter)))) {
      ERR("D3D12CreateDevice: default adapter is not DXMT");
      return E_FAIL;
    }
  }

  try {
    auto device = new MTLD3D12Device(
        CreateDXMTDevice({.device = dxgi_adapter->GetMTLDevice()}),
        dxgi_adapter.ptr());

    HRESULT hr = device->QueryInterface(riid, ppDevice);
    if (FAILED(hr)) {
      device->Release();
      return hr;
    }

    Logger::info(str::format("D3D12CreateDevice: created device with FL ",
                             MinimumFeatureLevel));
    return S_OK;
  } catch (const std::exception &e) {
    Logger::err(str::format("D3D12CreateDevice: exception: ", e.what()));
    return E_FAIL;
  }
}

extern "C" HRESULT WINAPI
D3D12SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC *pRootSignature,
                            D3D_ROOT_SIGNATURE_VERSION Version,
                            ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob) {
  Logger::info("D3D12SerializeRootSignature: stub");
  return E_NOTIMPL;
}

extern "C" HRESULT WINAPI
D3D12SerializeVersionedRootSignature(
    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pRootSignature,
    ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob) {
  Logger::info("D3D12SerializeVersionedRootSignature: stub");
  return E_NOTIMPL;
}

extern "C" HRESULT WINAPI
D3D12CreateRootSignatureDeserializer(const void *pData, SIZE_T NumBytes,
                                     REFIID riid, void **ppDeserializer) {
  Logger::info("D3D12CreateRootSignatureDeserializer: stub");
  return E_NOTIMPL;
}

extern "C" HRESULT WINAPI D3D12GetDebugInterface(REFIID riid,
                                                 void **ppDebug) {
  return E_NOINTERFACE;
}

#ifdef _WIN32
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  if (reason != DLL_PROCESS_ATTACH)
    return TRUE;
  DisableThreadLibraryCalls(instance);
  return TRUE;
}
#endif
