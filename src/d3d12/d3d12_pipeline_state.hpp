#pragma once

#include "com/com_pointer.hpp"
#include "d3d12.h"
#include "Metal.hpp"
#include "airconv_public.h"
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace dxmt {

class MTLD3D12Device;

struct CompiledShader {
  sm50_shader_t handle = nullptr;
  MTL_SHADER_REFLECTION reflection = {};
};

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

  bool Compile();

  bool EnsureCompiled() {
    if (!m_compiled) Compile();
    return m_compiled;
  }

  bool IsCompute() const { return m_is_compute; }
  bool IsCompiled() const { return m_compiled; }

  WMT::Reference<WMT::RenderPipelineState> GetRenderPSO() const {
    return m_render_pso;
  }
  WMT::Reference<WMT::ComputePipelineState> GetComputePSO() const {
    return m_compute_pso;
  }
  ID3D12RootSignature *GetRootSignature() const { return m_root_sig; }
  struct WMTSize GetThreadgroupSize() const {
    return {(uint64_t)m_threadgroup_size.width, (uint64_t)m_threadgroup_size.height, (uint64_t)m_threadgroup_size.depth};
  }

  static WMTPixelFormat DXGIToMTLPixelFormat(DXGI_FORMAT format);

private:
  bool CompileShader(const void *bytecode, SIZE_T size, ShaderType type,
                     const char *func_name, WMT::Reference<WMT::Function> &out_func);

  static std::mutex s_shader_mutex;
  static std::unordered_map<size_t, WMT::Reference<WMT::Function>> s_shader_cache;

  MTLD3D12Device *m_device;
  bool m_is_compute;
  bool m_compiled = false;
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

  WMT::Reference<WMT::RenderPipelineState> m_render_pso;
  WMT::Reference<WMT::ComputePipelineState> m_compute_pso;
  struct { uint32_t width = 1, height = 1, depth = 1; } m_threadgroup_size;

  std::atomic<uint32_t> m_refCount = {1ul};
};

} // namespace dxmt
