#pragma once

#include "com_include.h"

namespace dxmt {
/*
\see https://learn.microsoft.com/en-us/windows/win32/com/aggregation
*/
template <typename Outer, typename Inner> class ComAggregatedObject : public Inner {

public:
  ComAggregatedObject(Outer *outer) noexcept : outer_(outer) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    return outer_->QueryInterface(riid, ppvObject);
  };

  ULONG STDMETHODCALLTYPE AddRef() final { return outer_->AddRef(); };

  ULONG STDMETHODCALLTYPE Release() final { return outer_->Release(); };

protected:
  Outer *outer_;
};

} // namespace dxmt