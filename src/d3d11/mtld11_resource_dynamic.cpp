
#include "d3d11_device.hpp"
#include "mtld11_resource.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_texture.hpp"

namespace dxmt {

#pragma region StagingBuffer

class DynamicBuffer : public TResourceBase<tag_buffer> {
public:
  DynamicBuffer(const tag_buffer::DESC_S *desc, IMTLD3D11Device *device)
      : TResourceBase<tag_buffer>(desc, device) {
    auto metal = device->GetMTLDevice();
    buffer = transfer(metal->newBuffer(desc->ByteWidth, 0)); // FIXME
  }

  Obj<MTL::Buffer> buffer;
};

#pragma endregion

Com<ID3D11Buffer> CreateDynamicBuffer(IMTLD3D11Device *pDevice,
                                      const D3D11_BUFFER_DESC *pDesc) {
  return new DynamicBuffer(pDesc, pDevice);
}

} // namespace dxmt
