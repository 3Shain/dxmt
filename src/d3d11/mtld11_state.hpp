#pragma once

#include "com/com_pointer.hpp"
#include "d3d11_input_layout.hpp"
#include "d3d11_shader.hpp"
#include "d3d11_state_object.hpp"
#include "d3d11_view.hpp"
namespace dxmt {

typedef struct {
  Com<IMTLD3D11ShaderResourceView>
      shader_resource_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
  Com<MTLD3D11SamplerState>
      sampler_states[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
  Com<ID3D11Buffer>
      constant_buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  UINT cbv_first_constant_indices
      [D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  UINT cbv_constant_nums[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  Com<MTLD3D11VertexShader> shader;
} D3D11VertexStageState;

typedef struct {
  Com<IMTLD3D11ShaderResourceView>
      shader_resource_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
  Com<MTLD3D11SamplerState>
      sampler_states[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
  Com<ID3D11Buffer>
      constant_buffer_views[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  UINT cbv_first_constant_indices
      [D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  UINT cbv_constant_nums[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  Com<MTLD3D11PixelShader> shader;
} D3D11PixelStageState;

typedef struct {
  Com<IMTLD3D11ShaderResourceView>
      rv[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
  Com<MTLD3D11SamplerState> ss[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
  Com<ID3D11Buffer>
      ConstantBuffer[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  UINT CBVOffset[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  UINT CBVLength[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  Com<MTLD3D11HullShader> shader;
} D3D11HullStageState;

typedef struct {
  Com<IMTLD3D11ShaderResourceView>
      rv[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
  Com<MTLD3D11SamplerState> SamplerState[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
  Com<ID3D11Buffer>
      ConstantBuffer[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  UINT CBVOffset[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  UINT CBVLength[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  Com<MTLD3D11DomainShader> shader;
} D3D11DomainStageState;

typedef struct {
  Com<ID3D11ShaderResourceView>
      SRV[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
  Com<MTLD3D11SamplerState> SamplerState[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
  Com<ID3D11Buffer> CBV[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  UINT CBVOffset[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  UINT CBVLength[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  Com<MTLD3D11GeometryShader> Shader;
} D3D11GeometryStageState;

typedef struct {
  Com<IMTLD3D11ShaderResourceView>
      shader_resource_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
  Com<MTLD3D11SamplerState>
      sampler_states[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
  Com<IMTLD3D11UnorderedAccessView>
      unordered_access_views[D3D11_1_UAV_SLOT_COUNT];
  UINT uav_initial_count_values[D3D11_1_UAV_SLOT_COUNT];
  Com<ID3D11Buffer>
      constant_buffer_views[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  UINT cbv_first_constant_indices
      [D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  UINT cbv_constant_nums[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
  Com<MTLD3D11ComputeShader> shader;
} D3D11ComputeStageState;

struct D3D11ShaderStageState {};

typedef struct {
  Com<ID3D11Buffer> vertex_buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  UINT offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  D3D11_PRIMITIVE_TOPOLOGY topology;
  Com<IMTLD3D11InputLayout> input_layout;
  Com<ID3D11Buffer> index_buffer;
  // either DXGI_FORMAT_R16_UINT or DXGI_FORMAT_R32_UINT
  DXGI_FORMAT index_buffer_format;
  UINT index_buffer_offset;
} D3D11InputAssemblerStageState;

typedef struct {
  Com<IMTLD3D11RenderTargetView>
      render_target_views[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
  Com<IMTLD3D11DepthStencilView> depth_stencil_view;
  Com<IMTLD3D11UnorderedAccessView>
      unordered_access_views[D3D11_1_UAV_SLOT_COUNT];
  UINT uav_initial_count_values[D3D11_1_UAV_SLOT_COUNT];
  Com<MTLD3D11DepthStencilState> depth_stencil_state;
  Com<MTLD3D11BlendState> blend_state;
  UINT num_of_uav;
  FLOAT blend_factor[4];
  // See https://github.com/gpuweb/gpuweb/issues/267
  // Currently ignored
  UINT sample_mask;
  UINT stencil_ref;
} D3D11OutputMergerStageState;

typedef struct {
  Com<ID3D11Buffer> output_buffers[4];
  UINT output_buffer_offsets[4];
} D3D11StreamOutputStageState;

typedef struct RS {
  D3D11_RECT
  scissor_rects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {
      {}};
  D3D11_VIEWPORT
  viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {{}};
  UINT numScissorRects = 0;
  UINT numViewports = 0;
  Com<MTLD3D11RasterizerState> rasterizer_state = nullptr;

  // void clear() {
  //   rasterizer_state = nullptr;
  //   numScissorRects = 0;
  //   numViewports = 0;
  // }
} D3D11RasterizerStageState;

typedef struct D3D11ContextState {
  D3D11VertexStageState vertex_stage = {};
  D3D11HullStageState hull_stage = {};
  D3D11DomainStageState domain_stage = {};
  D3D11GeometryStageState geometry_stage = {};
  D3D11PixelStageState pixel_stage = {};
  D3D11ComputeStageState compute_stage = {};
  D3D11StreamOutputStageState stream_output = {};
  D3D11InputAssemblerStageState input_assembler = {};
  D3D11OutputMergerStageState output_merger = {};
  D3D11RasterizerStageState rasterizer = {};

  Com<ID3D11Predicate> predicate = nullptr;
  BOOL predicate_value = FALSE;
} D3D11ContextState;

} // namespace dxmt