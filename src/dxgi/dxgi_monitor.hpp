#pragma once

#include "dxgi.h"
#include "dxgi_interfaces.h"
#include "dxmt_presenter.hpp"

struct MTLDXGI_MONITOR_DATA {
  dxmt::DXMTGammaCurve gammaCurve;
};

namespace dxmt {

class MTLDXGIMonitor : public IMTLDXGIMonitor {
public:
  MTLDXGIMonitor(IUnknown* pParent);
  ULONG STDMETHODCALLTYPE AddRef();
  ULONG STDMETHODCALLTYPE Release();
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

  HRESULT STDMETHODCALLTYPE SetMonitorData(const MTLDXGI_MONITOR_DATA *pData);
  HRESULT STDMETHODCALLTYPE GetMonitorData(MTLDXGI_MONITOR_DATA **ppData);

private:
  IUnknown *parent_;
  MTLDXGI_MONITOR_DATA monitorData{};
};

} // namespace dxmt
