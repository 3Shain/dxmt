#pragma once
#include "dxgi_object.h"
#include "dxgi_private.h"
#include "dxgi_factory.h"
#include "dxgi_adapter.h"

namespace dxmt {

class MTLDXGIOutput : public MTLDXGIObject<IDXGIOutput1> {
public:
  MTLDXGIOutput(const Com<MTLDXGIFactory> &factory,
                const Com<MTLDXGIAdatper> &adapter, HMONITOR monitor);
  ~MTLDXGIOutput();
  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) final;

  HRESULT
  STDMETHODCALLTYPE
  GetParent(REFIID riid, void **parent) final;

  HRESULT
  STDMETHODCALLTYPE
  GetDesc(DXGI_OUTPUT_DESC *desc) final;

  HRESULT
  STDMETHODCALLTYPE
  GetDisplayModeList(DXGI_FORMAT format, UINT flags, UINT *mode_count,
                     DXGI_MODE_DESC *desc) final;

  HRESULT
  STDMETHODCALLTYPE
  FindClosestMatchingMode(const DXGI_MODE_DESC *mode,
                          DXGI_MODE_DESC *closest_match,
                          IUnknown *device) final;

  HRESULT
  STDMETHODCALLTYPE
  WaitForVBlank() final;

  HRESULT
  STDMETHODCALLTYPE
  TakeOwnership(IUnknown *device, BOOL exclusive) final;

  void STDMETHODCALLTYPE ReleaseOwnership() final;

  HRESULT
  STDMETHODCALLTYPE
  GetGammaControlCapabilities(
      DXGI_GAMMA_CONTROL_CAPABILITIES *gamma_caps) final;

  HRESULT
  STDMETHODCALLTYPE
  SetGammaControl(const DXGI_GAMMA_CONTROL *gamma_control) final;

  HRESULT
  STDMETHODCALLTYPE
  GetGammaControl(DXGI_GAMMA_CONTROL *gamma_control) final;

  HRESULT
  STDMETHODCALLTYPE
  SetDisplaySurface(IDXGISurface *surface) final;

  HRESULT
  STDMETHODCALLTYPE
  GetDisplaySurfaceData(IDXGISurface *surface) final;

  HRESULT
  STDMETHODCALLTYPE
  GetFrameStatistics(DXGI_FRAME_STATISTICS *stats) final;

  HRESULT
  STDMETHODCALLTYPE
  GetDisplayModeList1(DXGI_FORMAT enum_format, UINT flags, UINT *num_modes,
                      DXGI_MODE_DESC1 *desc) final;

  HRESULT
  STDMETHODCALLTYPE
  FindClosestMatchingMode1(const DXGI_MODE_DESC1 *mode_to_match,
                           DXGI_MODE_DESC1 *closest_match,
                           IUnknown *concerned_device) final;

  HRESULT
  STDMETHODCALLTYPE
  GetDisplaySurfaceData1(IDXGIResource *destination) final;

  HRESULT
  STDMETHODCALLTYPE
  DuplicateOutput(IUnknown *device,
                  IDXGIOutputDuplication **output_duplication) final;

private:
  Com<MTLDXGIAdatper> m_adapter = nullptr;
  HMONITOR m_monitor = nullptr;
};
} // namespace dxmt