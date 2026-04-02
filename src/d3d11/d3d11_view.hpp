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
  Texture *texture_{};
  unsigned view_id_{};
  MTL_RENDER_PASS_ATTACHMENT_DESC pass_desc_;
  WMTPixelFormat format_{};

  WMTPixelFormat
  pixelFormat() const {
    return format_;
  };
  const MTL_RENDER_PASS_ATTACHMENT_DESC &
  description() const {
    return pass_desc_;
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

struct D3D11DepthStencilView : ID3D11DepthStencilView {
  Texture *texture_{};
  unsigned view_id_{};
  MTL_RENDER_PASS_ATTACHMENT_DESC pass_desc_;
  WMTPixelFormat format_{};
  uint32_t readonly_flags_{};
  RenamableTexturePool *renamable_{};

  WMTPixelFormat
  pixelFormat() const {
    return format_;
  };
  const MTL_RENDER_PASS_ATTACHMENT_DESC &
  description() const {
    return pass_desc_;
  };
  Rc<Texture>
  texture() const {
    return texture_;
  };
  unsigned
  viewId() const {
    return view_id_;
  };
  UINT
  readonlyFlags() {
    return readonly_flags_;
  }
  Rc<RenamableTexturePool>
  renamable() {
    return renamable_;
  }
};
}
