#include "com/com_guid.hpp"
#include "com/com_pointer.hpp"
#include "dxgi_interfaces.h"
#include "dxgi_monitor.hpp"
#include "log/log.hpp"
#include "util_string.hpp"

namespace dxmt {

extern "C"
UINT
CGGetDisplayGammaRampCapacity(UINT DisplayId);

extern "C"
int
CGSetDisplayGammaRamp(UINT DisplayId,
                      UINT TableSize,
                      float *pRedTable,
                      float *pGreenTable,
                      float *pBlueTable);

extern "C"
int
CGGetDisplayGammaRamp(UINT DisplayId,
                      UINT Capacity,
                      float *pRed_table,
                      float *pGreen_table,
                      float *pBlue_table,
                      UINT *pNumSamples);

extern "C"
int
CGGetDisplayIdWithRect(int X,
                       int Y,
                       int Width,
                       int Height,
                       UINT *pDisplayId);

HRESULT
STDMETHODCALLTYPE
MTLDXGIMonitor::GetDisplayId(HWND hWnd, UINT *pDisplayId) {
  RECT window_rect = {0, 0, 0, 0};

  ::GetWindowRect(hWnd, &window_rect);

  if (!pDisplayId)
    return DXGI_ERROR_INVALID_CALL;

  *pDisplayId = 0;
  if (FAILED(CGGetDisplayIdWithRect((window_rect.left + window_rect.right) / 2,
                             (window_rect.top + window_rect.bottom) / 2,
                             1,
                             1,
                             pDisplayId))) {
  }
  return S_OK;
}

HRESULT
STDMETHODCALLTYPE
MTLDXGIMonitor::AttachGammaTable(MTLDXGI_GAMMA_DATA *pGammaData) {
  if (!pGammaData)
    return DXGI_ERROR_INVALID_CALL;
  gamma_data_ = pGammaData;
  return S_OK;
}

HRESULT
STDMETHODCALLTYPE
MTLDXGIMonitor::GetAttachedGammaTable(MTLDXGI_GAMMA_DATA **pGammaData) {
  if (!pGammaData)
    return DXGI_ERROR_INVALID_CALL;
  *pGammaData = gamma_data_;
  return S_OK;
}

HRESULT
STDMETHODCALLTYPE
MTLDXGIMonitor::SetGammaCurve(UINT DisplayId,
                              DXGI_RGB *pGammaCurve,
                              UINT NumPoints) {
  UINT capacity = CGGetDisplayGammaRampCapacity(DisplayId);
  if (capacity == 0 || capacity > MTLDXGI_MAX_GAMMA_CAPACITY || NumPoints > MTLDXGI_MAX_GAMMA_CAPACITY)
    return DXGI_ERROR_UNSUPPORTED;

  float red_table[MTLDXGI_MAX_GAMMA_CAPACITY];
  float green_table[MTLDXGI_MAX_GAMMA_CAPACITY];
  float blue_table[MTLDXGI_MAX_GAMMA_CAPACITY];
  for (UINT i = 0; i < capacity; i++) {
    red_table[i] = pGammaCurve[i].Red;
    green_table[i] = pGammaCurve[i].Green;
    blue_table[i] = pGammaCurve[i].Blue;
  }

  if (FAILED(CGSetDisplayGammaRamp(DisplayId,
                                   capacity,
                                   red_table,
                                   green_table,
                                   blue_table))) {
    return DXGI_ERROR_UNSUPPORTED;
  }

  return S_OK;
}

HRESULT
STDMETHODCALLTYPE
MTLDXGIMonitor::GetGammaCurve(UINT DisplayId,
                              DXGI_RGB *pGammaCurve,
                              UINT *pNumPoints) {
  UINT capacity = CGGetDisplayGammaRampCapacity(DisplayId);
  if (capacity == 0 || capacity > MTLDXGI_MAX_GAMMA_CAPACITY)
    return DXGI_ERROR_UNSUPPORTED;

  float red_table[MTLDXGI_MAX_GAMMA_CAPACITY];
  float green_table[MTLDXGI_MAX_GAMMA_CAPACITY];
  float blue_table[MTLDXGI_MAX_GAMMA_CAPACITY];
  UINT num_samples;

  if (FAILED(CGGetDisplayGammaRamp(DisplayId,
                                   capacity,
                                   red_table,
                                   green_table,
                                   blue_table,
                                   &num_samples))) {
    return DXGI_ERROR_UNSUPPORTED;
  }
  for (UINT i = 0; i < num_samples; i++) {
    pGammaCurve[i].Red = red_table[i];
    pGammaCurve[i].Green = green_table[i];
    pGammaCurve[i].Blue = blue_table[i];
  }
  *pNumPoints = num_samples;
  return S_OK;
}

VOID
STDMETHODCALLTYPE
MTLDXGIMonitor::GetNumGammaCurvePoints(UINT DisplayId, UINT *pNumPoints) {
  UINT capacity = CGGetDisplayGammaRampCapacity(DisplayId);
  *pNumPoints = capacity;
}

} // namespace dxmt
