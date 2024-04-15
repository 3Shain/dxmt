#include "dxgi_adapter.h"
#include "com/com_guid.hpp"
#include "dxgi_output.h"
#include "util_string.hpp"
#include "log/log.hpp"
#include "wsi_monitor.hpp"
#include "dxgi_interfaces.h"

namespace dxmt {

MTLDXGIAdatper::MTLDXGIAdatper(MTL::Device *device, MTLDXGIFactory *factory)
    : m_deivce(device), m_factory(factory) {
  m_deivce->retain();
}

MTLDXGIAdatper::~MTLDXGIAdatper() { m_deivce->release(); }

HRESULT STDMETHODCALLTYPE MTLDXGIAdatper::GetParent(REFIID riid,
                                                    void **ppParent) {
  return m_factory->QueryInterface(riid, ppParent);
}

HRESULT STDMETHODCALLTYPE MTLDXGIAdatper::QueryInterface(const IID &riid,
                                                         void **ppvObject) {
  if (ppvObject == nullptr)
    return E_POINTER;

  *ppvObject = nullptr;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(IDXGIObject) ||
      riid == __uuidof(IDXGIAdapter) || riid == __uuidof(IDXGIAdapter1) ||
      riid == __uuidof(IDXGIAdapter2) || riid == __uuidof(IMTLDXGIAdatper)) {
    *ppvObject = ref(this);
    return S_OK;
  }

  if (logQueryInterfaceError(__uuidof(IDXGIAdapter), riid)) {
    Logger::warn("DxgiAdapter::QueryInterface: Unknown interface query");
    Logger::warn(str::format(riid));
  }

  return E_NOINTERFACE;
};

HRESULT STDMETHODCALLTYPE MTLDXGIAdatper::GetDesc(DXGI_ADAPTER_DESC *pDesc) {
  if (pDesc == nullptr)
    return E_INVALIDARG;

  DXGI_ADAPTER_DESC2 desc;
  HRESULT hr = GetDesc2(&desc);

  if (SUCCEEDED(hr)) {
    std::memcpy(pDesc->Description, desc.Description,
                sizeof(pDesc->Description));

    pDesc->VendorId = desc.VendorId;
    pDesc->DeviceId = desc.DeviceId;
    pDesc->SubSysId = desc.SubSysId;
    pDesc->Revision = desc.Revision;
    pDesc->DedicatedVideoMemory = desc.DedicatedVideoMemory;
    pDesc->DedicatedSystemMemory = desc.DedicatedSystemMemory;
    pDesc->SharedSystemMemory = desc.SharedSystemMemory;
    pDesc->AdapterLuid = desc.AdapterLuid;
  }

  return hr;
}

HRESULT STDMETHODCALLTYPE MTLDXGIAdatper::GetDesc1(DXGI_ADAPTER_DESC1 *pDesc) {
  if (pDesc == nullptr)
    return E_INVALIDARG;

  DXGI_ADAPTER_DESC2 desc;
  HRESULT hr = GetDesc2(&desc);

  if (SUCCEEDED(hr)) {
    std::memcpy(pDesc->Description, desc.Description,
                sizeof(pDesc->Description));

    pDesc->VendorId = desc.VendorId;
    pDesc->DeviceId = desc.DeviceId;
    pDesc->SubSysId = desc.SubSysId;
    pDesc->Revision = desc.Revision;
    pDesc->DedicatedVideoMemory = desc.DedicatedVideoMemory;
    pDesc->DedicatedSystemMemory = desc.DedicatedSystemMemory;
    pDesc->SharedSystemMemory = desc.SharedSystemMemory;
    pDesc->AdapterLuid = desc.AdapterLuid;
    pDesc->Flags = desc.Flags;
  }

  return hr;
}

HRESULT STDMETHODCALLTYPE MTLDXGIAdatper::GetDesc2(DXGI_ADAPTER_DESC2 *pDesc) {
  if (pDesc == nullptr)
    return E_INVALIDARG;

  //   pDesc->VendorId = deviceProp.vendorID;
  //   pDesc->DeviceId = deviceProp.deviceID;
  //   pDesc->SubSysId = 0;
  //   pDesc->Revision = 0;
  //   pDesc->DedicatedVideoMemory = deviceMemory;
  //   pDesc->DedicatedSystemMemory = 0;
  //   pDesc->SharedSystemMemory = sharedMemory;
  //   pDesc->AdapterLuid = LUID{0, 0};
  //   pDesc->Flags = DXGI_ADAPTER_FLAG3_NONE;
  //   pDesc->GraphicsPreemptionGranularity =
  //       DXGI_GRAPHICS_PREEMPTION_DMA_BUFFER_BOUNDARY;
  //   pDesc->ComputePreemptionGranularity =
  //       DXGI_COMPUTE_PREEMPTION_DMA_BUFFER_BOUNDARY;

  // TODO

  return S_OK;
}

HRESULT STDMETHODVCALLTYPE MTLDXGIAdatper::CheckInterfaceSupport(
    const GUID &guid, LARGE_INTEGER *umd_version) {
  HRESULT hr = DXGI_ERROR_UNSUPPORTED;

  if (guid == __uuidof(IDXGIDevice))
    hr = S_OK;

  // We can't really reconstruct the version numbers
  // returned by Windows drivers from Metal
  if (SUCCEEDED(hr) && umd_version)
    umd_version->QuadPart = ~0ull;

  if (FAILED(hr)) {
    Logger::err("DXGI: CheckInterfaceSupport: Unsupported interface");
    Logger::err(str::format(guid));
  }

  return hr;
}

HRESULT STDMETHODCALLTYPE MTLDXGIAdatper::EnumOutputs(UINT Output,
                                                      IDXGIOutput **ppOutput) {
  InitReturnPtr(ppOutput);

  if (ppOutput == nullptr)
    return E_INVALIDARG;

  HMONITOR monitor = wsi::enumMonitors(Output);
  if (monitor == nullptr)
    return DXGI_ERROR_NOT_FOUND; // TODO

  *ppOutput = ref(new MTLDXGIOutput(m_factory.ptr(), this, monitor)); // TODO
  return S_OK;
}

MTL::Device *STDMETHODCALLTYPE MTLDXGIAdatper::GetMTLDevice() {
  return m_deivce.ptr();
}

HRESULT STDMETHODCALLTYPE MTLDXGIAdatper::QueryFormatDesc(DXGI_FORMAT dxgiFormat, MTL_FORMAT_DESC* pMtlDesc) {
  return S_OK;
};

} // namespace dxmt