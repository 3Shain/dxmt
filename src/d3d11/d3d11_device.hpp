#pragma once

#include "com/com_guid.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_1.h"
#include "Metal/MTLDevice.hpp"
#include "dxgi_interfaces.h"

DEFINE_COM_INTERFACE("a46de9a7-0233-4a94-b75c-9c0f8f364cda", IMTLD3D11Device)
    : public ID3D11Device1 {
  virtual MTL::Device *STDMETHODCALLTYPE GetMTLDevice() = 0;
};

namespace dxmt {
Com<IMTLDXGIDevice> CreateD3D11Device(IMTLDXGIAdatper *pAdapter,
                                      D3D_FEATURE_LEVEL FeatureLevel,
                                      UINT Flags);
} // namespace dxmt