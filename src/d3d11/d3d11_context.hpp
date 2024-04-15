
#pragma once

#include "d3d11_device.hpp"

#include "Metal/MTLCommandBuffer.hpp"

MIDL_INTERFACE("3a3f085a-d0fe-4324-b0ae-fe04de18571c")
IMTLD3D11DeviceContext : public ID3D11DeviceContext1 {

  virtual void STDMETHODCALLTYPE Flush2(
      std::function<void(MTL::CommandBuffer *)> && beforeCommit) = 0;
};
__CRT_UUID_DECL(IMTLD3D11DeviceContext, 0x3a3f085a, 0xd0fe, 0x4324, 0xb0, 0xae,
                0xfe, 0x04, 0xde, 0x18, 0x57, 0x1c);

namespace dxmt {

IMTLD3D11DeviceContext *NewD3D11DeviceContext(IMTLD3D11Device *device);

} // namespace dxmt