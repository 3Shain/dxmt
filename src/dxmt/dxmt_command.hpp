#pragma once

#include "Metal/MTLComputeCommandEncoder.hpp"
#include "Metal/MTLComputePipeline.hpp"
#include "Metal/MTLDevice.hpp"
#include "Metal/MTLTypes.hpp"
#include "objc_pointer.hpp"


namespace dxmt {

class DXMTCommandContext {
public:
  DXMTCommandContext(MTL::Device *device);

  void ClearBufferUint(MTL::ComputeCommandEncoder *encoder, MTL::Buffer *buffer,
                       uint64_t byte_offset, uint64_t elements_uint,
                       const std::array<uint32_t, 4> &value) {
    encoder->setComputePipelineState(clear_buffer_uint_pipeline);
    encoder->setBuffer(buffer, byte_offset, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->setBytes(&elements_uint, 4, 2);
    encoder->dispatchThreads(MTL::Size::Make(elements_uint, 1, 1),
                             MTL::Size::Make(32, 1, 1));
  }

  void ClearTextureBufferUint(MTL::ComputeCommandEncoder *encoder,
                              MTL::Texture *texture,
                              const std::array<uint32_t, 4> &value) {
    encoder->setComputePipelineState(clear_texture_buffer_uint_pipeline);
    encoder->setTexture(texture, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->dispatchThreads(MTL::Size::Make(texture->width(), 1, 1),
                             MTL::Size::Make(32, 1, 1));
  }

  void ClearTexture2DUint(MTL::ComputeCommandEncoder *encoder,
                          MTL::Texture *texture,
                          const std::array<uint32_t, 4> &value) {
    encoder->setComputePipelineState(clear_texture_2d_uint_pipeline);
    encoder->setTexture(texture, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->dispatchThreads(
        MTL::Size::Make(texture->width(), texture->height(), 1),
        MTL::Size::Make(8, 8, 1));
  }


  void ClearTexture3DFloat(MTL::ComputeCommandEncoder *encoder,
                          MTL::Texture *texture,
                          const std::array<float, 4> &value) {
    encoder->setComputePipelineState(clear_texture_3d_float_pipeline);
    encoder->setTexture(texture, 0);
    encoder->setBytes(value.data(), 16, 1);
    encoder->dispatchThreads(
        MTL::Size::Make(texture->width(), texture->height(), texture->depth()),
        MTL::Size::Make(8, 8, 1));
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
};

} // namespace dxmt