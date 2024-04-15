#pragma once

#include "dxgi_interfaces.h"
#include "dxgi_object.h"
#include "dxgi_factory.h"
#include "objc_pointer.hpp"

namespace dxmt {

class MTLDXGIAdatper : public MTLDXGIObject<IMTLDXGIAdatper> {
public:
  MTLDXGIAdatper(MTL::Device *device, MTLDXGIFactory *factory);
  ~MTLDXGIAdatper();
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;

  HRESULT STDMETHODCALLTYPE GetParent(REFIID riid, void **ppParent) final;
  HRESULT STDMETHODCALLTYPE GetDesc(DXGI_ADAPTER_DESC *pDesc) final;
  HRESULT STDMETHODCALLTYPE GetDesc1(DXGI_ADAPTER_DESC1 *pDesc) final;
  HRESULT STDMETHODCALLTYPE GetDesc2(DXGI_ADAPTER_DESC2 *pDesc) final;
  HRESULT STDMETHODCALLTYPE EnumOutputs(UINT output_idx,
                                        IDXGIOutput **output) final;
  HRESULT STDMETHODCALLTYPE
  CheckInterfaceSupport(const GUID &guid, LARGE_INTEGER *umd_version) final;

  // custom
  MTL::Device *STDMETHODCALLTYPE GetMTLDevice() final;

private:
  Obj<MTL::Device> m_deivce;
  Com<MTLDXGIFactory> m_factory;
};



} // namespace dxmt