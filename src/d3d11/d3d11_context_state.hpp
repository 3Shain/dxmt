#pragma once
#include "mtld11_state.hpp"

namespace dxmt {

class MTLD3D11DeviceContext;

class MTLD3D11DeviceContextState
    : public MTLD3D11DeviceChild<ID3DDeviceContextState> {
  friend class MTLD3D11DeviceContext;

public:
  MTLD3D11DeviceContextState(IMTLD3D11Device *pDevice);

  ~MTLD3D11DeviceContextState();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject);

private:
  D3D11ContextState state_;
};
} // namespace dxmt