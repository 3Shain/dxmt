#pragma once

/* header only: use it as an aggregated object */

#include "dxgi1_2.h"
#include "log/log.hpp"

namespace dxmt {

template <typename T>
concept DXGIResourceAggregateContext =
    std::is_base_of< IUnknown, T>::value &&
    requires(T t, REFGUID guid, UINT data_size, const void *data) {
      { t.GetEvictionPriority() } -> std::same_as<UINT>;
      { t.SetPrivateData(guid, data_size, data) } -> std::same_as<HRESULT>;
      // TODO: complete the constraints
    };

/* this is awkward */
struct IDXGIResourceVD: public IDXGIResource1 {
    virtual ~IDXGIResourceVD() {};
};

/* designed to be used as an aggregated object*/
template <DXGIResourceAggregateContext IResource> class MTLDXGIResource : public IDXGIResourceVD {
public:
  MTLDXGIResource(IResource *pResource) : resource_(pResource) {}
  ~MTLDXGIResource() {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    return resource_->QueryInterface(riid, ppvObject);
  }

  ULONG STDMETHODCALLTYPE AddRef() final { return resource_->AddRef(); }

  ULONG STDMETHODCALLTYPE Release() final { return resource_->Release(); }

  HRESULT
  STDMETHODCALLTYPE
  SetPrivateData(REFGUID guid, UINT data_size, const void *data) final {
    return resource_->SetPrivateData(guid, data_size, data);
  }

  HRESULT
  STDMETHODCALLTYPE
  SetPrivateDataInterface(REFGUID guid, const IUnknown *object) final {
    return resource_->SetPrivateDataInterface(guid, object);
  }

  HRESULT
  STDMETHODCALLTYPE
  GetPrivateData(REFGUID guid, UINT *data_size, void *data) final {
    return resource_->GetPrivateData(guid, data_size, data);
  }

  HRESULT
  STDMETHODCALLTYPE
  GetParent(REFIID riid, void **parent) final {
    return GetDevice(riid, parent);
  }

  HRESULT
  STDMETHODCALLTYPE
  GetDevice(REFIID riid, void **ppDevice) final {
    return resource_->GetDeviceInterface(riid, ppDevice);
  }

  HRESULT
  STDMETHODCALLTYPE
  GetSharedHandle(HANDLE *pSharedHandle) final {
    return resource_->GetSharedHandle(pSharedHandle);
  }

  HRESULT
  STDMETHODCALLTYPE
  GetUsage(DXGI_USAGE *pUsage) final {
    return resource_->GetDXGIUsage(pUsage);
  }

  HRESULT
  STDMETHODCALLTYPE
  SetEvictionPriority(UINT EvictionPriority) final {
    resource_->SetEvictionPriority(EvictionPriority);
    return S_OK;
  }

  HRESULT
  STDMETHODCALLTYPE
  GetEvictionPriority(UINT *pEvictionPriority) final {
    *pEvictionPriority = resource_->GetEvictionPriority();
    return S_OK;
  }

  HRESULT
  STDMETHODCALLTYPE
  CreateSubresourceSurface(UINT index, IDXGISurface2 **surface) final {
    ERR_ONCE("DXGIResource::CreateSubresourceSurface: stub");
    return E_NOTIMPL;
  }

  HRESULT
  STDMETHODCALLTYPE
  CreateSharedHandle(const SECURITY_ATTRIBUTES *attributes, DWORD access,
                     const WCHAR *name, HANDLE *handle) final {
    ERR_ONCE("DXGIResource::CCreateSharedHandle: stub");
    return E_NOTIMPL;
  }

private:
  IResource *resource_; // since it's aggregated, no extra reference is needed
};

} // namespace dxmt