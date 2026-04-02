#pragma once
#include "d3d11_3.h"
#include "dxmt_buffer.hpp"
#include "dxmt_texture.hpp"

struct MTL_RENDER_PASS_ATTACHMENT_DESC {
  uint32_t RenderTargetArrayLength;
  uint32_t SampleCount;
  uint32_t DepthPlane;
  uint32_t Width;
  uint32_t Height;
};

namespace dxmt {

struct D3D11ShaderResourceView : ID3D11ShaderResourceView1 {
  Buffer *buffer_{};
  BufferSlice slice_{};
  Texture *texture_{};
  unsigned view_id_{};

  Rc<Buffer>
  buffer() const {
    return buffer_;
  };
  BufferSlice
  bufferSlice() const {
    return slice_;
  };
  Rc<Texture>
  texture() const {
    return texture_;
  };
  unsigned
  viewId() const {
    return view_id_;
  };
};

struct D3D11UnorderedAccessView : ID3D11UnorderedAccessView1 {
  Buffer *buffer_{};
  BufferSlice slice_{};
  Texture *texture_{};
  unsigned view_id_{};
  Rc<Buffer> counter_;

  Rc<Buffer>
  buffer() const {
    return buffer_;
  };
  BufferSlice
  bufferSlice() const {
    return slice_;
  };
  Rc<Texture>
  texture() const {
    return texture_;
  };
  unsigned
  viewId() const {
    return view_id_;
  };
  Rc<Buffer>
  counter() const {
    return counter_;
  };
};

struct D3D11RenderTargetView : ID3D11RenderTargetView1 {
  virtual WMTPixelFormat pixelFormat() = 0;
  virtual MTL_RENDER_PASS_ATTACHMENT_DESC &description() = 0;
  virtual Rc<Texture> texture() = 0;
  virtual unsigned viewId() = 0;
};

struct D3D11DepthStencilView : ID3D11DepthStencilView {
  virtual WMTPixelFormat pixelFormat() = 0;
  virtual MTL_RENDER_PASS_ATTACHMENT_DESC &description() = 0;
  virtual UINT readonlyFlags() = 0;
  virtual Rc<Texture> texture() = 0;
  virtual Rc<RenamableTexturePool> renamable() = 0;
  virtual unsigned viewId() = 0;
};

}
