#pragma once

#include "d3d11_device_child.hpp"

namespace dxmt {

using timestamp_t = MTL::Timestamp;

enum class D3D11QueryState {
  Created, // ?
  Issued,
  Signaled,
};

class MTLD3D11Query : public MTLD3D11DeviceChild<ID3D11Query> {
public:
  MTLD3D11Query(IMTLD3D11Device *pDevice, const D3D11_QUERY_DESC &desc);
  ~MTLD3D11Query();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;

  void STDMETHODCALLTYPE GetDesc(D3D11_QUERY_DESC *pDesc) final;

  virtual UINT STDMETHODCALLTYPE GetDataSize() = 0;

  virtual HRESULT STDMETHODCALLTYPE GetData(void *pData, UINT GetDataFlags) = 0;

  virtual void Begin() {}

  virtual void End() {}

private:
  D3D11_QUERY_DESC m_desc;
};

class MTLD3D11TimeStampQuery : public MTLD3D11Query {
public:
  using MTLD3D11Query::MTLD3D11Query;
  UINT STDMETHODCALLTYPE GetDataSize() final {
    return sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT);
  };
  HRESULT STDMETHODCALLTYPE GetData(void *pData, UINT GetDataFlags) final;
  void End() final;
};

class MTLD3D11TimeStampDisjointQuery : public MTLD3D11Query {
public:
  using MTLD3D11Query::MTLD3D11Query;
  UINT STDMETHODCALLTYPE GetDataSize() final { return sizeof(UINT64); };
  HRESULT STDMETHODCALLTYPE GetData(void *pData, UINT GetDataFlags) final;
  void Begin() final;
  void End() final;

private:
  timestamp_t cputime_begin_;
  timestamp_t gputime_begin_;

  timestamp_t cputime_end_;
  timestamp_t gputime_end_;

  enum STATE : unsigned { begin = 1, end = 2, ready = begin | end };

  unsigned state;
};
} // namespace dxmt