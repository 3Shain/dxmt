#include "d3d12_descriptor_heap.hpp"
#include "d3d12_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include <cstring>
#include <cstdlib>
#include <windows.h>

#define HTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, "DescHeap::" fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

namespace dxmt {

MTLD3D12DescriptorHeap::MTLD3D12DescriptorHeap(
    MTLD3D12Device *device, const D3D12_DESCRIPTOR_HEAP_DESC &desc)
    : m_device(device), m_desc(desc), m_data(nullptr), m_data_is_virtual(false), m_data_size(0) {
  HTRACE("DescriptorHeap ctor: device=%p type=%u num=%u this=%p", (void*)m_device, desc.Type, desc.NumDescriptors, (void*)this);
  size_t alloc_size = (size_t)desc.NumDescriptors * sizeof(D3D12Descriptor);
  HTRACE("DescriptorHeap ctor: alloc_size computed");
  m_data_size = alloc_size;
  HTRACE("DescriptorHeap ctor: type=%u num=%u alloc=%u bytes", (unsigned)desc.Type, (unsigned)desc.NumDescriptors, (unsigned)alloc_size);
  if (alloc_size >= 1024 * 1024) {
    m_data_is_virtual = true;
    m_data = (D3D12Descriptor *)VirtualAlloc(nullptr, alloc_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    HTRACE("DescriptorHeap ctor: VirtualAlloc data=%p", (void *)m_data);
  } else {
    m_data = (D3D12Descriptor *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, alloc_size);
    HTRACE("DescriptorHeap ctor: HeapAlloc data=%p", (void *)m_data);
  }
  if (m_data) {
    HTRACE("DescriptorHeap ctor: OK data=%p", (void *)m_data);
  } else {
    HTRACE("DescriptorHeap ctor: ALLOC FAILED for %u bytes!", (unsigned)alloc_size);
  }
  Logger::info(str::format("D3D12DescriptorHeap: type=", desc.Type,
                            " count=", desc.NumDescriptors,
                            " flags=", desc.Flags,
                            " data=", (void *)m_data));
}

MTLD3D12DescriptorHeap::~MTLD3D12DescriptorHeap() {
  if (m_data) {
    if (m_data_is_virtual) {
      VirtualFree(m_data, 0, MEM_RELEASE);
    } else {
      HeapFree(GetProcessHeap(), 0, m_data);
    }
  }
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Pageable ||
      riid == IID_ID3D12DescriptorHeap) {
    *ppvObject = ref(this);
    return S_OK;
  }
  HTRACE("QI unknown IID %s -> E_NOINTERFACE", str::format(riid).c_str());
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::AddRef() { return ++m_refCount; }

ULONG STDMETHODCALLTYPE MTLD3D12DescriptorHeap::Release() {
  uint32_t rc = --m_refCount;
  if (!rc) {
    this->~MTLD3D12DescriptorHeap();
    HeapFree(GetProcessHeap(), 0, this);
  }
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::GetPrivateData(REFGUID guid, UINT *data_size,
                                        void *data) {
  HTRACE("GetPrivateData E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::SetPrivateData(REFGUID guid, UINT data_size,
                                       const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::SetPrivateDataInterface(REFGUID guid,
                                                const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::SetName(LPCWSTR name) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}

D3D12_DESCRIPTOR_HEAP_DESC *STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::GetDesc(D3D12_DESCRIPTOR_HEAP_DESC *__ret) {
  *__ret = m_desc;
  return __ret;
}

D3D12_CPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::GetCPUDescriptorHandleForHeapStart(
    D3D12_CPU_DESCRIPTOR_HANDLE *__ret) {
  HTRACE("GetCPUDescriptorHandleForHeapStart");
  __ret->ptr = reinterpret_cast<SIZE_T>(m_data);
  return __ret;
}

D3D12_GPU_DESCRIPTOR_HANDLE *STDMETHODCALLTYPE
MTLD3D12DescriptorHeap::GetGPUDescriptorHandleForHeapStart(
    D3D12_GPU_DESCRIPTOR_HANDLE *__ret) {
  HTRACE("GetGPUDescriptorHandleForHeapStart ptr=0x%llx", (unsigned long long)reinterpret_cast<UINT64>(m_data));
  __ret->ptr = reinterpret_cast<UINT64>(m_data);
  return __ret;
}

} // namespace dxmt
