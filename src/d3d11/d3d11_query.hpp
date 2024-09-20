#pragma once
#include "com/com_guid.hpp"
#include "d3d11_device_child.hpp"
#include "dxmt_occlusion_query.hpp"
#include "log/log.hpp"

DEFINE_COM_INTERFACE("a301e56d-d87e-4b69-8440-bd003e285904",
                     IMTLD3DOcclusionQuery)
    : public ID3D11Query {
  virtual HRESULT GetData(uint64_t * data) = 0;
  /**
  Note: this will add a ref if the query is in `Signaled` state
  */
  virtual dxmt::VisibilityResultObserver *Begin(
      uint64_t occlusion_counter_begin) = 0;
  virtual void End(uint64_t occlusion_counter_end) = 0;
};

DEFINE_COM_INTERFACE("81fe1837-05fa-4927-811f-3699f997cb9f", IMTLD3DEventQuery)
    : public ID3D11Query {
  virtual HRESULT GetData(BOOL* data, uint64_t coherent_seq_id) = 0;
  virtual void Issue(uint64_t current_seq_id) = 0;
};

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

template <typename DataType>
class MTLD3D11DummyQuery : public MTLD3DQueryBase<ID3D11Query> {
public:
  MTLD3D11DummyQuery(IMTLD3D11Device *pDevice, const D3D11_QUERY_DESC *desc)
      : MTLD3DQueryBase<ID3D11Query>(pDevice, desc) {}

  virtual UINT STDMETHODCALLTYPE GetDataSize() override {
    return sizeof(DataType);
  };
};

/**

From D3D11.3 Functional Spec:

- Passing IssueFlags with only the D3DISSUE_BEGIN bit set causes the Query to
enter the "building" state (regardless of whatever state it was in before)

- A second D3DISSUE_BEGIN will result in the range being reset (the first
D3DISSUE_BEGIN is effectively discarded/ ignored).

- Some Query Types only support D3DISSUE_END

- When the Query is in the "signaled" state, the Query Type supports
D3DISSUE_BEGIN, and Issue is invoked with just the D3DISSUE_END flag: it is
equivalent to invoking Issue with both D3DISSUE_BEGIN and D3DISSUE_END bits set,
as well as being equivalent to an invocation of Issue with D3DISSUE_BEGIN
followed immediately by another invocation of Issue with D3DISSUE_END

- (BEGIN and END) define a bracketing of graphics commands

- Bracketings of Queries are allowed to overlap and nest.
 */
enum class QueryState {
  /**
    After DeviceContext::Begin
   */
  Building,
  /**
    After DeviceContext::End
   */
  Issued,
  /**
    Default state, or when data is ready
   */
  Signaled,
};

HRESULT CreateEventQuery(IMTLD3D11Device *pDevice,
                         const D3D11_QUERY_DESC *pDesc, ID3D11Query **ppQuery);

HRESULT CreateOcculusionQuery(IMTLD3D11Device *pDevice,
                              const D3D11_QUERY_DESC *pDesc,
                              ID3D11Query **ppQuery);

} // namespace dxmt