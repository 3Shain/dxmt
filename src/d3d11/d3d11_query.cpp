#include "d3d11_query.hpp"

namespace dxmt {

class EventQuery : public MTLD3DQueryBase<IMTLD3DEventQuery> {
  using MTLD3DQueryBase<IMTLD3DEventQuery>::MTLD3DQueryBase;

  virtual UINT STDMETHODCALLTYPE GetDataSize() { return sizeof(BOOL); };
};

class OcculusionQuery : public MTLD3DQueryBase<IMTLD3DOcclusionQuery> {
  using MTLD3DQueryBase<IMTLD3DOcclusionQuery>::MTLD3DQueryBase;

  virtual UINT STDMETHODCALLTYPE GetDataSize() { return sizeof(UINT64); };
};

HRESULT CreateEventQuery(IMTLD3D11Device *pDevice,
                         const D3D11_QUERY_DESC *pDesc, ID3D11Query **ppQuery) {
  if (ppQuery) {
    *ppQuery = ref(new EventQuery(pDevice, pDesc));
    return S_OK;
  }
  return S_FALSE;
}

HRESULT CreateOcculusionQuery(IMTLD3D11Device *pDevice,
                              const D3D11_QUERY_DESC *pDesc,
                              ID3D11Query **ppQuery) {
  if (ppQuery) {
    *ppQuery = ref(new OcculusionQuery(pDevice, pDesc));
    return S_OK;
  }
  return S_FALSE;
}

} // namespace dxmt