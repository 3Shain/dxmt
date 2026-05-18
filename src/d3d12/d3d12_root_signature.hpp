#pragma once

#include "com/com_pointer.hpp"
#include "d3d12.h"
#include "Metal.hpp"
#include <atomic>
#include <vector>

namespace dxmt {

class MTLD3D12Device;

struct RootDescriptorRange {
  D3D12_DESCRIPTOR_RANGE_TYPE range_type;
  uint32_t num_descriptors;
  uint32_t base_register;
  uint32_t register_space;
  uint32_t offset_in_table;
};

struct RootParameter {
  D3D12_ROOT_PARAMETER_TYPE type;
  uint32_t shader_visibility;
  uint32_t register_space;
  uint32_t register_index;
  uint32_t num_descriptors;
  D3D12_DESCRIPTOR_RANGE_TYPE range_type;
  uint32_t descriptor_table_entries;
  std::vector<RootDescriptorRange> ranges;
};

struct RootStaticSampler {
  uint32_t shader_register;
  uint32_t register_space;
  uint32_t shader_visibility;
  uint64_t sampler_gpu_id;
  uint64_t sampler_cube_gpu_id;
  uint64_t lod_bias_bits;
  WMT::Reference<WMT::SamplerState> sampler;
  WMT::Reference<WMT::SamplerState> sampler_cube;
};

class MTLD3D12RootSignature : public ID3D12RootSignature {
public:
  MTLD3D12RootSignature(MTLD3D12Device *device, const void *blob,
                        SIZE_T blob_size);
  ~MTLD3D12RootSignature();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) override;
  ULONG STDMETHODCALLTYPE AddRef() override;
  ULONG STDMETHODCALLTYPE Release() override;

  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *data_size,
                                          void *data) override;
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT data_size,
                                          const void *data) override;
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
      REFGUID guid, const IUnknown *data) override;
  HRESULT STDMETHODCALLTYPE SetName(LPCWSTR name) override;

  HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **device) override;

  const std::vector<RootParameter> &GetParameters() const {
    return m_parameters;
  }
  bool FindDescriptorTableRange(D3D12_DESCRIPTOR_RANGE_TYPE range_type,
                                uint32_t shader_register,
                                uint32_t *root_parameter_index,
                                uint32_t *descriptor_offset) const;
  bool FindDescriptorTableRange(D3D12_DESCRIPTOR_RANGE_TYPE range_type,
                                uint32_t shader_register,
                                uint32_t register_space,
                                uint32_t *root_parameter_index,
                                uint32_t *descriptor_offset) const;
  bool FindDescriptorTableRangeForVisibility(
      D3D12_DESCRIPTOR_RANGE_TYPE range_type, uint32_t shader_register,
      D3D12_SHADER_VISIBILITY shader_visibility,
      uint32_t *root_parameter_index, uint32_t *descriptor_offset) const;
  bool FindDescriptorTableRangeForVisibility(
      D3D12_DESCRIPTOR_RANGE_TYPE range_type, uint32_t shader_register,
      uint32_t register_space, D3D12_SHADER_VISIBILITY shader_visibility,
      uint32_t *root_parameter_index, uint32_t *descriptor_offset) const;
  const RootStaticSampler *FindStaticSampler(
      uint32_t shader_register, uint32_t register_space,
      D3D12_SHADER_VISIBILITY shader_visibility) const;
  uint32_t GetNumParameters() const { return m_parameters.size(); }
  uint32_t GetNumStaticSamplers() const { return m_num_static_samplers; }
  D3D12_ROOT_SIGNATURE_FLAGS GetFlags() const { return m_flags; }

private:
  void Parse(const void *blob, SIZE_T blob_size);

  MTLD3D12Device *m_device;
  std::vector<RootParameter> m_parameters;
  std::vector<RootStaticSampler> m_static_samplers;
  uint32_t m_num_static_samplers = 0;
  D3D12_ROOT_SIGNATURE_FLAGS m_flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
  std::atomic<uint32_t> m_refCount = {1ul};
};

} // namespace dxmt
