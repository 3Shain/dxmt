#include "d3d12_fence.hpp"
#include "d3d12_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"

#define FTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, "Fence::" fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

namespace dxmt {

MTLD3D12Fence::MTLD3D12Fence(MTLD3D12Device *device, uint64_t initial_value,
                             D3D12_FENCE_FLAGS flags)
    : m_device(device), m_flags(flags), m_value(initial_value) {
  m_device->AddRef();
  auto wmt_device = m_device->GetDXMTDevice().device();
  m_shared_event = wmt_device.newSharedEvent();
  m_shared_event.signalValue(initial_value);
  Logger::info(str::format("D3D12Fence: created value=", initial_value));
}

MTLD3D12Fence::~MTLD3D12Fence() {
  m_shared_event = nullptr;
  m_device->Release();
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Fence::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Pageable ||
      riid == IID_ID3D12Fence) {
    *ppvObject = ref(this);
    return S_OK;
  }
  FTRACE("QI unknown IID %s -> E_NOINTERFACE", str::format(riid).c_str());
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MTLD3D12Fence::AddRef() { return ++m_refCount; }

ULONG STDMETHODCALLTYPE MTLD3D12Fence::Release() {
  uint32_t rc = --m_refCount;
  if (!rc)
    delete this;
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Fence::GetPrivateData(REFGUID guid, UINT *data_size, void *data) {
  FTRACE("GetPrivateData E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Fence::SetPrivateData(REFGUID guid, UINT data_size,
                              const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Fence::SetPrivateDataInterface(REFGUID guid, const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Fence::SetName(LPCWSTR name) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Fence::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}

uint64_t STDMETHODCALLTYPE MTLD3D12Fence::GetCompletedValue() {
  FTRACE("GetCompletedValue -> %llu", (unsigned long long)m_value.load(std::memory_order_acquire));
  return m_value.load(std::memory_order_acquire);
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Fence::SetEventOnCompletion(uint64_t value, HANDLE event) {
  FTRACE("SetEventOnCompletion value=%llu current=%llu this=%p event=%p", (unsigned long long)value, (unsigned long long)m_value.load(), (void *)this, (void *)(uintptr_t)event);
  if (m_value.load(std::memory_order_acquire) >= value) {
    if (event)
      SetEvent(event);
    return S_OK;
  }
  if (!event) {
    if (m_value.load(std::memory_order_acquire) < value) {
      FTRACE("SetEventOnCompletion: null event, auto-signaling fence %p from %llu to %llu (sync replay already done)",
        (void *)this, (unsigned long long)m_value.load(), (unsigned long long)value);
      m_value.store(value, std::memory_order_release);
      if (m_shared_event.handle) {
        m_shared_event.signalValue(value);
      }
    }
    return S_OK;
  }
  if (m_shared_event.handle) {
    m_shared_event.waitUntilSignaledValue(value, UINT64_MAX);
  }
  SetEvent(event);
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Fence::Signal(uint64_t value) {
  FTRACE("Signal value=%llu this=%p", (unsigned long long)value, (void *)this);
  m_value.store(value, std::memory_order_release);
  if (m_shared_event.handle) {
    m_shared_event.signalValue(value);
  }
  return S_OK;
}

} // namespace dxmt
