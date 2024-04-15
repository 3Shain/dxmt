#include "d3d11_context_state.hpp"
#include "com/com_guid.hpp"

namespace dxmt {

MTLD3D11DeviceContextState::MTLD3D11DeviceContextState(IMTLD3D11Device *pDevice)
    : MTLD3D11DeviceChild<ID3DDeviceContextState>(pDevice) {}

MTLD3D11DeviceContextState::~MTLD3D11DeviceContextState() {}

HRESULT STDMETHODCALLTYPE
MTLD3D11DeviceContextState::QueryInterface(REFIID riid, void **ppvObject) {
  if (ppvObject == nullptr)
    return E_POINTER;

  *ppvObject = nullptr;

  if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
      riid == __uuidof(ID3DDeviceContextState)) {
    *ppvObject = ref(this);
    return S_OK;
  }

  if (logQueryInterfaceError(__uuidof(ID3DDeviceContextState), riid)) {
    WARN("Unknown interface query");
    Logger::warn(str::format(riid));
  }

  return E_NOINTERFACE;
}

} // namespace dxmt