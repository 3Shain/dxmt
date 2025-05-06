#pragma once

#include "Metal.hpp"
#include "dxmt_context.hpp"
#include "dxmt_format.hpp"

namespace dxmt {

enum DXMT_PRESENT_FLAG { DXMT_PRESENT_FLAG_SRGB = 1 << 0 };

class EmulatedCommandContext {
public:
  EmulatedCommandContext(WMT::Device device, ArgumentEncodingContext &ctx);

  void
  setComputePipelineState(WMT::ComputePipelineState state, const WMTSize &threadgroup_size) {
    auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_setpso>();
    cmd.type = WMTComputeCommandSetPSO;
    cmd.pso = state;
    cmd.threadgroup_size = threadgroup_size;
  };

  void
  dispatchThreads(const WMTSize &grid_size) {
    auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_dispatch>();
    cmd.type = WMTComputeCommandDispatchThreads;
    cmd.size = grid_size;
  };

  void
  setComputeBuffer(WMT::Buffer buffer, uint64_t offset, uint8_t index) {
    auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_setbuffer>();
    cmd.type = WMTComputeCommandSetBuffer;
    cmd.buffer = buffer;
    cmd.offset = offset;
    cmd.index = index;
  }

  void
  setComputeTexture(WMT::Texture texture, uint8_t index) {
    auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_settexture>();
    cmd.type = WMTComputeCommandSetTexture;
    cmd.texture = texture;
    cmd.index = index;
  }

  void
  setComputeBytes(const void *buf, uint64_t length, uint8_t index) {
    auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_setbytes>();
    cmd.type = WMTComputeCommandSetBytes;
    void * temp = ctx.allocate_cpu_heap(length, 8);
    memcpy(temp, buf, length);
    cmd.bytes.set(temp);
    cmd.length = length;
    cmd.index = index;
  }

  void
  ClearBufferUint(
      WMT::Buffer buffer, uint64_t byte_offset, uint64_t elements_uint, const std::array<uint32_t, 4> &value
  ) {
    setComputePipelineState(clear_buffer_uint_pipeline, {32, 1, 1});
    setComputeBuffer(buffer, byte_offset, 0);
    setComputeBytes(value.data(), 16, 1);
    setComputeBytes(&elements_uint, 4, 2);
    dispatchThreads({elements_uint, 1, 1});
  }

  void
  ClearTextureBufferUint(WMT::Texture texture, const std::array<uint32_t, 4> &value) {
    setComputePipelineState(clear_texture_buffer_uint_pipeline, {32, 1, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture.width(), 1, 1});
  }

  void
  ClearBufferFloat(
      WMT::Buffer buffer, uint64_t byte_offset, uint64_t elements_uint, const std::array<float, 4> &value
  ) {
    // just reinterpret float as uint
    setComputePipelineState(clear_buffer_uint_pipeline, {32, 1, 1});
    setComputeBuffer(buffer, byte_offset, 0);
    setComputeBytes(value.data(), 16, 1);
    setComputeBytes(&elements_uint, 4, 2);
    dispatchThreads({elements_uint, 1, 1});
  }

  void
  ClearTextureBufferFloat(WMT::Texture texture, const std::array<float, 4> &value) {
    setComputePipelineState(clear_texture_buffer_float_pipeline, {32, 1, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture.width(), 1, 1});
  }

  void
  ClearTexture2DUint(WMT::Texture texture, const std::array<uint32_t, 4> &value) {
    setComputePipelineState(clear_texture_2d_uint_pipeline, {8, 4, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture.width(), texture.height(), 1});
  }

  void
  ClearTexture2DFloat(WMT::Texture texture, const std::array<float, 4> &value) {
    setComputePipelineState(clear_texture_2d_float_pipeline, {8, 4, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture.width(), texture.height(), 1});
  }

  void
  ClearTexture2DArrayUint(WMT::Texture texture, const std::array<uint32_t, 4> &value) {
    setComputePipelineState(clear_texture_2d_array_uint_pipeline, {8, 4, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture.width(), texture.height(), texture.arrayLength()});
  }

  void
  ClearTexture2DArrayFloat(WMT::Texture texture, const std::array<float, 4> &value) {
    setComputePipelineState(clear_texture_2d_array_float_pipeline, {8, 4, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture.width(), texture.height(), texture.arrayLength()});
  }

  void
  ClearTexture3DFloat(WMT::Texture texture, const std::array<float, 4> &value) {
    setComputePipelineState(clear_texture_3d_float_pipeline, {8, 4, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture.width(), texture.height(), texture.depth()});
  }

  void
  ClearTexture3DUint(WMT::Texture texture, const std::array<uint32_t, 4> &value) {
    setComputePipelineState(clear_texture_3d_uint_pipeline, {8, 4, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture.width(), texture.height(), texture.depth()});
  }

  void
  PresentToDrawable(WMT::CommandBuffer cmdbuf, WMT::Texture backbuffer, WMT::Texture drawable) {
    WMTRenderPassInfo info;
    WMT::InitializeRenderPassInfo(info);
    info.colors[0].load_action = WMTLoadActionDontCare;
    info.colors[0].store_action = WMTStoreActionStore;
    info.colors[0].texture = drawable;
    auto encoder = cmdbuf.renderCommandEncoder(info);

    encoder.setFragmentTexture(backbuffer, 0);
    uint32_t extend[4] = {(uint32_t)drawable.width(), (uint32_t)drawable.height(), 0, 0};
    if (backbuffer.pixelFormat() != Forget_sRGB(backbuffer.pixelFormat()))
      extend[2] |= DXMT_PRESENT_FLAG_SRGB;
    encoder.setFragmentBytes(extend, sizeof(extend), 0);
    if (backbuffer.width() == extend[0] && backbuffer.height() == extend[1])
      encoder.setRenderPipelineState(present_swapchain_blit);
    else
      encoder.setRenderPipelineState(present_swapchain_scale);
    encoder.setViewport({0, 0, (double)extend[0], (double)extend[1], 0, 1});
    encoder.drawPrimitives(WMTPrimitiveTypeTriangle, 0, 3);

    encoder.endEncoding();
  }

  void
  MarshalGSDispatchArguments(WMT::RenderCommandEncoder encoder, WMT::Buffer commands, uint32_t commands_offset) {
    encoder.setRenderPipelineState(gs_draw_arguments_marshal);
    encoder.setVertexBuffer(commands, commands_offset, 0);
    encoder.drawPrimitives(WMTPrimitiveTypePoint, 0, 1);
    encoder.setVertexBuffer({}, 0, 0);
    encoder.setVertexBuffer({}, 0, 1);
  }

private:
  ArgumentEncodingContext &ctx;
  WMT::Reference<WMT::ComputePipelineState> clear_texture_1d_uint_pipeline;
  WMT::Reference<WMT::ComputePipelineState> clear_texture_1d_array_uint_pipeline;
  WMT::Reference<WMT::ComputePipelineState> clear_texture_2d_uint_pipeline;
  WMT::Reference<WMT::ComputePipelineState> clear_texture_2d_array_uint_pipeline;
  WMT::Reference<WMT::ComputePipelineState> clear_texture_3d_uint_pipeline;
  WMT::Reference<WMT::ComputePipelineState> clear_texture_buffer_uint_pipeline;
  WMT::Reference<WMT::ComputePipelineState> clear_buffer_uint_pipeline;
  WMT::Reference<WMT::ComputePipelineState> clear_texture_1d_float_pipeline;
  WMT::Reference<WMT::ComputePipelineState> clear_texture_1d_array_float_pipeline;
  WMT::Reference<WMT::ComputePipelineState> clear_texture_2d_float_pipeline;
  WMT::Reference<WMT::ComputePipelineState> clear_texture_2d_array_float_pipeline;
  WMT::Reference<WMT::ComputePipelineState> clear_texture_3d_float_pipeline;
  WMT::Reference<WMT::ComputePipelineState> clear_texture_buffer_float_pipeline;

  WMT::Reference<WMT::RenderPipelineState> present_swapchain_blit;
  WMT::Reference<WMT::RenderPipelineState> present_swapchain_scale;
  WMT::Reference<WMT::RenderPipelineState> gs_draw_arguments_marshal;
};

} // namespace dxmt