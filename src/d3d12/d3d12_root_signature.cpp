#include "d3d12_root_signature.hpp"
#include "d3d12_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include <cstring>

namespace dxmt {

#pragma pack(push, 1)
struct RSHeader {
  uint32_t num_parameters;
  uint32_t num_static_samplers;
  uint32_t flags;
};

struct RSParameter {
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

struct RSDescriptorRange {
  uint8_t range_type;
  uint32_t num_descriptors;
  uint32_t base_register;
  uint32_t register_space;
  uint32_t offset_in_table;
};
#pragma pack(pop)

MTLD3D12RootSignature::MTLD3D12RootSignature(MTLD3D12Device *device,
                                             const void *blob, SIZE_T blob_size)
    : m_device(device) {
  m_device->AddRef();
  Parse(blob, blob_size);
  Logger::info(str::format("D3D12RootSignature: ", m_parameters.size(),
                            " params, ", m_num_static_samplers,
                            " static samplers, flags=", m_flags));
}

MTLD3D12RootSignature::~MTLD3D12RootSignature() { m_device->Release(); }

void MTLD3D12RootSignature::Parse(const void *blob, SIZE_T blob_size) {
  if (blob_size < sizeof(RSHeader))
    return;

  auto header = static_cast<const RSHeader *>(blob);
  m_num_static_samplers = header->num_static_samplers;
  m_flags = static_cast<D3D12_ROOT_SIGNATURE_FLAGS>(header->flags);

  auto params = reinterpret_cast<const uint8_t *>(blob) + sizeof(RSHeader);
  for (uint32_t i = 0; i < header->num_parameters; i++) {
    auto p = reinterpret_cast<const RSParameter *>(params);
    RootParameter rp = {};
    rp.type = static_cast<D3D12_ROOT_PARAMETER_TYPE>(p->type);
    rp.shader_visibility = p->visibility;

    if (p->type == D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS) {
      rp.register_space = p->constants.register_space;
      rp.register_index = p->constants.register_index;
    } else if (p->type == D3D12_ROOT_PARAMETER_TYPE_CBV ||
               p->type == D3D12_ROOT_PARAMETER_TYPE_SRV ||
               p->type == D3D12_ROOT_PARAMETER_TYPE_UAV) {
      rp.register_space = p->descriptor.register_space;
      rp.register_index = p->descriptor.register_index;
    } else if (p->type == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE) {
      auto ranges = reinterpret_cast<const RSDescriptorRange *>(
          params + sizeof(RSParameter));
      rp.descriptor_table_entries = p->table.num_ranges;
      if (p->table.num_ranges > 0) {
        rp.range_type =
            static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(ranges[0].range_type);
        rp.num_descriptors = ranges[0].num_descriptors;
        rp.register_space = ranges[0].register_space;
        rp.register_index = ranges[0].base_register;
      }
      uint32_t append_offset = 0;
      for (uint32_t r = 0; r < p->table.num_ranges; r++) {
        RootDescriptorRange range = {};
        range.range_type =
            static_cast<D3D12_DESCRIPTOR_RANGE_TYPE>(ranges[r].range_type);
        range.num_descriptors = ranges[r].num_descriptors;
        range.base_register = ranges[r].base_register;
        range.register_space = ranges[r].register_space;
        range.offset_in_table =
            ranges[r].offset_in_table == D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND
                ? append_offset
                : ranges[r].offset_in_table;
        rp.ranges.push_back(range);

        if (range.num_descriptors != UINT32_MAX)
          append_offset = range.offset_in_table + range.num_descriptors;
        else
          append_offset = range.offset_in_table;
      }
      params += p->table.num_ranges * sizeof(RSDescriptorRange);
    }
    m_parameters.push_back(rp);
    params += sizeof(RSParameter);
  }
}

bool MTLD3D12RootSignature::FindDescriptorTableRange(
    D3D12_DESCRIPTOR_RANGE_TYPE range_type, uint32_t shader_register,
    uint32_t *root_parameter_index, uint32_t *descriptor_offset) const {
  return FindDescriptorTableRangeForVisibility(
      range_type, shader_register, D3D12_SHADER_VISIBILITY_ALL,
      root_parameter_index, descriptor_offset);
}

bool MTLD3D12RootSignature::FindDescriptorTableRangeForVisibility(
    D3D12_DESCRIPTOR_RANGE_TYPE range_type, uint32_t shader_register,
    D3D12_SHADER_VISIBILITY shader_visibility, uint32_t *root_parameter_index,
    uint32_t *descriptor_offset) const {
  auto search = [&](bool prefer_space_zero) -> bool {
    for (uint32_t visibility_pass = 0; visibility_pass < 2; visibility_pass++) {
      for (uint32_t p = 0; p < m_parameters.size(); p++) {
      const auto &param = m_parameters[p];
      if (param.type != D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        continue;
      if (visibility_pass == 0 &&
          param.shader_visibility != shader_visibility)
        continue;
      if (visibility_pass == 1 &&
          param.shader_visibility != D3D12_SHADER_VISIBILITY_ALL)
        continue;

      for (const auto &range : param.ranges) {
        if (range.range_type != range_type)
          continue;
        if (prefer_space_zero && range.register_space != 0)
          continue;
        if (shader_register < range.base_register)
          continue;
        uint32_t relative = shader_register - range.base_register;
        if (range.num_descriptors != UINT32_MAX &&
            relative >= range.num_descriptors)
          continue;

        if (root_parameter_index)
          *root_parameter_index = p;
        if (descriptor_offset)
          *descriptor_offset = range.offset_in_table + relative;
        return true;
      }
    }
    }
    return false;
  };

  return search(true) || search(false);
}

HRESULT STDMETHODCALLTYPE
MTLD3D12RootSignature::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12RootSignature) {
    *ppvObject = ref(this);
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE
MTLD3D12RootSignature::AddRef() { return ++m_refCount; }

ULONG STDMETHODCALLTYPE MTLD3D12RootSignature::Release() {
  uint32_t rc = --m_refCount;
  if (!rc)
    delete this;
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12RootSignature::GetPrivateData(REFGUID guid, UINT *data_size,
                                      void *data) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12RootSignature::SetPrivateData(REFGUID guid, UINT data_size,
                                      const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12RootSignature::SetPrivateDataInterface(REFGUID guid,
                                               const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12RootSignature::SetName(LPCWSTR name) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12RootSignature::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}

} // namespace dxmt
