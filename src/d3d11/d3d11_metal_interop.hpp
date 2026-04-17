#pragma once

#include "d3d11_interfaces.hpp"
#include "d3d11_device.hpp"

namespace dxmt {

class MTLD3D11InteropDevice : public IMTLD3D11InteropDevice {
public:
  MTLD3D11InteropDevice(IUnknown *container, MTLD3D11Device *device) : container_(container), device_(device) {}

  HRESULT STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) final {
    return container_->QueryInterface(riid, ppvObject);
  }
  ULONG STDMETHODCALLTYPE
  AddRef() final {
    return container_->AddRef();
  }
  ULONG STDMETHODCALLTYPE
  Release() final {
    return container_->Release();
  }

  HRESULT STDMETHODCALLTYPE ImportMTLTexture2D(
      const D3D11_TEXTURE2D_DESC1 *pDesc, uint64_t mtlTexture, ID3D11Texture2D **ppTexture2D
  ) final;

  HRESULT STDMETHODCALLTYPE GetFenceSharedEvent(ID3D11Fence *pFence, uint64_t *pMtlSharedEvent) final;

private:
  IUnknown *container_;
  MTLD3D11Device *device_;
};

} // namespace dxmt
