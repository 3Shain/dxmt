#include "dxgi_interfaces.h"
#include "dxgi_monitor.hpp"

namespace dxmt {

MTLDXGIMonitor::MTLDXGIMonitor(IUnknown *pParent) : parent_(pParent) {
}

ULONG
STDMETHODCALLTYPE
MTLDXGIMonitor::AddRef() {
  return parent_->AddRef();
}

ULONG
STDMETHODCALLTYPE
MTLDXGIMonitor::Release() {
  return parent_->Release();
}

HRESULT
STDMETHODCALLTYPE
MTLDXGIMonitor::QueryInterface(REFIID riid, void **ppvObject) {
  return parent_->QueryInterface(riid, ppvObject);
}

HRESULT
STDMETHODCALLTYPE
MTLDXGIMonitor::SetMonitorData(const MTLDXGI_MONITOR_DATA *pData) {
  if (!pData)
    return DXGI_ERROR_INVALID_CALL;

  monitorData = *pData;

  return S_OK;
}

HRESULT
STDMETHODCALLTYPE
MTLDXGIMonitor::GetMonitorData(MTLDXGI_MONITOR_DATA **ppData) {
  if (!ppData)
    return DXGI_ERROR_INVALID_CALL;

  *ppData = &monitorData;

  return S_OK;
}

} // namespace dxmt
