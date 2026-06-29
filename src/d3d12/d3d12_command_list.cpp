#include "d3d12_command_allocator.hpp"

namespace dxmt {
HRESULT STDMETHODCALLTYPE
MTLD3D12CommandAllocatorImpl::CreateCommandList(
    UINT NodeMask, D3D12_COMMAND_LIST_TYPE Type, ID3D12PipelineState *pInitialPipelineState, REFIID riid,
    void **ppCommandList
) {
  return E_NOTIMPL;
}

}; // namespace dxmt