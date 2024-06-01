#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

#pragma region StagingBuffer

class StagingBuffer : public TResourceBase<tag_buffer> {
public:
  StagingBuffer(const tag_buffer::DESC_S *desc,
                const D3D11_SUBRESOURCE_DATA *pInitialData,
                IMTLD3D11Device *device)
      : TResourceBase<tag_buffer>(desc, device) {
    auto metal = device->GetMTLDevice();
    buffer = transfer(
        metal->newBuffer(desc->ByteWidth, MTL::ResourceStorageModeShared));
    if (pInitialData) {
      memcpy(buffer->contents(), pInitialData->pSysMem, desc->ByteWidth);
    }
  }

  Obj<MTL::Buffer> buffer;
};

HRESULT
CreateStagingBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                    ID3D11Buffer **ppBuffer) {
  *ppBuffer = ref(new StagingBuffer(pDesc, pInitialData, pDevice));
  return S_OK;
}

#pragma endregion

#pragma region StagingTexture

template <typename tag_texture>
class StagingTexture : public TResourceBase<tag_texture> {

public:
  StagingTexture(const tag_texture::DESC_S *pDesc, IMTLD3D11Device *pDevice)
      : TResourceBase<tag_texture>(pDesc, pDevice) {}

  HRESULT PrivateQueryInterface(REFIID riid, void **ppvObject) {

    return E_FAIL;
  }
};

#pragma endregion

template <typename tag>
HRESULT CreateStagingTextureInternal(IMTLD3D11Device *pDevice,
                                     const typename tag::DESC_S *pDesc,
                                     const D3D11_SUBRESOURCE_DATA *pInitialData,
                                     typename tag::COM **ppTexture) {
  // auto metal = pDevice->GetMTLDevice();
  // auto textureDescriptor = getTextureDescriptor(pDevice, pDesc);
  // assert(textureDescriptor);
  // auto texture = transfer(metal->newTexture(textureDescriptor));
  // if (pInitialData) {
  //   initWithSubresourceData(texture, pDesc, pInitialData);
  // }
  *ppTexture = ref(new StagingTexture<tag>(pDesc, pDevice));
  return S_OK;
}

HRESULT
CreateStagingTexture1D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE1D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture1D **ppTexture) {
  return CreateStagingTextureInternal<tag_texture_1d>(pDevice, pDesc,
                                                      pInitialData, ppTexture);
}

HRESULT
CreateStagingTexture2D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE2D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture2D **ppTexture) {
  return CreateStagingTextureInternal<tag_texture_2d>(pDevice, pDesc,
                                                      pInitialData, ppTexture);
}

HRESULT
CreateStagingTexture3D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE3D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture3D **ppTexture) {
  return CreateStagingTextureInternal<tag_texture_3d>(pDevice, pDesc,
                                                      pInitialData, ppTexture);
}

} // namespace dxmt