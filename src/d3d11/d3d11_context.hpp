
#pragma once

#include "com/com_guid.hpp"
#include "d3d11_device.hpp"

#include "Metal/MTLCommandBuffer.hpp"

DEFINE_COM_INTERFACE("3a3f085a-d0fe-4324-b0ae-fe04de18571c", IMTLD3D11DeviceContext)
 : public ID3D11DeviceContext1 {

  virtual void STDMETHODCALLTYPE Flush2(
      std::function<void(MTL::CommandBuffer *)> && beforeCommit) = 0;
};

namespace dxmt {

IMTLD3D11DeviceContext *NewD3D11DeviceContext(IMTLD3D11Device *device);

} // namespace dxmt