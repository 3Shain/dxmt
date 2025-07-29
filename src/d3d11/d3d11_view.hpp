#pragma once
#include "d3d11_1.h"
#include "dxmt_buffer.hpp"
#include "dxmt_resource_binding.hpp"
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
  virtual Rc<Buffer> buffer() = 0;
  virtual BufferSlice bufferSlice() = 0;
  virtual Rc<Texture> texture() = 0;
  virtual unsigned viewId() = 0;
};

struct D3D11UnorderedAccessView : ID3D11UnorderedAccessView1 {
  virtual Rc<Buffer> buffer() = 0;
  virtual BufferSlice bufferSlice() = 0;
  virtual Rc<Texture> texture() = 0;
  virtual unsigned viewId() = 0;
  virtual Rc<Buffer> counter() = 0;
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
