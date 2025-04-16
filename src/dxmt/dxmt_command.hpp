#pragma once

#include "Foundation/NSTypes.hpp"
#include "Metal.hpp"
#include "Metal/MTLCommandBuffer.hpp"
#include "Metal/MTLComputeCommandEncoder.hpp"
#include "Metal/MTLDevice.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLRenderPass.hpp"
#include "Metal/MTLRenderPipeline.hpp"
#include "dxmt_context.hpp"
#include "dxmt_format.hpp"
#include "objc_pointer.hpp"

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
  setComputeBuffer(MTL::Buffer *buffer, uint64_t offset, uint8_t index) {
    auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_setbuffer>();
    cmd.type = WMTComputeCommandSetBuffer;
    cmd.buffer = (obj_handle_t)buffer;
    cmd.offset = offset;
    cmd.index = index;
  }

  void
  setComputeTexture(MTL::Texture *texture, uint8_t index) {
    auto &cmd = ctx.encodeComputeCommand<wmtcmd_compute_settexture>();
    cmd.type = WMTComputeCommandSetTexture;
    cmd.texture = (obj_handle_t)texture;
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
      MTL::Buffer *buffer, uint64_t byte_offset, uint64_t elements_uint, const std::array<uint32_t, 4> &value
  ) {
    setComputePipelineState(clear_buffer_uint_pipeline, {32, 1, 1});
    setComputeBuffer(buffer, byte_offset, 0);
    setComputeBytes(value.data(), 16, 1);
    setComputeBytes(&elements_uint, 4, 2);
    dispatchThreads({elements_uint, 1, 1});
  }

  void
  ClearTextureBufferUint(MTL::Texture *texture, const std::array<uint32_t, 4> &value) {
    setComputePipelineState(clear_texture_buffer_uint_pipeline, {32, 1, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture->width(), 1, 1});
  }

  void
  ClearBufferFloat(
      MTL::Buffer *buffer, uint64_t byte_offset, uint64_t elements_uint, const std::array<float, 4> &value
  ) {
    // just reinterpret float as uint
    setComputePipelineState(clear_buffer_uint_pipeline, {32, 1, 1});
    setComputeBuffer(buffer, byte_offset, 0);
    setComputeBytes(value.data(), 16, 1);
    setComputeBytes(&elements_uint, 4, 2);
    dispatchThreads({elements_uint, 1, 1});
  }

  void
  ClearTextureBufferFloat(MTL::Texture *texture, const std::array<float, 4> &value) {
    setComputePipelineState(clear_texture_buffer_float_pipeline, {32, 1, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture->width(), 1, 1});
  }

  void
  ClearTexture2DUint(MTL::Texture *texture, const std::array<uint32_t, 4> &value) {
    setComputePipelineState(clear_texture_2d_uint_pipeline, {8, 4, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture->width(), texture->height(), 1});
  }

  void
  ClearTexture2DFloat(MTL::Texture *texture, const std::array<float, 4> &value) {
    setComputePipelineState(clear_texture_2d_float_pipeline, {8, 4, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture->width(), texture->height()});
  }

  void
  ClearTexture2DArrayUint(MTL::Texture *texture, const std::array<uint32_t, 4> &value) {
    setComputePipelineState(clear_texture_2d_array_uint_pipeline, {8, 4, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture->width(), texture->height(), texture->arrayLength()});
  }

  void
  ClearTexture2DArrayFloat(MTL::Texture *texture, const std::array<float, 4> &value) {
    setComputePipelineState(clear_texture_2d_array_float_pipeline, {8, 4, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture->width(), texture->height(), texture->arrayLength()});
  }

  void
  ClearTexture3DFloat(MTL::Texture *texture, const std::array<float, 4> &value) {
    setComputePipelineState(clear_texture_3d_float_pipeline, {8, 4, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture->width(), texture->height(), texture->depth()});
  }

  void
  ClearTexture3DUint(MTL::Texture *texture, const std::array<uint32_t, 4> &value) {
    setComputePipelineState(clear_texture_3d_uint_pipeline, {8, 4, 1});
    setComputeTexture(texture, 0);
    setComputeBytes(value.data(), 16, 1);
    dispatchThreads({texture->width(), texture->height(), texture->depth()});
  }

  void
  PresentToDrawable(MTL::CommandBuffer *cmdbuf, MTL::Texture *backbuffer, MTL::Texture *drawable) {
    auto descriptor = transfer(MTL::RenderPassDescriptor::alloc()->init());
    auto attachment = descriptor->colorAttachments()->object(0);
    attachment->setLoadAction(MTL::LoadActionDontCare);
    attachment->setStoreAction(MTL::StoreActionStore);
    attachment->setTexture(drawable);
    auto encoder = cmdbuf->renderCommandEncoder(descriptor);

    encoder->setFragmentTexture(backbuffer, 0);
    uint32_t extend[4] = {(uint32_t)drawable->width(), (uint32_t)drawable->height(), 0, 0};
    if (backbuffer->pixelFormat() != (MTL::PixelFormat)Forget_sRGB((WMTPixelFormat)backbuffer->pixelFormat()))
      extend[2] |= DXMT_PRESENT_FLAG_SRGB;
    encoder->setFragmentBytes(extend, sizeof(extend), 0);
    if (backbuffer->width() == extend[0] && backbuffer->height() == extend[1])
      encoder->setRenderPipelineState((MTL::RenderPipelineState *)present_swapchain_blit.handle);
    else
      encoder->setRenderPipelineState((MTL::RenderPipelineState *)present_swapchain_scale.handle);
    encoder->setViewport({0, 0, (float)extend[0], (float)extend[1], 0, 1});
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, (NS::UInteger)0u, 3u);

    encoder->endEncoding();
  }

  void
  MarshalGSDispatchArguments(MTL::RenderCommandEncoder *encoder, MTL::Buffer *commands, uint32_t commands_offset) {
    encoder->setRenderPipelineState((MTL::RenderPipelineState *)gs_draw_arguments_marshal.handle);
    encoder->setVertexBuffer(commands, commands_offset, 0);
    encoder->drawPrimitives(MTL::PrimitiveTypePoint, NS::UInteger(0), NS::UInteger(1));
    encoder->setVertexBuffer(nullptr, 0, 0);
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