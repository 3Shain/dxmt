#include "d3d11_query.hpp"
#include "d3d11_private.h"

namespace dxmt {

class EventQuery : public MTLD3DQueryBase<IMTLD3DEventQuery> {
  using MTLD3DQueryBase<IMTLD3DEventQuery>::MTLD3DQueryBase;

  QueryState state = QueryState::Signaled;

  virtual UINT STDMETHODCALLTYPE
  GetDataSize() override {
    return sizeof(BOOL);
  };

  uint64_t should_be_signaled_at = 0;

  virtual void
  Issue(uint64_t current_seq_id) override {
    state = QueryState::Issued;
    should_be_signaled_at = current_seq_id;
  };

  virtual HRESULT
  GetData(BOOL *data, uint64_t coherent_seq_id) override {
    if (state == QueryState::Signaled || should_be_signaled_at <= coherent_seq_id) {
      state = QueryState::Signaled;
      *data = TRUE;
      return S_OK;
    }
    *data = FALSE;
    return S_FALSE;
  };
};

class OcculusionQuery : public MTLD3DQueryBase<IMTLD3DOcclusionQuery> {
  using MTLD3DQueryBase<IMTLD3DOcclusionQuery>::MTLD3DQueryBase;

  QueryState state = QueryState::Signaled;

  uint64_t seq_id_begin = 0;
  uint64_t occlusion_counter_begin = 0;
  uint64_t seq_id_end = 0;
  uint64_t occlusion_counter_end = 0;
  uint64_t accumulated_value = 0;

  virtual UINT STDMETHODCALLTYPE
  GetDataSize() override {
    return desc_.Query == D3D11_QUERY_OCCLUSION_PREDICATE ? sizeof(BOOL) : sizeof(UINT64);
  };

  virtual HRESULT
  GetData(void *data) override {
    if (state == QueryState::Signaled) {
      if (desc_.Query == D3D11_QUERY_OCCLUSION_PREDICATE) {
        *((BOOL *)data) = accumulated_value != 0;
      } else {
        *((uint64_t *)data) = accumulated_value;
      }
      return S_OK;
    }
    return S_FALSE;
  }

  virtual void
  Begin(uint64_t seq_id, uint64_t occlusion_counter_begin) override {
    seq_id_begin = seq_id;
    seq_id_end = ~0uLL;
    this->occlusion_counter_begin = occlusion_counter_begin;
    occlusion_counter_end = ~0uLL;
    accumulated_value = 0;
    if (state == QueryState::Signaled) {
      state = QueryState::Building;
      return;
    }
    if (state == QueryState::Issued) {
      // discard previous issued query
      state = QueryState::Building;
    }
  };

  virtual void
  End(uint64_t seq_id, uint64_t occlusion_counter_end) override {
    if (state == QueryState::Issued) {
      // FIXME: is this actually allowed?
      ERR("try to re-issue an occlusion query");
      return;
    }
    if (state == QueryState::Signaled) {
      // no effect
      seq_id_begin = seq_id;
      seq_id_end = seq_id;
      occlusion_counter_begin = occlusion_counter_end;
      this->occlusion_counter_end = occlusion_counter_end;
      accumulated_value = 0;
      return;
    }
    seq_id_end = seq_id;
    this->occlusion_counter_end = occlusion_counter_end;
    state = QueryState::Issued;
    return;
  };

  virtual bool
  Issue(uint64_t current_seq_id, uint64_t *data, uint64_t count) override {
    D3D11_ASSERT(state != QueryState::Signaled);
    if (current_seq_id < seq_id_begin) {
      // ?
      return false;
    }
    uint64_t *start = current_seq_id == seq_id_begin ? data + occlusion_counter_begin : data;
    uint64_t *end = current_seq_id == seq_id_end ? data + occlusion_counter_end : data + count;
    D3D11_ASSERT(start <= end);
    while (start != end) {
      accumulated_value += *start;
      start++;
    }
    if (current_seq_id == seq_id_end) {
      state = QueryState::Signaled;
      return true;
    }
    return false;
  }
};

HRESULT
CreateEventQuery(MTLD3D11Device *pDevice, const D3D11_QUERY_DESC *pDesc, ID3D11Query **ppQuery) {
  if (ppQuery) {
    *ppQuery = ref(new EventQuery(pDevice, pDesc));
    return S_OK;
  }
  return S_FALSE;
}

HRESULT
CreateOcculusionQuery(MTLD3D11Device *pDevice, const D3D11_QUERY_DESC *pDesc, ID3D11Query **ppQuery) {
  if (ppQuery) {
    *ppQuery = ref(new OcculusionQuery(pDevice, pDesc));
    return S_OK;
  }
  return S_FALSE;
}

} // namespace dxmt