#pragma once

#include "com/com_private_data.hpp"
#include "com/com_object.hpp"

namespace dxmt {

template <typename Base> class MTLDXGIObject : public ComObjectWithInitialRef<Base> {

public:
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID Name, UINT *pDataSize,
                                           void *pData) final {
    return private_data_.getData(Name, pDataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID Name, UINT DataSize,
                                           const void *pData) final {
    return private_data_.setData(Name, DataSize, pData);
  }

  HRESULT STDMETHODCALLTYPE
  SetPrivateDataInterface(REFGUID Name, const IUnknown *pUnknown) final {
    return private_data_.setInterface(Name, pUnknown);
  }

private:
  ComPrivateData private_data_;
};

template <typename Base, typename Device = IUnknown> class MTLDXGISubObject : public MTLDXGIObject<Base> {
public:
  MTLDXGISubObject(Device *device) { device_ = device; }

  HRESULT STDMETHODCALLTYPE GetDevice(REFIID riid, void **device) final {
    return device_->QueryInterface(riid, device);
  }

protected:
  Device* device_;
};

} // namespace dxmt
