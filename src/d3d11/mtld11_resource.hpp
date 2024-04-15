#pragma once
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_1.h"

typedef struct MappedResource {
  void *pData;
  uint32_t RowPitch;
  uint32_t DepthPitch;
} MappedResource;

MIDL_INTERFACE("d8a49d20-9a1f-4bb8-9ee6-442e064dce23")
IDXMTResource : public IUnknown {
  virtual HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
      const D3D11_SHADER_RESOURCE_VIEW_DESC *desc,
      ID3D11ShaderResourceView **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
      const D3D11_UNORDERED_ACCESS_VIEW_DESC *desc,
      ID3D11UnorderedAccessView **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
      const D3D11_RENDER_TARGET_VIEW_DESC *desc,
      ID3D11RenderTargetView **ppView) = 0;
  virtual HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
      const D3D11_DEPTH_STENCIL_VIEW_DESC *desc,
      ID3D11DepthStencilView **ppView) = 0;
};
__CRT_UUID_DECL(IDXMTResource, 0xd8a49d20, 0x9a1f, 0x4bb8, 0x9e, 0xe6, 0x44,
                0x2e, 0x06, 0x4d, 0xce, 0x23);

MIDL_INTERFACE("1c7e7c98-6dd4-42f0-867b-67960806886e")
IMTLBuffer : public IUnknown { virtual MTL::Buffer *Get() = 0; };
__CRT_UUID_DECL(IMTLBuffer, 0x1c7e7c98, 0x6dd4, 0x42f0, 0x86, 0x7b, 0x67, 0x96,
                0x08, 0x06, 0x88, 0x6e);

MIDL_INTERFACE("4fe0ec8e-8be0-4c41-a9a4-11726ceba59c")
IMTLTexture : public IUnknown { virtual MTL::Texture *Get() = 0; };
__CRT_UUID_DECL(IMTLTexture, 0x4fe0ec8e, 0x8be0, 0x4c41, 0xa9, 0xa4, 0x11, 0x72,
                0x6c, 0xeb, 0xa5, 0x9c);

struct IMTLSwappableBuffer : public IUnknown {
  virtual MTL::Buffer *GetCurrent() = 0;
  virtual void Swap() = 0;
};

struct IMTLSwappableTexture : public IUnknown {
  virtual MTL::Texture *GetCurrent() = 0;
  virtual void Swap() = 0;
};

namespace dxmt {

struct tag_buffer {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_BUFFER;
  using COM = ID3D11Buffer;
  using DESC = D3D11_BUFFER_DESC;
  using DESC_S = D3D11_BUFFER_DESC;
};

struct tag_texture_1d {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE1D;
  using COM = ID3D11Texture1D;
  using DESC = D3D11_TEXTURE1D_DESC;
  using DESC_S = D3D11_TEXTURE1D_DESC;
};

struct tag_texture_2d {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE2D;
  using COM = ID3D11Texture2D;
  using DESC = D3D11_TEXTURE2D_DESC;
  using DESC_S = D3D11_TEXTURE2D_DESC;
};

struct tag_texture_3d {
  static const D3D11_RESOURCE_DIMENSION dimension =
      D3D11_RESOURCE_DIMENSION_TEXTURE3D;
  using COM = ID3D11Texture3D;
  using DESC = D3D11_TEXTURE3D_DESC;
  using DESC_S = D3D11_TEXTURE3D_DESC;
};

} // namespace dxmt