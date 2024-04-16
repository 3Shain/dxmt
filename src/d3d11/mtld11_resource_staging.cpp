
#include "mtld11_resource.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_texture.hpp"

namespace dxmt {

#pragma region StagingBuffer

class StagingBuffer : public TResourceBase<tag_buffer> {
public:
  StagingBuffer(const tag_buffer::DESC_S *desc, IMTLD3D11Device *device)
      : TResourceBase<tag_buffer>(desc, device) {
    auto metal = device->GetMTLDevice();
    buffer = transfer(metal->newBuffer(desc->ByteWidth, 0)); // FIXME
  }

  Obj<MTL::Buffer> buffer;
};

#pragma endregion

#pragma region StagingTexture

template <typename tag_texture>
class StagingTexture : public TResourceBase<tag_texture> {

public:
  StagingTexture(const tag_texture::DESC_S *desc, IMTLD3D11Device *device)
      : TResourceBase<tag_texture>(desc, device) {
    auto descriptor = getTextureDescriptor(nullptr, desc);
    auto metal = device->GetMTLDevice();
    texture = transfer(metal->newTexture(descriptor));
  }

  virtual HRESULT PrivateQueryInterface(REFIID riid, void **ppvObject) {

    return E_FAIL;
  }

  Obj<MTL::Texture> texture;
};

#pragma endregion

Com<ID3D11Buffer> CreateStagingBuffer(IMTLD3D11Device *pDevice,
                                      const D3D11_BUFFER_DESC *pDesc) {
  return new StagingBuffer(pDesc, pDevice);
}

Com<ID3D11Texture1D> CreateStagingTexture1D(IMTLD3D11Device *pDevice,
                                            const D3D11_TEXTURE1D_DESC *pDesc) {
  return new StagingTexture<tag_texture_1d>(pDesc, pDevice);
}

Com<ID3D11Texture2D> CreateStagingTexture2D(IMTLD3D11Device *pDevice,
                                            const D3D11_TEXTURE2D_DESC *pDesc) {
  return new StagingTexture<tag_texture_2d>(pDesc, pDevice);
}

} // namespace dxmt