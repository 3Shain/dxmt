#pragma once
#include "Metal/MTLPixelFormat.hpp"
#include "com/com_guid.hpp"
#include "d3d11_1.h"
#include "dxmt_buffer.hpp"
#include "dxmt_resource_binding.hpp"
#include "dxmt_texture.hpp"

struct MTL_RENDER_PASS_ATTACHMENT_DESC {
  uint32_t RenderTargetArrayLength;
  uint32_t SampleCount;
  uint32_t Level;
  uint32_t Slice;
  uint32_t DepthPlane;
};

DEFINE_COM_INTERFACE("f1d21087-fbde-44b3-bc2c-b69be540a0ad",
                     IMTLD3D11RenderTargetView)
    : public ID3D11RenderTargetView1 {
  virtual MTL::PixelFormat GetPixelFormat() = 0;
  virtual ULONG64 GetUnderlyingResourceId() = 0;
  virtual dxmt::ResourceSubset GetViewRange() = 0;
  virtual MTL_RENDER_PASS_ATTACHMENT_DESC &GetAttachmentDesc() = 0;

  virtual dxmt::Rc<dxmt::Texture> __texture() = 0;
  virtual unsigned __viewId() = 0;
};

DEFINE_COM_INTERFACE("42e48164-8733-422b-8421-4c57229641f9",
                     IMTLD3D11DepthStencilView)
    : public ID3D11DepthStencilView {
  virtual ULONG64 GetUnderlyingResourceId() = 0;
  virtual dxmt::ResourceSubset GetViewRange() = 0;
  virtual MTL::PixelFormat GetPixelFormat() = 0;
  virtual MTL_RENDER_PASS_ATTACHMENT_DESC &GetAttachmentDesc() = 0;

  virtual dxmt::Rc<dxmt::Texture> __texture() = 0;
  virtual dxmt::Rc<dxmt::RenamableTexturePool> __renamable() = 0;
  virtual unsigned __viewId() = 0;
};

DEFINE_COM_INTERFACE("a8f906f1-137a-49a6-b9fa-3f89ef52e3eb",
                     IMTLD3D11UnorderedAccessView)
    : public ID3D11UnorderedAccessView1 {
  virtual ULONG64 GetUnderlyingResourceId() = 0;
  virtual dxmt::ResourceSubset GetViewRange() = 0;

  virtual dxmt::Rc<dxmt::Buffer> __counter() = 0;
};

DEFINE_COM_INTERFACE("acfa8c6e-699a-4607-b229-a55532dde5fd",
                     IMTLD3D11ShaderResourceView)
    : public ID3D11ShaderResourceView1 {
  virtual ULONG64 GetUnderlyingResourceId() = 0;
  virtual dxmt::ResourceSubset GetViewRange() = 0;
};
