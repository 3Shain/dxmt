#pragma once
#include "com/com_guid.hpp"
#include "d3d11_device_child.hpp"

DEFINE_COM_INTERFACE("a301e56d-d87e-4b69-8440-bd003e285904",
                     IMTLD3DOcclusionQuery)
    : public ID3D11Query{};

DEFINE_COM_INTERFACE("81fe1837-05fa-4927-811f-3699f997cb9f", IMTLD3DEventQuery)
    : public ID3D11Query{};

namespace dxmt {

template <typename Query>
class MTLD3DQueryBase : public MTLD3D11DeviceChild<Query> {
public:
  MTLD3DQueryBase(IMTLD3D11Device *pDevice, const D3D11_QUERY_DESC *desc)
      : MTLD3D11DeviceChild<Query>(pDevice), desc_(*desc) {}
  ~MTLD3DQueryBase() {};

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11Query) || riid == __uuidof(Query)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(Query), riid)) {
      Logger::warn("D3D11Query::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }

    return E_NOINTERFACE;
  };

  void STDMETHODCALLTYPE GetDesc(D3D11_QUERY_DESC *pDesc) final {
    if (pDesc) {
      *pDesc = desc_;
    }
  };

private:
  D3D11_QUERY_DESC desc_;
};

enum class QueryState {
  Created, // ?
  Issued,
  Signaled,
};

HRESULT CreateEventQuery(IMTLD3D11Device *pDevice,
                         const D3D11_QUERY_DESC *pDesc, ID3D11Query **ppQuery);

HRESULT CreateOcculusionQuery(IMTLD3D11Device *pDevice,
                              const D3D11_QUERY_DESC *pDesc,
                              ID3D11Query **ppQuery);

} // namespace dxmt