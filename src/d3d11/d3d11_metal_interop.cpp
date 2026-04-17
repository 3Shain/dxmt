#include "d3d11_metal_interop.hpp"
#include "d3d11_fence.hpp"
#include "d3d11_resource.hpp"

namespace dxmt {

HRESULT STDMETHODCALLTYPE
MTLD3D11InteropDevice::ImportMTLTexture2D(
    const D3D11_TEXTURE2D_DESC1 *pDesc, uint64_t mtlTexture, ID3D11Texture2D **ppTexture2D
) {
  return dxmt::ImportMTLTexture2D(device_, pDesc, mtlTexture, ppTexture2D);
}

HRESULT STDMETHODCALLTYPE
MTLD3D11InteropDevice::GetFenceSharedEvent(ID3D11Fence *pFence, uint64_t *pMtlSharedEvent) {
  if (!pFence || !pMtlSharedEvent)
    return E_POINTER;
  auto *fence = static_cast<MTLD3D11Fence *>(pFence);
  *pMtlSharedEvent = fence->event.handle;
  return S_OK;
}

} // namespace dxmt
