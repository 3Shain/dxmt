#pragma once

#include "com/com_guid.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_1.h"
#include "Metal/MTLDevice.hpp"
#include "dxgi_interfaces.h"
#include "threadpool.hpp"

DEFINE_COM_INTERFACE("14e1e5e4-3f08-4741-a8e3-597d79373266", IMTLThreadpoolWork)
    : public IUnknown {
  virtual void RunThreadpoolWork() = 0;
};

struct threadpool_trait {
  using work_type = IMTLThreadpoolWork;
  constexpr void invoke_work(work_type *work) { work->RunThreadpoolWork(); };
};

// struct THREADGROUP_WORK_STATE_OPAQUE;
typedef dxmt::threadpool<threadpool_trait>::work_handle THREADGROUP_WORK_STATE;

DEFINE_COM_INTERFACE("a46de9a7-0233-4a94-b75c-9c0f8f364cda", IMTLD3D11Device)
    : public ID3D11Device1 {
  virtual MTL::Device *STDMETHODCALLTYPE GetMTLDevice() = 0;
  virtual void GetAdapter(IMTLDXGIAdatper * *ppAdapter) = 0;
  /**
  TODO: should ensure pWork is not released before executed
  or support cancellation.
  In theory a work can be submitted multiple time,
  if that makes sense (usually not)
  */
  virtual void SubmitThreadgroupWork(IMTLThreadpoolWork * pWork,
                                     THREADGROUP_WORK_STATE * pState) = 0;
  virtual void WaitThreadgroupWork(THREADGROUP_WORK_STATE * pState) = 0;
};

namespace dxmt {
Com<IMTLDXGIDevice> CreateD3D11Device(IMTLDXGIAdatper *pAdapter,
                                      D3D_FEATURE_LEVEL FeatureLevel,
                                      UINT Flags);
} // namespace dxmt