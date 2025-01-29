#pragma once

#include "Foundation/NSTypes.hpp"
#include "Metal/MTLCommandBuffer.hpp"
#include "Metal/MTLComputeCommandEncoder.hpp"
#include "Metal/MTLComputePipeline.hpp"
#include "Metal/MTLDevice.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLRenderPass.hpp"
#include "Metal/MTLRenderPipeline.hpp"
#include "Metal/MTLTypes.hpp"
#include "dxmt_format.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

enum DXMT_PRESENT_FLAG {
  DXMT_PRESENT_FLAG_SRGB = 1 << 0
};

class EmulatedCommandContext {
public:
  EmulatedCommandContext(MTL::Device *device);

  void
  ClearBufferUint(
      MTL::ComputeCommandEncoder *encoder, MTL::Buffer *buffer, uint64_t byte_offset, uint64_t elements_uint,
      const std::array<uint32_t, 4> &value
  ) {
    encoder->setComputePipelineState(clear_buffer_uint_pipeline);
    encoder->setBuffer(buffer, byte_offset, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->setBytes(&elements_uint, 4, 2);
    encoder->dispatchThreads(MTL::Size::Make(elements_uint, 1, 1), MTL::Size::Make(32, 1, 1));
  }

  void
  ClearTextureBufferUint(
      MTL::ComputeCommandEncoder *encoder, MTL::Texture *texture, const std::array<uint32_t, 4> &value
  ) {
    encoder->setComputePipelineState(clear_texture_buffer_uint_pipeline);
    encoder->setTexture(texture, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->dispatchThreads(MTL::Size::Make(texture->width(), 1, 1), MTL::Size::Make(32, 1, 1));
  }

  void
  ClearBufferFloat(
      MTL::ComputeCommandEncoder *encoder, MTL::Buffer *buffer, uint64_t byte_offset, uint64_t elements_uint,
      const std::array<float, 4> &value
  ) {
    // just reinterpret float as uint
    encoder->setComputePipelineState(clear_buffer_uint_pipeline);
    encoder->setBuffer(buffer, byte_offset, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->setBytes(&elements_uint, 4, 2);
    encoder->dispatchThreads(MTL::Size::Make(elements_uint, 1, 1), MTL::Size::Make(32, 1, 1));
  }

  void
  ClearTextureBufferFloat(
      MTL::ComputeCommandEncoder *encoder, MTL::Texture *texture, const std::array<float, 4> &value
  ) {
    encoder->setComputePipelineState(clear_texture_buffer_float_pipeline);
    encoder->setTexture(texture, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->dispatchThreads(MTL::Size::Make(texture->width(), 1, 1), MTL::Size::Make(32, 1, 1));
  }

  void
  ClearTexture2DUint(MTL::ComputeCommandEncoder *encoder, MTL::Texture *texture, const std::array<uint32_t, 4> &value) {
    encoder->setComputePipelineState(clear_texture_2d_uint_pipeline);
    encoder->setTexture(texture, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->dispatchThreads(MTL::Size::Make(texture->width(), texture->height(), 1), MTL::Size::Make(8, 4, 1));
  }

  void
  ClearTexture2DFloat(MTL::ComputeCommandEncoder *encoder, MTL::Texture *texture, const std::array<float, 4> &value) {
    encoder->setComputePipelineState(clear_texture_2d_float_pipeline);
    encoder->setTexture(texture, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->dispatchThreads(MTL::Size::Make(texture->width(), texture->height(), 1), MTL::Size::Make(8, 4, 1));
  }

  void
  ClearTexture2DArrayUint(MTL::ComputeCommandEncoder *encoder, MTL::Texture *texture, const std::array<uint32_t, 4> &value) {
    encoder->setComputePipelineState(clear_texture_2d_array_uint_pipeline);
    encoder->setTexture(texture, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->dispatchThreads(MTL::Size::Make(texture->width(), texture->height(), texture->arrayLength()), MTL::Size::Make(8, 4, 1));
  }

  void
  ClearTexture2DArrayFloat(MTL::ComputeCommandEncoder *encoder, MTL::Texture *texture, const std::array<float, 4> &value) {
    encoder->setComputePipelineState(clear_texture_2d_array_float_pipeline);
    encoder->setTexture(texture, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->dispatchThreads(MTL::Size::Make(texture->width(), texture->height(), texture->arrayLength()), MTL::Size::Make(8, 4, 1));
  }

  void
  ClearTexture3DFloat(MTL::ComputeCommandEncoder *encoder, MTL::Texture *texture, const std::array<float, 4> &value) {
    encoder->setComputePipelineState(clear_texture_3d_float_pipeline);
    encoder->setTexture(texture, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->dispatchThreads(
        MTL::Size::Make(texture->width(), texture->height(), texture->depth()), MTL::Size::Make(8, 4, 1)
    );
  }

  void
  ClearTexture3DUint(MTL::ComputeCommandEncoder *encoder, MTL::Texture *texture, const std::array<uint32_t, 4> &value) {
    encoder->setComputePipelineState(clear_texture_3d_uint_pipeline);
    encoder->setTexture(texture, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->dispatchThreads(
        MTL::Size::Make(texture->width(), texture->height(), texture->depth()), MTL::Size::Make(8, 4, 1)
    );
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
    if (backbuffer->pixelFormat() != Forget_sRGB(backbuffer->pixelFormat()))
      extend[2] |= DXMT_PRESENT_FLAG_SRGB;
    encoder->setFragmentBytes(extend, sizeof(extend), 0);
    if (backbuffer->width() == extend[0] && backbuffer->height() == extend[1])
      encoder->setRenderPipelineState(present_swapchain_blit);
    else
      encoder->setRenderPipelineState(present_swapchain_scale);
    encoder->setViewport({0, 0, (float)extend[0], (float)extend[1], 0, 1});
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, (NS::UInteger)0u, 3u);

    encoder->endEncoding();
  }

private:
  Obj<MTL::ComputePipelineState> clear_texture_1d_uint_pipeline;
  Obj<MTL::ComputePipelineState> clear_texture_1d_array_uint_pipeline;
  Obj<MTL::ComputePipelineState> clear_texture_2d_uint_pipeline;
  Obj<MTL::ComputePipelineState> clear_texture_2d_array_uint_pipeline;
  Obj<MTL::ComputePipelineState> clear_texture_3d_uint_pipeline;
  Obj<MTL::ComputePipelineState> clear_texture_buffer_uint_pipeline;
  Obj<MTL::ComputePipelineState> clear_buffer_uint_pipeline;
  Obj<MTL::ComputePipelineState> clear_texture_1d_float_pipeline;
  Obj<MTL::ComputePipelineState> clear_texture_1d_array_float_pipeline;
  Obj<MTL::ComputePipelineState> clear_texture_2d_float_pipeline;
  Obj<MTL::ComputePipelineState> clear_texture_2d_array_float_pipeline;
  Obj<MTL::ComputePipelineState> clear_texture_3d_float_pipeline;
  Obj<MTL::ComputePipelineState> clear_texture_buffer_float_pipeline;

  Obj<MTL::RenderPipelineState> present_swapchain_blit;
  Obj<MTL::RenderPipelineState> present_swapchain_scale;
};

} // namespace dxmt