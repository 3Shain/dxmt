#pragma once
#include "Metal/MTLPixelFormat.hpp"
#include "d3d11_1.h"

MIDL_INTERFACE("f1d21087-fbde-44b3-bc2c-b69be540a0ad")
IMTLD3D11RenderTargetView : public ID3D11RenderTargetView {
  virtual MTL::PixelFormat GetPixelFormat() = 0;
};
__CRT_UUID_DECL(IMTLD3D11RenderTargetView, 0xf1d21087, 0xfbde, 0x44b3, 0xbc,
                0x2c, 0xb6, 0x9b, 0xe5, 0x40, 0xa0, 0xad);

MIDL_INTERFACE("42e48164-8733-422b-8421-4c57229641f9")
IMTLD3D11DepthStencilView : public ID3D11DepthStencilView {
  virtual MTL::PixelFormat GetPixelFormat() = 0;
};
__CRT_UUID_DECL(IMTLD3D11DepthStencilView, 0x42e48164, 0x8733, 0x422b, 0x84,
                0x21, 0x4c, 0x57, 0x22, 0x96, 0x41, 0xf9);

MIDL_INTERFACE("a8f906f1-137a-49a6-b9fa-3f89ef52e3eb")
IMTLD3D11UnorderedAccessView : public ID3D11UnorderedAccessView{

                               };
__CRT_UUID_DECL(IMTLD3D11UnorderedAccessView, 0xa8f906f1, 0x137a, 0x49a6, 0xb9,
                0xfa, 0x3f, 0x89, 0xef, 0x52, 0xe3, 0xeb);

MIDL_INTERFACE("acfa8c6e-699a-4607-b229-a55532dde5fd")
IMTLD3D11ShaderResourceView : public ID3D11ShaderResourceView{

                              };
__CRT_UUID_DECL(IMTLD3D11ShaderResourceView, 0xacfa8c6e, 0x699a, 0x4607, 0xb2,
                0x29, 0xa5, 0x55, 0x32, 0xdd, 0xe5, 0xfd);
