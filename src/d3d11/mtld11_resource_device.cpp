#include "Metal/MTLResource.hpp"
#include "com/com_pointer.hpp"
#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

class DeviceBuffer : public TResourceBase<tag_buffer, IMTLBindable> {
private:
  Obj<MTL::Buffer> buffer;

public:
  DeviceBuffer(const tag_buffer::DESC_S *desc,
               const D3D11_SUBRESOURCE_DATA *pInitialData,
               IMTLD3D11Device *device)
      : TResourceBase<tag_buffer, IMTLBindable>(desc, device) {
    auto metal = device->GetMTLDevice();
    buffer = transfer(
        metal->newBuffer(desc->ByteWidth, MTL::ResourceStorageModeManaged));
    if (pInitialData) {
      memcpy(buffer->contents(), pInitialData->pSysMem, desc->ByteWidth);
      buffer->didModifyRange({0, desc->ByteWidth});
    }
  }
  virtual HRESULT PrivateQueryInterface(REFIID riid, void **ppvObject) {
    if (riid == __uuidof(IMTLBindable)) {
      *ppvObject = ref_and_cast<IMTLBindable>(this);
      return S_OK;
    }
    return E_NOINTERFACE;
  };
  virtual void GetBoundResource(MTL_BIND_RESOURCE *ppResource) {
    (*ppResource).IsTexture = 0;
    (*ppResource).Buffer = buffer.ptr();
  };
  virtual void GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) {
    QueryInterface(riid, ppLogicalResource);
  };
};

Com<ID3D11Buffer>
CreateDeviceBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                   const D3D11_SUBRESOURCE_DATA *pInitialData) {
  return new DeviceBuffer(pDesc, pInitialData, pDevice);
}

} // namespace dxmt