#include "d3d11_query.hpp"
#include "d3d11_private.h"
#include "dxmt_occlusion_query.hpp"

namespace dxmt {

class EventQuery : public MTLD3DQueryBase<IMTLD3DEventQuery> {
  using MTLD3DQueryBase<IMTLD3DEventQuery>::MTLD3DQueryBase;

  QueryState state = QueryState::Signaled;

  virtual UINT STDMETHODCALLTYPE GetDataSize() override {
    return sizeof(BOOL);
  };

  uint64_t should_be_signaled_at = 0;

  virtual void Issue(uint64_t current_seq_id) override {
    state = QueryState::Issued;
    should_be_signaled_at = current_seq_id;
  };

  virtual HRESULT GetData(BOOL* data, uint64_t coherent_seq_id) override {
    if (state == QueryState::Signaled ||
        should_be_signaled_at <= coherent_seq_id) {
      state = QueryState::Signaled;
      *data = TRUE;
      return S_OK;
    }
    *data = FALSE;
    return S_FALSE;
  };
};

class OcculusionQuery : public MTLD3DQueryBase<IMTLD3DOcclusionQuery>,
                        VisibilityResultObserver {
  using MTLD3DQueryBase<IMTLD3DOcclusionQuery>::MTLD3DQueryBase;

  QueryState state = QueryState::Signaled;
  uint64_t occlusion_counter_begin = 0;
  // accumulate counter strictly BEFORE
  uint64_t occlusion_counter_end = 0;
  uint64_t accumulated_value = 0;

  virtual UINT STDMETHODCALLTYPE GetDataSize() override {
    return sizeof(UINT64);
  };

  virtual HRESULT GetData(uint64_t *data) override {
    if (state == QueryState::Signaled) {
      *data = accumulated_value;
      return S_OK;
    }
    return S_FALSE;
  }

  virtual VisibilityResultObserver *
  Begin(uint64_t occlusion_counter_begin) override {
    this->occlusion_counter_begin = occlusion_counter_begin;
    this->occlusion_counter_end = ~0uLL;
    accumulated_value = 0;
    if (state == QueryState::Signaled) {
      AddRef();
      state = QueryState::Building;
      return static_cast<VisibilityResultObserver *>(this);
    }
    if (state == QueryState::Issued) {
      // discard previous issued query
      state = QueryState::Building;
    }
    return nullptr;
  };

  virtual void End(uint64_t occlusion_counter_end) override {
    if (state == QueryState::Issued) {
      // FIXME: is this actually allowed?
      ERR("try to re-issue an occlusion query");
      return;
    }
    if (state == QueryState::Signaled) {
      // no effect
      this->occlusion_counter_begin = occlusion_counter_end;
      this->occlusion_counter_end = occlusion_counter_end;
      this->accumulated_value = 0;
      return;
    }
    this->occlusion_counter_end = occlusion_counter_end;
    state = QueryState::Issued;
    return;
  };

  virtual bool Update(uint64_t occlusion_counter, uint64_t value) override {
    // expect `occlusion_counter` to be monotonous increasing for each
    // invocation
    D3D11_ASSERT(state != QueryState::Signaled);
    if (occlusion_counter < occlusion_counter_begin) {
      // simply ignore
      return false;
    }
    if (occlusion_counter == occlusion_counter_end) {
      // this is possible if End happens immediately after Begin
      state = QueryState::Signaled;
      return true;
    }
    if (occlusion_counter > occlusion_counter_end) {
      D3D11_ASSERT(0 && "unreachable");
    }
    accumulated_value += value;
    if (occlusion_counter + 1 == occlusion_counter_end) {
      D3D11_ASSERT(state == QueryState::Issued);
      state = QueryState::Signaled;
      Release();
      // return true, so caller can know this query has done
      return true;
    }
    return false;
  }
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