#define INITGUID
#include "d3d12_dxgi_device.hpp"
#include "d3d12_device.hpp"
#include "com/com_pointer.hpp"
#include "dxgi_interfaces.h"
#include "dxmt_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include <d3d12.h>
#include <atomic>
#include <cstdarg>
#include <exception>
#include <vector>
#include <cstring>

namespace {

constexpr UINT kD3D12AgilitySDKVersion = 620;

constexpr GUID kCLSID_D3D12SDKConfiguration = {
    0x7cda6aca,
    0xa03e,
    0x49c8,
    {0x94, 0x58, 0x03, 0x34, 0xd2, 0x0e, 0x07, 0xce}};
constexpr GUID kIID_ID3D12SDKConfiguration = {
    0xe9eb5314,
    0x33aa,
    0x42b2,
    {0xa7, 0x18, 0xd7, 0x7f, 0x58, 0xb1, 0xf1, 0xc7}};
constexpr GUID kIID_ID3D12SDKConfiguration1 = {
    0x8aaf9303,
    0xad25,
    0x48b9,
    {0x9a, 0x57, 0xd9, 0xc3, 0x7e, 0x00, 0x9d, 0x9f}};

struct ID3D12SDKConfiguration : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE SetSDKVersion(UINT SDKVersion,
                                                  LPCSTR SDKPath) = 0;
};

struct ID3D12SDKConfiguration1 : public ID3D12SDKConfiguration {
  virtual HRESULT STDMETHODCALLTYPE CreateDeviceFactory(UINT SDKVersion,
                                                        LPCSTR SDKPath,
                                                        REFIID riid,
                                                        void **ppvFactory) = 0;
  virtual void STDMETHODCALLTYPE FreeUnusedSDKs() = 0;
};

static void TraceAgility(const char *fmt, ...) {
  FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
  if (!f)
    return;

  va_list args;
  va_start(args, fmt);
  vfprintf(f, fmt, args);
  va_end(args);
  fprintf(f, "\n");
  fclose(f);
}

class MTLD3D12SDKConfiguration final : public ID3D12SDKConfiguration1 {
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
    if (!ppv)
      return E_POINTER;

    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == kIID_ID3D12SDKConfiguration ||
        riid == kIID_ID3D12SDKConfiguration1) {
      *ppv = this;
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }

  ULONG STDMETHODCALLTYPE Release() override {
    ULONG ref = --m_ref;
    if (!ref)
      delete this;
    return ref;
  }

  HRESULT STDMETHODCALLTYPE SetSDKVersion(UINT SDKVersion,
                                          LPCSTR SDKPath) override {
    TraceAgility("ID3D12SDKConfiguration::SetSDKVersion version=%u path=%s "
                 "accepted_runtime=%u",
                 SDKVersion, SDKPath ? SDKPath : "(null)",
                 kD3D12AgilitySDKVersion);
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE CreateDeviceFactory(UINT SDKVersion, LPCSTR SDKPath,
                                                REFIID riid,
                                                void **ppvFactory) override {
    if (!ppvFactory)
      return E_POINTER;

    *ppvFactory = nullptr;
    TraceAgility("ID3D12SDKConfiguration1::CreateDeviceFactory version=%u "
                 "path=%s riid=%s -> E_NOINTERFACE",
                 SDKVersion, SDKPath ? SDKPath : "(null)",
                 dxmt::str::format(riid).c_str());
    return E_NOINTERFACE;
  }

  void STDMETHODCALLTYPE FreeUnusedSDKs() override {
    TraceAgility("ID3D12SDKConfiguration1::FreeUnusedSDKs");
  }

private:
  std::atomic<ULONG> m_ref = {1};
};

} // namespace

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
    struct {
      uint32_t register_space;
      uint32_t register_index;
      uint32_t num_32bit_values;
    } constants;
    struct {
      uint32_t register_space;
      uint32_t register_index;
    } descriptor;
    struct {
      uint32_t num_ranges;
    } table;
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
    if (riid == IID_IUnknown || riid == IID_ID3D10Blob ||
        riid == __uuidof(ID3DBlob)) {
      *ppv = this;
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }
  ULONG STDMETHODCALLTYPE AddRef() { return ++m_ref; }
  ULONG STDMETHODCALLTYPE Release() {
    ULONG r = --m_ref;
    if (!r)
      delete this;
    return r;
  }
  LPVOID STDMETHODCALLTYPE GetBufferPointer() { return m_data.data(); }
  SIZE_T STDMETHODCALLTYPE GetBufferSize() { return m_data.size(); }
};

class _RSDeserializer final : public ID3D12RootSignatureDeserializer,
                              public ID3D12VersionedRootSignatureDeserializer {
public:
  _RSDeserializer(const void *data, SIZE_T size) { m_valid = Parse(data, size); }

  bool Valid() const { return m_valid; }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppv) override {
    if (!ppv)
      return E_POINTER;
    *ppv = nullptr;

    if (riid == IID_IUnknown || riid == IID_ID3D12RootSignatureDeserializer) {
      *ppv = static_cast<ID3D12RootSignatureDeserializer *>(this);
      AddRef();
      return S_OK;
    }
    if (riid == IID_ID3D12VersionedRootSignatureDeserializer) {
      *ppv = static_cast<ID3D12VersionedRootSignatureDeserializer *>(this);
      AddRef();
      return S_OK;
    }
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override { return ++m_ref; }

  ULONG STDMETHODCALLTYPE Release() override {
    ULONG r = --m_ref;
    if (!r)
      delete this;
    return r;
  }

  const D3D12_ROOT_SIGNATURE_DESC *STDMETHODCALLTYPE
  GetRootSignatureDesc() override {
    return &m_desc;
  }

  HRESULT STDMETHODCALLTYPE GetRootSignatureDescAtVersion(
      D3D_ROOT_SIGNATURE_VERSION version,
      const D3D12_VERSIONED_ROOT_SIGNATURE_DESC **desc) override {
    if (!desc)
      return E_POINTER;
    *desc = nullptr;
    if (version != D3D_ROOT_SIGNATURE_VERSION_1_0)
      return E_INVALIDARG;
    *desc = &m_versioned_desc;
    return S_OK;
  }

  const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *STDMETHODCALLTYPE
  GetUnconvertedRootSignatureDesc() override {
    return &m_versioned_desc;
  }

private:
  bool Parse(const void *data, SIZE_T size) {
    if (!data || size < sizeof(_RSHeader))
      return false;

    const uint8_t *ptr = static_cast<const uint8_t *>(data);
    const uint8_t *end = ptr + size;
    auto canRead = [&](SIZE_T bytes) -> bool {
      return bytes <= static_cast<SIZE_T>(end - ptr);
    };

    auto *hdr = reinterpret_cast<const _RSHeader *>(ptr);
    if (hdr->num_parameters > size / sizeof(_RSParameter) ||
        hdr->num_static_samplers > size / sizeof(_RSStaticSampler))
      return false;

    ptr += sizeof(_RSHeader);
    m_params.resize(hdr->num_parameters);
    m_ranges.resize(hdr->num_parameters);
    m_static_samplers.resize(hdr->num_static_samplers);

    for (UINT i = 0; i < hdr->num_parameters; i++) {
      if (!canRead(sizeof(_RSParameter)))
        return false;

      auto *src = reinterpret_cast<const _RSParameter *>(ptr);
      auto &dst = m_params[i];
      dst.ParameterType = static_cast<D3D12_ROOT_PARAMETER_TYPE>(src->type);
      dst.ShaderVisibility = static_cast<D3D12_SHADER_VISIBILITY>(src->visibility);
      ptr += sizeof(_RSParameter);

      switch (dst.ParameterType) {
      case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
        dst.Constants.RegisterSpace = src->constants.register_space;
        dst.Constants.ShaderRegister = src->constants.register_index;
        dst.Constants.Num32BitValues = src->constants.num_32bit_values;
        break;
      case D3D12_ROOT_PARAMETER_TYPE_CBV:
      case D3D12_ROOT_PARAMETER_TYPE_SRV:
      case D3D12_ROOT_PARAMETER_TYPE_UAV:
        dst.Descriptor.RegisterSpace = src->descriptor.register_space;
        dst.Descriptor.ShaderRegister = src->descriptor.register_index;
        break;
      case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
        if (src->table.num_ranges > static_cast<UINT>((end - ptr) / sizeof(_RSDescriptorRange)))
          return false;
        m_ranges[i].resize(src->table.num_ranges);
        for (UINT r = 0; r < src->table.num_ranges; r++) {
          auto *range = reinterpret_cast<const _RSDescriptorRange *>(ptr);
          auto &out_range = m_ranges[i][r];
          out_range.RangeType =
              static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(range->range_type);
          out_range.NumDescriptors = range->num_descriptors;
          out_range.BaseShaderRegister = range->base_register;
          out_range.RegisterSpace = range->register_space;
          out_range.OffsetInDescriptorsFromTableStart = range->offset_in_table;
          ptr += sizeof(_RSDescriptorRange);
        }
        dst.DescriptorTable.NumDescriptorRanges = src->table.num_ranges;
        dst.DescriptorTable.pDescriptorRanges = m_ranges[i].data();
        break;
      default:
        return false;
      }
    }

    for (UINT i = 0; i < hdr->num_static_samplers; i++) {
      if (!canRead(sizeof(_RSStaticSampler)))
        return false;

      auto *src = reinterpret_cast<const _RSStaticSampler *>(ptr);
      auto &dst = m_static_samplers[i];
      dst.Filter = static_cast<D3D12_FILTER>(src->filter);
      dst.AddressU = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(src->address_u);
      dst.AddressV = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(src->address_v);
      dst.AddressW = static_cast<D3D12_TEXTURE_ADDRESS_MODE>(src->address_w);
      dst.MipLODBias = src->mip_lod_bias;
      dst.MaxAnisotropy = src->max_anisotropy;
      dst.ComparisonFunc = static_cast<D3D12_COMPARISON_FUNC>(src->comparison_func);
      dst.BorderColor = static_cast<D3D12_STATIC_BORDER_COLOR>(src->border_color);
      dst.MinLOD = src->min_lod;
      dst.MaxLOD = src->max_lod;
      dst.ShaderRegister = src->register_index;
      dst.RegisterSpace = src->register_space;
      dst.ShaderVisibility =
          static_cast<D3D12_SHADER_VISIBILITY>(src->shader_visibility);
      ptr += sizeof(_RSStaticSampler);
    }

    m_desc.NumParameters = hdr->num_parameters;
    m_desc.pParameters = m_params.empty() ? nullptr : m_params.data();
    m_desc.NumStaticSamplers = hdr->num_static_samplers;
    m_desc.pStaticSamplers =
        m_static_samplers.empty() ? nullptr : m_static_samplers.data();
    m_desc.Flags = static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(hdr->flags);

    m_versioned_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
    m_versioned_desc.Desc_1_0 = m_desc;
    return ptr <= end;
  }

  ULONG m_ref = 1;
  bool m_valid = false;
  D3D12_ROOT_SIGNATURE_DESC m_desc = {};
  D3D12_VERSIONED_ROOT_SIGNATURE_DESC m_versioned_desc = {};
  std::vector<D3D12_ROOT_PARAMETER> m_params;
  std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> m_ranges;
  std::vector<D3D12_STATIC_SAMPLER_DESC> m_static_samplers;
};

static HRESULT _SerializeRootSig(const D3D12_ROOT_SIGNATURE_DESC *desc,
                                 ID3DBlob **ppBlob) {
  if (!desc || !ppBlob)
    return E_INVALIDARG;
  *ppBlob = nullptr;

  std::vector<uint8_t> buf;
  size_t total = sizeof(_RSHeader) + desc->NumParameters * sizeof(_RSParameter);
  for (UINT i = 0; i < desc->NumParameters; i++) {
    if (desc->pParameters[i].ParameterType ==
        D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
      total += desc->pParameters[i].DescriptorTable.NumDescriptorRanges *
               sizeof(_RSDescriptorRange);
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
    if (f) {
      fprintf(f, "=== D3D12CreateDevice CALLED FL=%d adapter=%p riid=%s ===\n",
              MinimumFeatureLevel, pAdapter, str::format(riid).c_str());
      fclose(f);
    }
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
    void *device_mem =
        VirtualAlloc((void *)0x500000000ULL, sizeof(MTLD3D12DXGIDevice),
                     MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!device_mem) {
      device_mem =
          VirtualAlloc((void *)0x200000000ULL, sizeof(MTLD3D12DXGIDevice),
                       MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
    if (!device_mem) {
      device_mem = VirtualAlloc(nullptr, sizeof(MTLD3D12DXGIDevice),
                                MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
    {
      FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
      if (f) {
        fprintf(f, "Device allocated at %p size=%zu\n", device_mem,
                sizeof(MTLD3D12DXGIDevice));
        fclose(f);
      }
    }
    auto dxgi_device = new (device_mem) MTLD3D12DXGIDevice(
        CreateDXMTDevice({.device = dxgi_adapter->GetMTLDevice()}),
        dxgi_adapter.ptr());

    HRESULT hr = dxgi_device->QueryInterface(riid, ppDevice);
    if (FAILED(hr)) {
      {
        FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
        if (f) {
          fprintf(f, "D3D12CreateDevice QI FAILED hr=0x%lx FL=%d\n", hr,
                  MinimumFeatureLevel);
          fclose(f);
        }
      }
      dxgi_device->Release();
      return hr;
    }

    Logger::info(str::format("D3D12CreateDevice: created device with FL ",
                             MinimumFeatureLevel));
    {
      FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
      if (f) {
        fprintf(f, "D3D12CreateDevice SUCCESS FL=%d\n", MinimumFeatureLevel);
        fclose(f);
      }
    }
    return S_OK;
  } catch (const std::exception &e) {
    Logger::err(str::format("D3D12CreateDevice: exception: ", e.what()));
    {
      FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
      if (f) {
        fprintf(f, "D3D12CreateDevice EXCEPTION: %s FL=%d\n", e.what(),
                MinimumFeatureLevel);
        fclose(f);
      }
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
    if (f) {
      fprintf(f, "D3D12SerializeRootSignature version=%u params=%u\n", Version,
              pRootSignature ? pRootSignature->NumParameters : 0);
      fclose(f);
    }
  }
  return _SerializeRootSig(pRootSignature, ppBlob);
}

extern "C" HRESULT WINAPI D3D12SerializeVersionedRootSignature(
    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pRootSignature,
    ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob) {
  {
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) {
      fprintf(f, "D3D12SerializeVersionedRootSignature version=%u\n",
              pRootSignature ? pRootSignature->Version : 0);
      fclose(f);
    }
  }
  if (!pRootSignature)
    return E_INVALIDARG;
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
        dst.Constants = src.Constants;
        break;
      case D3D12_ROOT_PARAMETER_TYPE_CBV:
      case D3D12_ROOT_PARAMETER_TYPE_SRV:
      case D3D12_ROOT_PARAMETER_TYPE_UAV:
        dst.Descriptor.ShaderRegister = src.Descriptor.ShaderRegister;
        dst.Descriptor.RegisterSpace = src.Descriptor.RegisterSpace;
        break;
      case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
        dst.DescriptorTable.NumDescriptorRanges =
            src.DescriptorTable.NumDescriptorRanges;
        size_t base = ranges.size();
        for (UINT r = 0; r < src.DescriptorTable.NumDescriptorRanges; r++) {
          auto &rs = src.DescriptorTable.pDescriptorRanges[r];
          D3D12_DESCRIPTOR_RANGE dr = {};
          dr.RangeType = rs.RangeType;
          dr.NumDescriptors = rs.NumDescriptors;
          dr.BaseShaderRegister = rs.BaseShaderRegister;
          dr.RegisterSpace = rs.RegisterSpace;
          dr.OffsetInDescriptorsFromTableStart =
              rs.OffsetInDescriptorsFromTableStart;
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

extern "C" HRESULT WINAPI D3D12CreateRootSignatureDeserializer(
    const void *pData, SIZE_T NumBytes, REFIID riid, void **ppDeserializer) {
  if (!ppDeserializer)
    return E_POINTER;
  *ppDeserializer = nullptr;

  auto *deserializer = new _RSDeserializer(pData, NumBytes);
  if (!deserializer->Valid()) {
    deserializer->Release();
    TraceAgility("D3D12CreateRootSignatureDeserializer bytes=%zu -> E_INVALIDARG",
                 NumBytes);
    return E_INVALIDARG;
  }

  HRESULT hr = deserializer->QueryInterface(riid, ppDeserializer);
  deserializer->Release();
  TraceAgility("D3D12CreateRootSignatureDeserializer bytes=%zu riid=%s -> 0x%lx",
               NumBytes, str::format(riid).c_str(), hr);
  return hr;
}

extern "C" HRESULT WINAPI D3D12CreateVersionedRootSignatureDeserializer(
    const void *pData, SIZE_T NumBytes, REFIID riid, void **ppDeserializer) {
  return D3D12CreateRootSignatureDeserializer(pData, NumBytes, riid,
                                              ppDeserializer);
}

extern "C" HRESULT WINAPI D3D12GetDebugInterface(REFIID riid, void **ppDebug) {
  return E_NOINTERFACE;
}

extern "C" UINT WINAPI D3D12SDKVersion() {
  TraceAgility("D3D12SDKVersion() -> %u", kD3D12AgilitySDKVersion);
  return kD3D12AgilitySDKVersion;
}

extern "C" HRESULT WINAPI D3D12GetInterface(REFCLSID clsid, REFIID riid,
                                            void **ppv) {
  if (!ppv)
    return E_POINTER;
  *ppv = nullptr;
  if (clsid == kCLSID_D3D12SDKConfiguration) {
    auto *configuration = new MTLD3D12SDKConfiguration();
    HRESULT hr = configuration->QueryInterface(riid, ppv);
    configuration->Release();
    TraceAgility("D3D12GetInterface SDKConfiguration riid=%s -> 0x%lx out=%p",
                 str::format(riid).c_str(), hr, ppv ? *ppv : nullptr);
    return hr;
  }

  Logger::warn(str::format("D3D12GetInterface: clsid=", clsid, " riid=", riid,
                           " -> E_NOINTERFACE"));
  TraceAgility("D3D12GetInterface clsid=%s riid=%s -> E_NOINTERFACE",
               str::format(clsid).c_str(), str::format(riid).c_str());
  return E_NOINTERFACE;
}

#ifdef _WIN32
extern void install_crash_handler();
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    DisableThreadLibraryCalls(instance);
    install_crash_handler();
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) {
      char exe[MAX_PATH];
      GetModuleFileNameA(NULL, exe, MAX_PATH);
      fprintf(f, "=== d3d12.dll DllMain PROCESS_ATTACH pid=%lu exe=[%s] ===\n",
              GetCurrentProcessId(), exe);
      fclose(f);
    }
  } else if (reason == DLL_PROCESS_DETACH) {
    FILE *f = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a");
    if (f) {
      char exe[MAX_PATH];
      GetModuleFileNameA(NULL, exe, MAX_PATH);
      fprintf(f, "=== d3d12.dll DllMain PROCESS_DETACH pid=%lu exe=[%s] ===\n",
              GetCurrentProcessId(), exe);
      fclose(f);
    }
  }
  return TRUE;
}
#endif
