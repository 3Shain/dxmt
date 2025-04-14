#include "dxmt_command.hpp"
#include "Metal.hpp"

extern "C" unsigned char dxmt_command_metallib[];
extern "C" unsigned int dxmt_command_metallib_len;

#define CREATE_PIPELINE(name)                                                                                          \
  auto name##_function = library.newFunction(#name);           \
  name##_pipeline = device.newComputePipelineState(name##_function, error);

namespace dxmt {
EmulatedCommandContext::EmulatedCommandContext(WMT::Device device) {
  auto pool = WMT::MakeAutoreleasePool();

  WMT::Reference<WMT::Error> error;
  auto library = device.newLibrary(dxmt_command_metallib, dxmt_command_metallib_len, error);

  CREATE_PIPELINE(clear_texture_1d_uint);
  CREATE_PIPELINE(clear_texture_1d_array_uint);
  CREATE_PIPELINE(clear_texture_2d_uint);
  CREATE_PIPELINE(clear_texture_2d_array_uint);
  CREATE_PIPELINE(clear_texture_3d_uint);
  CREATE_PIPELINE(clear_texture_buffer_uint);
  CREATE_PIPELINE(clear_buffer_uint);
  CREATE_PIPELINE(clear_texture_1d_float);
  CREATE_PIPELINE(clear_texture_1d_array_float);
  CREATE_PIPELINE(clear_texture_2d_float);
  CREATE_PIPELINE(clear_texture_2d_array_float);
  CREATE_PIPELINE(clear_texture_3d_float);
  CREATE_PIPELINE(clear_texture_buffer_float);

  auto vs_present_quad = library.newFunction("vs_present_quad");
  auto fs_present_quad = library.newFunction("fs_present_quad");
  auto fs_present_quad_scaled = library.newFunction("fs_present_quad_scaled");
  {
    WMTRenderPipelineInfo present_pipeline;
    WMT::InitializeRenderPipelineInfo(present_pipeline);
    present_pipeline.vertex_function = vs_present_quad;
    present_pipeline.fragment_function = fs_present_quad;
    present_pipeline.colors[0].pixel_format = WMTPixelFormatBGRA8Unorm;
    present_swapchain_blit = device.newRenderPipelineState(&present_pipeline, error);
    present_pipeline.fragment_function = fs_present_quad_scaled;
    present_swapchain_scale = device.newRenderPipelineState(&present_pipeline, error);
  }

  auto gs_draw_arguments_marshal_vs = library.newFunction("gs_draw_arguments_marshal");
  {
    WMTRenderPipelineInfo gs_marshal_pipeline;
    WMT::InitializeRenderPipelineInfo(gs_marshal_pipeline);
    gs_marshal_pipeline.vertex_function = gs_draw_arguments_marshal_vs;
    gs_marshal_pipeline.rasterization_enabled = false;
    gs_draw_arguments_marshal = device.newRenderPipelineState(&gs_marshal_pipeline, error);
  }
}
} // namespace dxmt