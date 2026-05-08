#pragma once

#include "com/com_pointer.hpp"
#include "d3d12.h"
#include <atomic>
#include <vector>

namespace dxmt {

class MTLD3D12Device;
class MTLD3D12RootSignature;

class MTLD3D12PipelineState : public ID3D12PipelineState {
public:
  MTLD3D12PipelineState(MTLD3D12Device *device, bool is_compute);
  ~MTLD3D12PipelineState();

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
  HRESULT STDMETHODCALLTYPE GetCachedBlob(ID3DBlob **blob) override;

  void SetGraphicsDesc(const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc);
  void SetComputeDesc(const D3D12_COMPUTE_PIPELINE_STATE_DESC &desc);

  bool IsCompute() const { return m_is_compute; }
  ID3D12RootSignature *GetRootSignature() const { return m_root_sig; }

  const void *GetVSBytecode() const { return m_vs.data(); }
  SIZE_T GetVSBytecodeSize() const { return m_vs.size(); }
  const void *GetPSBytecode() const { return m_ps.data(); }
  SIZE_T GetPSBytecodeSize() const { return m_ps.size(); }
  const void *GetCSBytecode() const { return m_cs.data(); }
  SIZE_T GetCSBytecodeSize() const { return m_cs.size(); }

  D3D12_BLEND_DESC GetBlendDesc() const { return m_blend_desc; }
  D3D12_RASTERIZER_DESC GetRasterizerDesc() const { return m_rasterizer_desc; }
  D3D12_DEPTH_STENCIL_DESC GetDepthStencilDesc() const {
    return m_depth_stencil_desc;
  }
  DXGI_FORMAT GetRTVFormat(int i) const {
    return i < 8 ? m_rtv_formats[i] : DXGI_FORMAT_UNKNOWN;
  }
  DXGI_FORMAT GetDSVFormat() const { return m_dsv_format; }
  UINT GetSampleMask() const { return m_sample_mask; }
  UINT GetSampleCount() const { return m_sample_count; }

private:
  MTLD3D12Device *m_device;
  bool m_is_compute;
  ID3D12RootSignature *m_root_sig = nullptr;
  std::vector<uint8_t> m_vs, m_ps, m_gs, m_hs, m_ds, m_cs;
  D3D12_BLEND_DESC m_blend_desc = {};
  D3D12_RASTERIZER_DESC m_rasterizer_desc = {};
  D3D12_DEPTH_STENCIL_DESC m_depth_stencil_desc = {};
  D3D12_INPUT_LAYOUT_DESC m_input_layout = {};
  D3D12_INDEX_BUFFER_STRIP_CUT_VALUE m_strip_cut_value = {};
  D3D12_PRIMITIVE_TOPOLOGY_TYPE m_topology = {};
  UINT m_num_render_targets = 0;
  DXGI_FORMAT m_rtv_formats[8] = {};
  DXGI_FORMAT m_dsv_format = DXGI_FORMAT_UNKNOWN;
  UINT m_sample_mask = UINT_MAX;
  UINT m_sample_count = 1;
  std::atomic<uint32_t> m_refCount = {1ul};
};

} // namespace dxmt
