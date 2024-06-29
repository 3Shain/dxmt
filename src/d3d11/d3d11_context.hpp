
#pragma once

#include "com/com_guid.hpp"
#include "com/com_object.hpp"
#include "d3d11_device.hpp"

#include "Metal/MTLCommandBuffer.hpp"

DEFINE_COM_INTERFACE("3a3f085a-d0fe-4324-b0ae-fe04de18571c",
                     IMTLD3D11DeviceContext)
    : public ID3D11DeviceContext2 {

  virtual void STDMETHODCALLTYPE FlushInternal(
      std::function<void(MTL::CommandBuffer *)> && beforeCommit,
      bool is_present_boundary) = 0;

  virtual void WaitUntilGPUIdle() = 0;
};

DEFINE_COM_INTERFACE("00214122-71ad-4ffc-9990-2e5a936e4652",
                     DynamicBindAsIndexBuffer)
    : dxmt::ComObject<IUnknown>{};

DEFINE_COM_INTERFACE("a9485107-6901-4280-b44d-f292ce8823f9",
                     DynamicBindAsVertexBuffer)
    : dxmt::ComObject<IUnknown> {
  UINT Slot;
};

DEFINE_COM_INTERFACE("5499f7ad-4db7-4ef0-b729-969e6bfcffde",
                     DynamicBindAsConstantBuffer)
    : dxmt::ComObject<IUnknown> {
  UINT ShaderStage;
  UINT Slot;
};

DEFINE_COM_INTERFACE("52bd05b7-af8f-4979-8525-81cebb65520b",
                     DynamicBindAsShaderResource)
    : dxmt::ComObject<IUnknown> {
  UINT ShaderStage;
  UINT Slot;
};

namespace dxmt {

Com<IMTLD3D11DeviceContext> CreateD3D11DeviceContext(IMTLD3D11Device *device);

} // namespace dxmt