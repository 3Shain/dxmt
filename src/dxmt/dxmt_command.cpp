#include "dxmt_command.hpp"
#include "dxmt_context.hpp"

extern "C" unsigned char dxmt_command_metallib[];
extern "C" unsigned int dxmt_command_metallib_len;

#define CREATE_PIPELINE(name)                                                                                          \
  auto name##_function = library.newFunction(#name);           \
  name##_pipeline = device.newComputePipelineState(name##_function, error);

namespace dxmt {

InternalCommandLibrary::InternalCommandLibrary(WMT::Device device) {
  WMT::Reference<WMT::Error> error;
  library_ = device.newLibrary(dxmt_command_metallib, dxmt_command_metallib_len, error);

  if (error) {
    ERR("Failed to create internal command library: ", error.description().getUTF8String());
    abort();
  }
};

EmulatedCommandContext::EmulatedCommandContext(WMT::Device device, InternalCommandLibrary &lib, ArgumentEncodingContext &ctx): ctx(ctx) {
  auto pool = WMT::MakeAutoreleasePool();

  auto library = lib.getLibrary();

  WMT::Reference<WMT::Error> error;

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


  auto gs_draw_arguments_marshal_vs = library.newFunction("gs_draw_arguments_marshal");
  {
    WMTRenderPipelineInfo gs_marshal_pipeline;
    WMT::InitializeRenderPipelineInfo(gs_marshal_pipeline);
    gs_marshal_pipeline.vertex_function = gs_draw_arguments_marshal_vs;
    gs_marshal_pipeline.rasterization_enabled = false;
    gs_draw_arguments_marshal = device.newRenderPipelineState(gs_marshal_pipeline, error);
  }
}

void
EmulatedCommandContext::setComputePipelineState(WMT::ComputePipelineState state, const WMTSize &threadgroup_size) {
  auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_setpso>();
  cmd.type = WMTComputeCommandSetPSO;
  cmd.pso = state;
  cmd.threadgroup_size = threadgroup_size;
}

void EmulatedCommandContext::dispatchThreads(const WMTSize &grid_size) {
  auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_dispatch>();
  cmd.type = WMTComputeCommandDispatchThreads;
  cmd.size = grid_size;
}

void
EmulatedCommandContext::setComputeBuffer(WMT::Buffer buffer, uint64_t offset, uint8_t index) {
  auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_setbuffer>();
  cmd.type = WMTComputeCommandSetBuffer;
  cmd.buffer = buffer;
  cmd.offset = offset;
  cmd.index = index;
}

void
EmulatedCommandContext::setComputeTexture(WMT::Texture texture, uint8_t index) {
  auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_settexture>();
  cmd.type = WMTComputeCommandSetTexture;
  cmd.texture = texture;
  cmd.index = index;
}

void
EmulatedCommandContext::setComputeBytes(const void *buf, uint64_t length, uint8_t index) {
  auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_setbytes>();
  cmd.type = WMTComputeCommandSetBytes;
  void *temp = ctx.allocate_cpu_heap(length, 8);
  memcpy(temp, buf, length);
  cmd.bytes.set(temp);
  cmd.length = length;
  cmd.index = index;
}

} // namespace dxmt