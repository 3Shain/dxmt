#pragma once

#include "dxgi.h"
#include "dxgi_interfaces.h"

constexpr uint32_t MTLDXGI_MAX_GAMMA_CAPACITY = 1025;

struct MTLDXGI_GAMMA_DATA {
    DXGI_RGB gamma_curve[MTLDXGI_MAX_GAMMA_CAPACITY];
    DXGI_RGB original_gamma_curve[MTLDXGI_MAX_GAMMA_CAPACITY];
    bool original_gamma_curve_valid, gamma_curve_valid;
    UINT mum_curve_points;
    UINT display_id;
};

namespace dxmt {

class MTLDXGIOutput;

class MTLDXGIMonitor : public IMTLDXGIMonitor {
public:
    MTLDXGIMonitor(MTLDXGIOutput *pDXGIOutput);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

    HRESULT STDMETHODCALLTYPE GetDisplayId(HWND hWnd, UINT *pDisplayId);

    HRESULT STDMETHODCALLTYPE AttachGammaTable(MTLDXGI_GAMMA_DATA *pGammaData);
    HRESULT STDMETHODCALLTYPE GetAttachedGammaTable(MTLDXGI_GAMMA_DATA **pGammaData);

    HRESULT STDMETHODCALLTYPE SetGammaCurve(UINT DisplayId, DXGI_RGB *pGammaCurve, UINT NumPoints);
    HRESULT STDMETHODCALLTYPE GetGammaCurve(UINT DisplayId, DXGI_RGB *pGammaCurve, UINT *NumPoints);
    VOID STDMETHODCALLTYPE GetNumGammaCurvePoints(UINT DisplayId, UINT *NumPoints);

private:
    Com<MTLDXGIOutput> dxgi_output_;
    MTLDXGI_GAMMA_DATA *gamma_data_{};
};

} // namespace dxmt
