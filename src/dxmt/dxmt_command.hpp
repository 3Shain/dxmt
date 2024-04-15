#pragma once
#include "rc/util_rc.hpp"
#include "rc/util_rc_ptr.hpp"

#include <variant>

#include "Metal/MTLArgumentEncoder.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLSampler.hpp"
#include "dxmt_binding.hpp"

namespace dxmt {

struct MTLBlitCommand {
  using Thunk = std::function<void(MTL::BlitCommandEncoder *encoder)>;
  Thunk exec_thunk;

  MTLBlitCommand(Thunk &&thunk) : exec_thunk(thunk) {}
};

struct MTLRenderCommand {
  using Thunk = std::function<void(MTL::RenderCommandEncoder *encoder)>;
  Thunk exec_thunk;

  MTLRenderCommand(Thunk &&thunk) : exec_thunk(thunk) {}
};

struct MTLComputeCommand {
  using Thunk = std::function<void(MTL::ComputeCommandEncoder *encoder)>;
  Thunk exec_thunk;

  MTLComputeCommand(Thunk &&thunk) : exec_thunk(thunk) {}
};

struct MTLBindRenderTarget {
  using Thunk = std::function<void(MTL::RenderPassDescriptor *desc)>;
  Thunk thunk;
  MTLBindRenderTarget(Thunk &&thunk) : thunk(thunk) {}

  MTLBindRenderTarget()
      : thunk([](auto) {
          // do nothing
        }) {}
};

struct MTLBindPipelineState {
  using Accessor = std::function<MTL::RenderPipelineState *()>;
  Accessor accessor;
  Obj<MTL::ArgumentEncoder> vs_encoder;
  Obj<MTL::ArgumentEncoder> ps_encoder;
  MTLBindPipelineState(Accessor &&thunk, MTL::ArgumentEncoder *vs_encoder,
                       MTL::ArgumentEncoder *ps_encoder)
      : accessor(thunk), vs_encoder(vs_encoder), ps_encoder(ps_encoder) {}
  MTLBindPipelineState()
      : accessor([]() { return (MTL::RenderPipelineState *)NULL; }) {}
};

struct MTLBindDepthStencilState {
  using Thunk = std::function<void(MTL::RenderCommandEncoder *enc)>;
  Thunk exec_thunk;

  MTLBindDepthStencilState(Thunk &&thunk) : exec_thunk(thunk) {}

  MTLBindDepthStencilState()
      : exec_thunk([](auto) {
          // do nothing
        }) {}
};

struct MTLCommandClearRenderTargetView {
  Rc<RenderTargetBinding> rtv;
  float color[4];

  MTLCommandClearRenderTargetView(RenderTargetBinding *rtv,
                                  const float _color[4])
      : rtv(rtv) {
    color[0] = _color[0];
    color[1] = _color[1];
    color[2] = _color[2];
    color[3] = _color[3];
  }
};

struct MTLCommandClearDepthStencilView {
  Rc<DepthStencilBinding> dsv;
  float depth;
  uint8_t stencil;
  bool clearDepth;
  bool clearStencil;

  MTLCommandClearDepthStencilView(DepthStencilBinding *dsv, const float depth,
                                  const uint8_t stencil, bool clearDepth,
                                  bool clearStencil)
      : dsv(dsv), depth(depth), stencil(stencil), clearDepth(clearDepth),
        clearStencil(clearStencil) {
    ;
  }
};

struct MTLSetViewports {
  std::vector<MTL::Viewport> viewports;

  MTLSetViewports(std::vector<MTL::Viewport> &&viewports)
      : viewports(viewports) {}

  MTLSetViewports() : viewports() {}
};
struct MTLSetScissorRects {
  std::vector<MTL::ScissorRect> scissorRects;

  MTLSetScissorRects(std::vector<MTL::ScissorRect> &&scissorRects)
      : scissorRects(scissorRects) {}

  MTLSetScissorRects() : scissorRects() {}
};

struct MTLSetVertexBuffer {
  uint32_t index;
  uint32_t offset;
  Rc<VertexBufferBinding> vertex_buffer_ref_;
  MTLSetVertexBuffer(uint32_t index, uint32_t offset, VertexBufferBinding *v)
      : index(index), offset(offset), vertex_buffer_ref_(v) {}

  MTLSetVertexBuffer() {
    index = 0;
    offset = 0;
    vertex_buffer_ref_ = nullptr;
  };
};

enum ShaderStage { Vertex, Pixel };

template <ShaderStage Stage> class MTLSetSampler {
public:
  uint32_t slot;
  Obj<MTL::SamplerState> sampler;
  MTLSetSampler<Stage>(MTL::SamplerState *sampler, uint32_t slot)
      : slot(slot), sampler(sampler) {}

  MTLSetSampler<Stage>() { sampler = nullptr; }
};

template <ShaderStage Stage> class MTLSetShaderResource {
public:
  uint32_t slot;
  Rc<ShaderResourceBinding> shader_resource_ref_;
  MTLSetShaderResource<Stage>(ShaderResourceBinding *v, uint32_t slot)
      : slot(slot), shader_resource_ref_(v) {}

  MTLSetShaderResource<Stage>() { shader_resource_ref_ = nullptr; };
};

template <ShaderStage Stage> struct MTLSetConstantBuffer {
  uint32_t index;
  uint32_t offset;
  uint32_t length;
  Rc<ConstantBufferBinding> constant_buffer_ref_;
  MTLSetConstantBuffer(uint32_t index, uint32_t offset, uint32_t length,
                       ConstantBufferBinding *v)
      : index(index), offset(offset), length(length), constant_buffer_ref_(v) {}

  MTLSetConstantBuffer() {
    index = 0;
    offset = 0;
    constant_buffer_ref_ = nullptr;
  };
};

struct MTLBufferRotated {

  void *resource_ref;
  Obj<MTL::Buffer> new_buffer;

  MTLBufferRotated(void *resource_ref, MTL::Buffer *newBuffer)
      : resource_ref(resource_ref), new_buffer(newBuffer){

                                    };
};

using MTLCommand =
    std::variant<MTLBlitCommand, MTLRenderCommand, MTLComputeCommand,
                 MTLBindRenderTarget, MTLBindPipelineState,
                 // MTLDefineInputLayout, MTLDefineBlendState
                 MTLBindDepthStencilState, MTLCommandClearRenderTargetView,
                 MTLCommandClearDepthStencilView, MTLSetViewports,
                 MTLSetScissorRects, MTLSetVertexBuffer, MTLSetSampler<Pixel>,
                 MTLSetShaderResource<Pixel>, MTLSetConstantBuffer<Vertex>,
                 MTLSetConstantBuffer<Pixel>, MTLBufferRotated>;

} // namespace dxmt