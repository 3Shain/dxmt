#include "d3d11_query.hpp"
#include "com/com_guid.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"

namespace dxmt {

MTLD3D11Query::MTLD3D11Query(IMTLD3D11Device *pDevice,
                             const D3D11_QUERY_DESC &desc)
    : MTLD3D11DeviceChild(pDevice), m_desc(desc) {}

MTLD3D11Query::~MTLD3D11Query() {}
HRESULT
STDMETHODCALLTYPE
MTLD3D11Query::QueryInterface(REFIID riid, void **ppvObject) {
  InitReturnPtr(ppvObject);

  if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
      riid == __uuidof(ID3D11Asynchronous) || riid == __uuidof(ID3D11Query)) {
    *ppvObject = ref(this);
    return S_OK;
  }

  if (logQueryInterfaceError(__uuidof(ID3D11Query), riid)) {
    WARN("Unknown interface query", str::format(riid));
  }

  return E_NOINTERFACE;
}

// UINT STDMETHODCALLTYPE MTLD3D11Query::GetDataSize() {
//   switch (m_desc.Query) {
//   case D3D11_QUERY_EVENT:
//     return sizeof(BOOL);

//   case D3D11_QUERY_OCCLUSION:
//     return sizeof(UINT64);

//   case D3D11_QUERY_TIMESTAMP:
//     return sizeof(UINT64);

//   case D3D11_QUERY_TIMESTAMP_DISJOINT:
//     return sizeof(D3D11_QUERY_DATA_TIMESTAMP_DISJOINT);

//   case D3D11_QUERY_PIPELINE_STATISTICS:
//     return sizeof(D3D11_QUERY_DATA_PIPELINE_STATISTICS);

//   case D3D11_QUERY_OCCLUSION_PREDICATE:
//     return sizeof(BOOL);

//   case D3D11_QUERY_SO_STATISTICS:
//   case D3D11_QUERY_SO_STATISTICS_STREAM0:
//   case D3D11_QUERY_SO_STATISTICS_STREAM1:
//   case D3D11_QUERY_SO_STATISTICS_STREAM2:
//   case D3D11_QUERY_SO_STATISTICS_STREAM3:
//     return sizeof(D3D11_QUERY_DATA_SO_STATISTICS);

//   case D3D11_QUERY_SO_OVERFLOW_PREDICATE:
//   case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM0:
//   case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM1:
//   case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM2:
//   case D3D11_QUERY_SO_OVERFLOW_PREDICATE_STREAM3:
//     return sizeof(BOOL);
//   }

//   ERR("D3D11Query: Failed to query data size");
//   return 0;
// }

void STDMETHODCALLTYPE MTLD3D11Query::GetDesc(D3D11_QUERY_DESC *pDesc) {
  *pDesc = m_desc;
}

void MTLD3D11TimeStampDisjointQuery::Begin() {
  m_parent->GetMTLDevice()->sampleTimestamps(&cputime_begin_, &gputime_begin_);
  state |= STATE::begin;
};

void MTLD3D11TimeStampDisjointQuery::End() {
  m_parent->GetMTLDevice()->sampleTimestamps(&cputime_end_, &gputime_end_);
  state |= end;
};

HRESULT STDMETHODCALLTYPE
MTLD3D11TimeStampDisjointQuery::GetData(void *pData, UINT GetDataFlags) {
  if ((state & ready) == ready) {
    if (pData) {
      D3D11_QUERY_DATA_TIMESTAMP_DISJOINT *data =
          (D3D11_QUERY_DATA_TIMESTAMP_DISJOINT *)pData;
      if (cputime_end_ > cputime_begin_ && gputime_end_ > gputime_begin_) {
        data->Disjoint = FALSE;
        auto cpuSpan = (cputime_end_ - cputime_begin_); // nanoseconds
        data->Frequency =
            1000000000 * (gputime_end_ - gputime_begin_) /
            cpuSpan; // 1e9 * gpuTicks per nanosec = gpuTicks per sec
      } else {
        data->Disjoint = TRUE;
        data->Frequency = 0;
      }
    }
    return S_OK;
  }
  return S_FALSE;
}

void MTLD3D11TimeStampQuery::End(){
    // TODO: insert a timestamp counter
};

HRESULT STDMETHODCALLTYPE MTLD3D11TimeStampQuery::GetData(void *pData,
                                                          UINT GetDataFlags) {
  return S_FALSE;
}

} // namespace dxmt