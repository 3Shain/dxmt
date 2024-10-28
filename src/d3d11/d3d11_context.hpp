
#pragma once

#include "com/com_guid.hpp"
#include "d3d11_device.hpp"
#include "d3d11_device_child.hpp"

#include "Metal/MTLCommandBuffer.hpp"

DEFINE_COM_INTERFACE("3a3f085a-d0fe-4324-b0ae-fe04de18571c",
                     IMTLD3D11DeviceContext)
    : public ID3D11DeviceContext4 {

  virtual void STDMETHODCALLTYPE FlushInternal(
      std::function<void(MTL::CommandBuffer *)> && beforeCommit,
      std::function<void(void)> && onFinished, bool present_) = 0;

  virtual void WaitUntilGPUIdle() = 0;
};

namespace dxmt {

using MTLD3D11DeviceContextBase =
    MTLD3D11DeviceChild<IMTLD3D11DeviceContext>;

class CommandQueue;

std::unique_ptr<MTLD3D11DeviceContextBase>
InitializeImmediateContext(MTLD3D11Device *device, CommandQueue& cmd_queue);

Com<MTLD3D11DeviceContextBase>
CreateDeferredContext(MTLD3D11Device *pDevice, UINT ContextFlags);

} // namespace dxmt