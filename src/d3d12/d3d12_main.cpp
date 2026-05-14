#include "d3d12_private.h"
#include "d3d12_device.hpp"
#include "dxgi_interfaces.h"
#include "dxmt_device.hpp"
#include "log/log.hpp"
#include "com/com_pointer.hpp"
#include "util_string.hpp"

namespace dxmt::d3d12 {

// ──────────────────────────────────────────────
// D3D12CreateDevice
// ──────────────────────────────────────────────

extern "C" HRESULT WINAPI D3D12CreateDevice(
    IUnknown *pAdapter,
    D3D12_FEATURE_LEVEL MinimumFeatureLevel,
    REFIID riid,
    void **ppDevice) {

    InitReturnPtr(ppDevice);

    if (pAdapter == nullptr) {
        ERR("D3D12CreateDevice: No adapter provided");
        return E_INVALIDARG;
    }

    // Try to find the corresponding Metal device from the DXGI adapter
    Com<IMTLDXGIAdapter> dxgi_adapter;
    if (FAILED(pAdapter->QueryInterface(IID_PPV_ARGS(&dxgi_adapter)))) {
        ERR("D3D12CreateDevice: Not a DXMT adapter");
        return E_INVALIDARG;
    }

    // Determine the feature level
    D3D_FEATURE_LEVEL feature_level;
    WMT::Device mtl_device = dxgi_adapter->GetMTLDevice();

    if (mtl_device == nullptr) {
        ERR("D3D12CreateDevice: MTLDevice is null");
        return E_FAIL;
    }

    // Probe supported feature levels
    // For M1, report 12.0 if Metal 3.0+ is available
    WMTMetalVersion metal_ver = WMTMetalVersionMax;
    uint64_t macos_major = 0, macos_minor = 0;
    WMTGetOSVersion(&macos_major, &macos_minor, nullptr);

    if (macos_major >= 15) {
        metal_ver = std::min(WMTMetal320, metal_ver);
    } else {
        metal_ver = std::min(WMTMetal310, metal_ver);
    }

    if (metal_ver >= WMTMetal310) {
        feature_level = D3D_FEATURE_LEVEL_12_1;
    } else {
        feature_level = D3D_FEATURE_LEVEL_12_0;
    }

    // Cap at what the caller asked for
    if (feature_level > MinimumFeatureLevel) {
        // Use the highest available, caller accepts anything >= MinimumFeatureLevel
    } else if (feature_level < MinimumFeatureLevel) {
        ERR("D3D12CreateDevice: Requested feature level not supported. ",
            "Available: ", feature_level == D3D_FEATURE_LEVEL_12_1 ? "12.1" : "12.0",
            ", Requested: >= ", MinimumFeatureLevel == D3D_FEATURE_LEVEL_12_2 ? "12.2" :
                                  MinimumFeatureLevel == D3D_FEATURE_LEVEL_12_1 ? "12.1" : "12.0");
        return E_FAIL;
    }

    TRACE("D3D12CreateDevice: Metal device: ", mtl_device.name().getUTF8String());
    TRACE("D3D12CreateDevice: Feature level: ",
          feature_level == D3D_FEATURE_LEVEL_12_1 ? "12.1" : "12.0");

    // Create the DXMT device
    auto dxmt_device = CreateDXMTDevice({.device = mtl_device});

    // Create the D3D12 device
    auto *device = new D3D12Device(std::move(dxmt_device), feature_level);

    HRESULT hr = device->QueryInterface(riid, ppDevice);
    device->Release();

    if (FAILED(hr)) {
        ERR("D3D12CreateDevice: Failed to query requested interface");
        return hr;
    }

    return S_OK;
}

// ──────────────────────────────────────────────
// D3D12GetDebugInterface
// ──────────────────────────────────────────────

extern "C" HRESULT WINAPI D3D12GetDebugInterface(
    REFIID riid,
    void **ppDebug) {

    if (ppDebug == nullptr)
        return E_POINTER;

    *ppDebug = nullptr;
    WARN("D3D12GetDebugInterface: Debug layer not supported");
    return E_NOINTERFACE;
}

} // namespace dxmt::d3d12
