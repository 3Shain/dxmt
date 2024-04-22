
#include "Metal/MTLResource.hpp"
#include "mtld11_resource.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_texture.hpp"

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

#pragma endregion

#pragma region StagingTexture

template <typename tag_texture>
class StagingTexture : public TResourceBase<tag_texture> {

public:
  StagingTexture(const tag_texture::DESC_S *pDesc,
                 const D3D11_SUBRESOURCE_DATA *pInitialData,
                 IMTLD3D11Device *pDevice)
      : TResourceBase<tag_texture>(pDesc, pDevice) {
    auto metal = pDevice->GetMTLDevice();
    auto textureDescriptor = getTextureDescriptor(pDevice, pDesc);
    assert(textureDescriptor);
    texture = transfer(metal->newTexture(textureDescriptor));
    if (pInitialData) {
      initWithSubresourceData(texture, pDesc, pInitialData);
    }
  }

  HRESULT PrivateQueryInterface(REFIID riid, void **ppvObject) {

    return E_FAIL;
  }

  Obj<MTL::Texture> texture;
};

#pragma endregion

Com<ID3D11Buffer>
CreateStagingBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                    const D3D11_SUBRESOURCE_DATA *pInitialData) {
  return new StagingBuffer(pDesc, pInitialData, pDevice);
}

Com<ID3D11Texture1D>
CreateStagingTexture1D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE1D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData) {
  return new StagingTexture<tag_texture_1d>(pDesc, pInitialData, pDevice);
}

Com<ID3D11Texture2D>
CreateStagingTexture2D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE2D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData) {
  return new StagingTexture<tag_texture_2d>(pDesc, pInitialData, pDevice);
}

} // namespace dxmt