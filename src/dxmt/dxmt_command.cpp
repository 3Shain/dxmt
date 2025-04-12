#include "dxmt_command.hpp"

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
    auto present_pipeline = MTL::RenderPipelineDescriptor::alloc()->init();
    present_pipeline->setVertexFunction((MTL::Function *)vs_present_quad.handle);
    present_pipeline->setFragmentFunction((MTL::Function *)fs_present_quad.handle);
    auto attachment = present_pipeline->colorAttachments()->object(0);
    attachment->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    present_swapchain_blit = transfer(((MTL::Device *)device.handle)->newRenderPipelineState(present_pipeline, nullptr));
    present_pipeline->setFragmentFunction((MTL::Function *)fs_present_quad_scaled.handle);
    present_swapchain_scale = transfer(((MTL::Device *)device.handle)->newRenderPipelineState(present_pipeline, nullptr));
  }

  auto gs_draw_arguments_marshal_vs = library.newFunction("gs_draw_arguments_marshal");
  {
    auto gs_marshal_pipeline = MTL::RenderPipelineDescriptor::alloc()->init();
    gs_marshal_pipeline->setVertexFunction((MTL::Function *)gs_draw_arguments_marshal_vs.handle);
    gs_marshal_pipeline->setRasterizationEnabled(false);
    gs_draw_arguments_marshal = transfer(((MTL::Device *)device.handle)->newRenderPipelineState(gs_marshal_pipeline, nullptr));
  }
}
} // namespace dxmt