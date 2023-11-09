#pragma once
#include "../util/rc/util_rc.h"
#include "../util/objc_pointer.h"

#include "Metal/MTLArgumentEncoder.hpp"
#include "Metal/MTLBlitCommandEncoder.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLCommandEncoder.hpp"
#include "Metal/MTLComputeCommandEncoder.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLRenderPass.hpp"
#include "Metal/MTLTexture.hpp"
#include <cstdint>
#include <cstring>

#include "../util/util_string.h"
#include "../util/log/log.h"

namespace dxmt {

using AliasLookup = std::function<void *(void *)>;

class ShaderResourceBinding : public RcObject {
public:
  virtual void Bind(MTL::RenderCommandEncoder *renc, MTL::ArgumentEncoder *enc,
                    uint32_t index) = 0;
  virtual ~ShaderResourceBinding(){};
};

class RenderTargetBinding : public RcObject {
public:
  virtual void Bind(MTL::RenderPassDescriptor *render_pass_desc, uint32_t index,
                    MTL::LoadAction loadAction,
                    MTL::StoreAction storeAction) = 0;
  virtual ~RenderTargetBinding(){};
};

class DepthStencilBinding : public RcObject {
public:
  virtual ~DepthStencilBinding(){};
  virtual void Bind(MTL::RenderPassDescriptor *render_pass_desc) = 0;
};

class VertexBufferBinding : public RcObject {
public:
  virtual ~VertexBufferBinding(){};
  virtual void Bind(uint32_t index, uint32_t offset,
                    MTL::RenderCommandEncoder *encoder) = 0;
  virtual void Bind(uint32_t index, uint32_t offset,
                    MTL::ComputeCommandEncoder *encoder) = 0;
};

/**
TODO: Should expose DrawIndexed ? 
*/
class IndexBufferBinding : public RcObject {
public:
  virtual ~IndexBufferBinding(){};
  virtual MTL::Buffer *Get() = 0;
};

class ConstantBufferBinding : public RcObject {
public:
  virtual void Bind(MTL::RenderCommandEncoder *renc, MTL::ArgumentEncoder *enc,
                    uint32_t index, AliasLookup &lookup) = 0;
  virtual ~ConstantBufferBinding(){};
};

class SimpleLazyTextureShaderResourceBinding : public ShaderResourceBinding {
public:
  SimpleLazyTextureShaderResourceBinding(
      std::function<MTL::Texture *()> &&texture)
      : texture(std::move(texture)) {}
  virtual void Bind(MTL::RenderCommandEncoder *renc, MTL::ArgumentEncoder *enc,
                    uint32_t index) {
    auto w = texture();
    enc->setTexture(w, index);
    renc->useResource(w, MTL::ResourceUsageRead);
  }

private:
  std::function<MTL::Texture *()> texture;
};

class SimpleLazyConstantBufferBinding : public ConstantBufferBinding {
public:
  SimpleLazyConstantBufferBinding(std::function<MTL::Buffer *()> &&buffer,
                                  void *reference)
      : buffer(std::move(buffer)), reference_(reference) {}
  virtual void Bind(MTL::RenderCommandEncoder *renc, MTL::ArgumentEncoder *enc,
                    uint32_t index, AliasLookup &lookup) {
    auto ww = lookup(reference_);
    if (ww) {
      auto w = (MTL::Buffer *)ww;
      enc->setBuffer(w, 0, index);
      renc->useResource(w, MTL::ResourceUsageRead);
    } else {
      auto w = buffer();
      enc->setBuffer(w, 0, index);
      renc->useResource(w, MTL::ResourceUsageRead);
    }
  }

private:
  std::function<MTL::Buffer *()> buffer;
  void *reference_;
};

class SimpleLazyTextureRenderTargetBinding : public RenderTargetBinding {
public:
  SimpleLazyTextureRenderTargetBinding(
      std::function<MTL::Texture *()> &&texture, uint32_t level, uint32_t slice)
      : texture(std::move(texture)), level(level), slice(slice){};

  ~SimpleLazyTextureRenderTargetBinding() { texture.~function(); }

  virtual void Bind(MTL::RenderPassDescriptor *render_pass_desc, uint32_t index,
                    MTL::LoadAction loadAction, MTL::StoreAction storeAction) {
    auto colorAttachment = render_pass_desc->colorAttachments()->object(index);
    colorAttachment->setLoadAction(loadAction);
    colorAttachment->setStoreAction(storeAction);
    colorAttachment->setTexture(texture());
    colorAttachment->setLevel(level);
    colorAttachment->setSlice(slice);
  };

private:
  std::function<MTL::Texture *()> texture;
  uint32_t level;
  uint32_t slice;
};

class SimpleLazyTextureDepthStencilBinding : public DepthStencilBinding {
public:
  SimpleLazyTextureDepthStencilBinding(
      std::function<MTL::Texture *()> &&texture, uint32_t level, uint32_t slice,
      bool depthReadonly, bool stencilReadonly)
      : texture(std::move(texture)), level(level), slice(slice),
        depthReadonly(depthReadonly), stencilReadonly(stencilReadonly){};

  ~SimpleLazyTextureDepthStencilBinding() { texture.~function(); }

  virtual void Bind(MTL::RenderPassDescriptor *render_pass_desc) {
    auto stencil = render_pass_desc->depthAttachment();
    stencil->setLoadAction(MTL::LoadActionLoad);
    stencil->setStoreAction(stencilReadonly ? MTL::StoreActionDontCare
                                            : MTL::StoreActionStore);
    stencil->setTexture(texture());
    stencil->setLevel(level);
    stencil->setSlice(slice);
    auto depthAttachment = render_pass_desc->depthAttachment();
    depthAttachment->setLoadAction(MTL::LoadActionLoad);
    depthAttachment->setStoreAction(depthReadonly ? MTL::StoreActionDontCare
                                                  : MTL::StoreActionStore);
    depthAttachment->setTexture(texture());
    depthAttachment->setLevel(level);
    depthAttachment->setSlice(slice);
  };

private:
  std::function<MTL::Texture *()> texture;
  uint32_t level;
  uint32_t slice;
  bool depthReadonly;
  bool stencilReadonly;
};

class SimpleTextureDepthStencilBinding : public DepthStencilBinding {
public:
  SimpleTextureDepthStencilBinding(MTL::Texture *texture, uint32_t level,
                                   uint32_t slice, bool depthReadonly,
                                   bool stencilReadonly)
      : texture(texture), level(level), slice(slice),
        depthReadonly(depthReadonly), stencilReadonly(stencilReadonly){};

  virtual void Bind(MTL::RenderPassDescriptor *render_pass_desc) {
    auto depthAttachment = render_pass_desc->depthAttachment();
    depthAttachment->setLoadAction(MTL::LoadActionLoad);
    depthAttachment->setStoreAction(depthReadonly ? MTL::StoreActionDontCare
                                                  : MTL::StoreActionStore);
    depthAttachment->setTexture(texture.ptr());
    depthAttachment->setLevel(level);
    depthAttachment->setSlice(slice);
    auto stencilAttachment = render_pass_desc->stencilAttachment();
    stencilAttachment->setLoadAction(MTL::LoadActionLoad);
    stencilAttachment->setStoreAction(stencilReadonly ? MTL::StoreActionDontCare
                                                      : MTL::StoreActionStore);
    stencilAttachment->setTexture(texture.ptr());
    stencilAttachment->setLevel(level);
    stencilAttachment->setSlice(slice);
  };

private:
  Obj<MTL::Texture> texture;
  uint32_t level;
  uint32_t slice;
  bool depthReadonly;
  bool stencilReadonly;
};

class SimpleVertexBufferBinding : public VertexBufferBinding {
public:
  SimpleVertexBufferBinding(MTL::Buffer *buffer) : buffer(buffer) {}
  virtual void Bind(uint32_t index, uint32_t offset,
                    MTL::RenderCommandEncoder *encoder) {
    encoder->setVertexBuffer(buffer.ptr(), offset, index);
  };
  virtual void Bind(uint32_t index, uint32_t offset,
                    MTL::ComputeCommandEncoder *encoder) {
    encoder->setBuffer(buffer.ptr(), offset, index);
  };

private:
  Obj<MTL::Buffer> buffer;
};

// class SimpleConstantBufferBinding : public ConstantBufferBinding {
// public:
//   SimpleConstantBufferBinding(MTL::Buffer *buffer) : buffer(buffer) {}

//   virtual void Bind(MTL::RenderCommandEncoder *renc, MTL::ArgumentEncoder
//   *enc,
//                     uint32_t index,AliasLookup& lookup) {
//     enc->setBuffer(buffer.ptr(), 0, index);
//     renc->useResource(buffer.ptr(), MTL::ResourceUsageRead);
//   }

// private:
//   Obj<MTL::Buffer> buffer;
// };

class SimpleIndexBufferBinding : public IndexBufferBinding {
public:
  SimpleIndexBufferBinding(MTL::Buffer *buffer) : buffer(buffer) {}
  virtual MTL::Buffer *Get() { return buffer.ptr(); }

private:
  Obj<MTL::Buffer> buffer;
};

} // namespace dxmt