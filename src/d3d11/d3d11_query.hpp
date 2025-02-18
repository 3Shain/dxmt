#pragma once
#include "com/com_guid.hpp"
#include "d3d11_device_child.hpp"
#include "dxmt_occlusion_query.hpp"
#include "log/log.hpp"

DEFINE_COM_INTERFACE("a301e56d-d87e-4b69-8440-bd003e285904",
                     IMTLD3DOcclusionQuery)
    : public ID3D11Query {
  virtual HRESULT GetData(void *data) = 0;
  virtual bool Begin() = 0;
  virtual bool End() = 0;
  virtual void DoDeferredQuery(dxmt::Rc<dxmt::VisibilityResultQuery> &deferred_query) = 0;

  virtual dxmt::Rc<dxmt::VisibilityResultQuery> __query() = 0;
};

namespace dxmt {

template <typename Query>
class MTLD3DQueryBase : public MTLD3D11DeviceChild<Query> {
public:
  MTLD3DQueryBase(MTLD3D11Device *pDevice, const D3D11_QUERY_DESC *desc)
      : MTLD3D11DeviceChild<Query>(pDevice), desc_(*desc) {}
  ~MTLD3DQueryBase() {};

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11Query)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11Query), riid)) {
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

protected:
  D3D11_QUERY_DESC desc_;
};

template <typename DataType>
class MTLD3D11DummyQuery : public MTLD3DQueryBase<ID3D11Query> {
public:
  MTLD3D11DummyQuery(MTLD3D11Device *pDevice, const D3D11_QUERY_DESC *desc)
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

struct MTLD3D11EventQuery : public ID3D11Query {
  virtual void Issue(uint64_t current_seq_id) = 0;
  virtual bool CheckEventState(uint64_t coherent_seq_id) = 0;
};

template <typename DataType>
class MTLD3D11EventQueryImpl : public MTLD3DQueryBase<MTLD3D11EventQuery> {
public:
  MTLD3D11EventQueryImpl(MTLD3D11Device *pDevice, const D3D11_QUERY_DESC *desc)
      : MTLD3DQueryBase<MTLD3D11EventQuery>(pDevice, desc) {}

  UINT STDMETHODCALLTYPE GetDataSize() override { return sizeof(DataType); };

  void Issue(uint64_t current_seq_id) override {
    state = QueryState::Issued;
    should_be_signaled_at = current_seq_id;
  }

  bool CheckEventState(uint64_t coherent_seq_id) override {
    if (state == QueryState::Signaled || should_be_signaled_at <= coherent_seq_id) {
      state = QueryState::Signaled;
      return true;
    }
    return false;
  };

private:
  QueryState state = QueryState::Signaled;
  uint64_t should_be_signaled_at = 0;
};

HRESULT CreateOcculusionQuery(MTLD3D11Device *pDevice,
                              const D3D11_QUERY_DESC *pDesc,
                              ID3D11Query **ppQuery);

} // namespace dxmt