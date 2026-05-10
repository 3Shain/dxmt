#define INITGUID
#include "d3d12_dxgi_device.hpp"
#include "d3d12_device.hpp"
#include "com/com_pointer.hpp"
#include "dxgi_interfaces.h"
#include "dxmt_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include <d3d12.h>
#include <exception>
#include <vector>
#include <cstring>

#pragma pack(push, 1)
struct _RSHeader {
  uint32_t num_parameters;
  uint32_t num_static_samplers;
  uint32_t flags;
};
struct _RSParameter {
  uint8_t type;
  uint8_t visibility;
  union {
    struct { uint32_t register_space; uint32_t register_index; uint32_t num_32bit_values; } constants;
    struct { uint32_t register_space; uint32_t register_index; } descriptor;
    struct { uint32_t num_ranges; } table;
  };
};
struct _RSDescriptorRange {
  uint8_t range_type;
  uint32_t num_descriptors;
  uint32_t base_register;
  uint32_t register_space;
  uint32_t offset_in_table;
};
struct _RSStaticSampler {
  uint32_t filter;
  uint32_t address_u;
  uint32_t address_v;
  uint32_t address_w;
  float mip_lod_bias;
  uint32_t max_anisotropy;
  uint32_t comparison_func;
  uint32_t border_color;
  float min_lod;
  float max_lod;
  uint32_t register_space;
  uint32_t register_index;
  uint32_t shader_register_space;
  uint8_t shader_visibility;
};
#pragma pack(pop)

class _RSBlob : public ID3DBlob {
  ULONG m_ref = 1;
  std::vector<uint8_t> m_data;
public:
  _RSBlob(std::vector<uint8_t> &&data) : m_data(std::move(data)) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) {
    if (riid == IID_IUnknown || riid == IID_ID3D10Blob || riid == __uuidof(ID3DBlob)) { *ppv = this; AddRef(); return S_OK; }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() { return ++m_ref; }
  ULONG STDMETHODCALLTYPE Release() { ULONG r = --m_ref; if (!r) delete this; return r; }
  LPVOID STDMETHODCALLTYPE GetBufferPointer() { return m_data.data(); }
  SIZE_T STDMETHODCALLTYPE GetBufferSize() { return m_data.size(); }
};

static HRESULT _SerializeRootSig(const D3D12_ROOT_SIGNATURE_DESC *desc, ID3DBlob **ppBlob) {
  if (!desc || !ppBlob) return E_INVALIDARG;
  *ppBlob = nullptr;

  std::vector<uint8_t> buf;
  size_t total = sizeof(_RSHeader) + desc->NumParameters * sizeof(_RSParameter);
  for (UINT i = 0; i < desc->NumParameters; i++) {
    if (desc->pParameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      total += desc->pParameters[i].DescriptorTable.NumDescriptorRanges * sizeof(_RSDescriptorRange);
  }
  total += desc->NumStaticSamplers * sizeof(_RSStaticSampler);
  buf.resize(total);

  auto *hdr = reinterpret_cast<_RSHeader *>(buf.data());
  hdr->num_parameters = desc->NumParameters;
  hdr->num_static_samplers = desc->NumStaticSamplers;
  hdr->flags = desc->Flags;

  uint8_t *ptr = buf.data() + sizeof(_RSHeader);
  for (UINT i = 0; i < desc->NumParameters; i++) {
    auto &p = desc->pParameters[i];
    auto *out = reinterpret_cast<_RSParameter *>(ptr);
    out->type = (uint8_t)p.ParameterType;
    out->visibility = (uint8_t)p.ShaderVisibility;
    switch (p.ParameterType) {
    case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
      out->constants.register_space = p.Constants.RegisterSpace;
      out->constants.register_index = p.Constants.ShaderRegister;
      out->constants.num_32bit_values = p.Constants.Num32BitValues;
      break;
    case D3D12_ROOT_PARAMETER_TYPE_CBV:
    case D3D12_ROOT_PARAMETER_TYPE_SRV:
    case D3D12_ROOT_PARAMETER_TYPE_UAV:
      out->descriptor.register_space = p.Descriptor.RegisterSpace;
      out->descriptor.register_index = p.Descriptor.ShaderRegister;
      break;
    case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
      out->table.num_ranges = p.DescriptorTable.NumDescriptorRanges;
      ptr += sizeof(_RSParameter);
      for (UINT r = 0; r < p.DescriptorTable.NumDescriptorRanges; r++) {
        auto &rng = p.DescriptorTable.pDescriptorRanges[r];
        auto *orng = reinterpret_cast<_RSDescriptorRange *>(ptr);
        orng->range_type = (uint8_t)rng.RangeType;
        orng->num_descriptors = rng.NumDescriptors;
        orng->base_register = rng.BaseShaderRegister;
        orng->register_space = rng.RegisterSpace;
        orng->offset_in_table = rng.OffsetInDescriptorsFromTableStart;
        ptr += sizeof(_RSDescriptorRange);
      }
      continue;
    }
    ptr += sizeof(_RSParameter);
  }

  for (UINT i = 0; i < desc->NumStaticSamplers; i++) {
    auto &s = desc->pStaticSamplers[i];
    auto *out = reinterpret_cast<_RSStaticSampler *>(ptr);
    out->filter = s.Filter;
    out->address_u = s.AddressU;
    out->address_v = s.AddressV;
    out->address_w = s.AddressW;
    out->mip_lod_bias = s.MipLODBias;
    out->max_anisotropy = s.MaxAnisotropy;
    out->comparison_func = s.ComparisonFunc;
    out->border_color = s.BorderColor;
    out->min_lod = s.MinLOD;
    out->max_lod = s.MaxLOD;
    out->register_space = s.RegisterSpace;
    out->register_index = s.ShaderRegister;
    out->shader_register_space = s.RegisterSpace;
    out->shader_visibility = (uint8_t)s.ShaderVisibility;
    ptr += sizeof(_RSStaticSampler);
  }

  *ppBlob = new _RSBlob(std::move(buf));
  return S_OK;
}

using namespace dxmt;

extern "C" HRESULT WINAPI
D3D12CreateDevice(IUnknown *pAdapter, D3D_FEATURE_LEVEL MinimumFeatureLevel,
                  REFIID riid, void **ppDevice) {
  {
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) { fprintf(f, "D3D12CreateDevice called FL=%d adapter=%p\n", MinimumFeatureLevel, pAdapter); fclose(f); }
  }
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
    void *device_mem = VirtualAlloc((void*)0x500000000ULL, sizeof(MTLD3D12DXGIDevice),
      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!device_mem) {
      device_mem = VirtualAlloc((void*)0x200000000ULL, sizeof(MTLD3D12DXGIDevice),
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
    if (!device_mem) {
      device_mem = VirtualAlloc(nullptr, sizeof(MTLD3D12DXGIDevice),
        MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
    {
      FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
      if (f) { fprintf(f, "Device allocated at %p size=%zu\n", device_mem, sizeof(MTLD3D12DXGIDevice)); fclose(f); }
    }
    auto dxgi_device = new (device_mem) MTLD3D12DXGIDevice(
        CreateDXMTDevice({.device = dxgi_adapter->GetMTLDevice()}),
        dxgi_adapter.ptr());

    HRESULT hr = dxgi_device->QueryInterface(riid, ppDevice);
    if (FAILED(hr)) {
      {
        FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
        if (f) { fprintf(f, "D3D12CreateDevice QI FAILED hr=0x%lx FL=%d\n", hr, MinimumFeatureLevel); fclose(f); }
      }
      dxgi_device->Release();
      return hr;
    }

    Logger::info(str::format("D3D12CreateDevice: created device with FL ",
                             MinimumFeatureLevel));
    {
      FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
      if (f) { fprintf(f, "D3D12CreateDevice SUCCESS FL=%d\n", MinimumFeatureLevel); fclose(f); }
    }
    return S_OK;
  } catch (const std::exception &e) {
    Logger::err(str::format("D3D12CreateDevice: exception: ", e.what()));
    {
      FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
      if (f) { fprintf(f, "D3D12CreateDevice EXCEPTION: %s FL=%d\n", e.what(), MinimumFeatureLevel); fclose(f); }
    }
    return E_FAIL;
  }
}

extern "C" HRESULT WINAPI
D3D12SerializeRootSignature(const D3D12_ROOT_SIGNATURE_DESC *pRootSignature,
                             D3D_ROOT_SIGNATURE_VERSION Version,
                             ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob) {
  {
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) { fprintf(f, "D3D12SerializeRootSignature version=%u params=%u\n", Version, pRootSignature ? pRootSignature->NumParameters : 0); fclose(f); }
  }
  return _SerializeRootSig(pRootSignature, ppBlob);
}

extern "C" HRESULT WINAPI
D3D12SerializeVersionedRootSignature(
    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pRootSignature,
    ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob) {
  {
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) { fprintf(f, "D3D12SerializeVersionedRootSignature version=%u\n", pRootSignature ? pRootSignature->Version : 0); fclose(f); }
  }
  if (!pRootSignature) return E_INVALIDARG;
  if (pRootSignature->Version == D3D_ROOT_SIGNATURE_VERSION_1_0)
    return _SerializeRootSig(&pRootSignature->Desc_1_0, ppBlob);
  if (pRootSignature->Version == D3D_ROOT_SIGNATURE_VERSION_1_1) {
    const auto &d1 = pRootSignature->Desc_1_1;
    D3D12_ROOT_SIGNATURE_DESC desc0 = {};
    desc0.NumParameters = d1.NumParameters;
    desc0.NumStaticSamplers = d1.NumStaticSamplers;
    desc0.pStaticSamplers = d1.pStaticSamplers;
    desc0.Flags = d1.Flags;
    std::vector<D3D12_ROOT_PARAMETER> params(d1.NumParameters);
    std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
    for (UINT i = 0; i < d1.NumParameters; i++) {
      auto &src = d1.pParameters[i];
      auto &dst = params[i];
      dst.ParameterType = src.ParameterType;
      dst.ShaderVisibility = src.ShaderVisibility;
      switch (src.ParameterType) {
      case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
        dst.Constants = src.Constants; break;
      case D3D12_ROOT_PARAMETER_TYPE_CBV:
      case D3D12_ROOT_PARAMETER_TYPE_SRV:
      case D3D12_ROOT_PARAMETER_TYPE_UAV:
        dst.Descriptor.ShaderRegister = src.Descriptor.ShaderRegister;
        dst.Descriptor.RegisterSpace = src.Descriptor.RegisterSpace; break;
      case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
        dst.DescriptorTable.NumDescriptorRanges = src.DescriptorTable.NumDescriptorRanges;
        size_t base = ranges.size();
        for (UINT r = 0; r < src.DescriptorTable.NumDescriptorRanges; r++) {
          auto &rs = src.DescriptorTable.pDescriptorRanges[r];
          D3D12_DESCRIPTOR_RANGE dr = {};
          dr.RangeType = rs.RangeType;
          dr.NumDescriptors = rs.NumDescriptors;
          dr.BaseShaderRegister = rs.BaseShaderRegister;
          dr.RegisterSpace = rs.RegisterSpace;
          dr.OffsetInDescriptorsFromTableStart = rs.OffsetInDescriptorsFromTableStart;
          ranges.push_back(dr);
        }
        dst.DescriptorTable.pDescriptorRanges = ranges.data() + base;
        break;
      }
      }
    }
    desc0.pParameters = params.data();
    return _SerializeRootSig(&desc0, ppBlob);
  }
  return E_INVALIDARG;
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
extern void install_crash_handler();
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  if (reason != DLL_PROCESS_ATTACH)
    return TRUE;
  DisableThreadLibraryCalls(instance);
  install_crash_handler();
  return TRUE;
}
#endif
