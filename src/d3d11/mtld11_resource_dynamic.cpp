
#include "com/com_object.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "mtld11_resource.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "objc_pointer.hpp"
#include <set>

namespace dxmt {

#pragma region StagingBuffer

class DynamicBuffer : public TResourceBase<tag_buffer, IMTLDynamicBuffer> {
private:
  Obj<MTL::Buffer> buffer;

  // using SRVBase = TResourceViewBase<tag_shader_resource_view<DynamicBuffer>,
  // IMTLDynamicBuffer> ; class SRVView :public SRVBase {};

  class DynamicBindable : public ComObject<IMTLBindable> {
  private:
    Com<DynamicBuffer> parent;
    std::function<void()> observer;

  public:
    DynamicBindable(DynamicBuffer *parent, std::function<void()> &&observer)
        : ComObject(), parent(parent), observer(observer) {}

    ~DynamicBindable() { parent->RemoveObserver(this); }

    HRESULT QueryInterface(REFIID riid, void **ppvObject) {
      if (ppvObject == nullptr)
        return E_POINTER;

      *ppvObject = nullptr;

      if (riid == __uuidof(IUnknown) || riid == __uuidof(IMTLBindable)) {
        *ppvObject = ref(this);
        return S_OK;
      }

      return E_NOINTERFACE;
    }

    void GetBoundResource(MTL_BIND_RESOURCE *ppResource) {
      MTL_BIND_RESOURCE &resource = *ppResource;
      resource.IsTexture = false;
      resource.Buffer = parent->buffer.ptr();
    };

    void GetLogicalResourceOrView(REFIID riid, void **ppLogicalResource) {
      parent->QueryInterface(riid, ppLogicalResource);
    };

    void NotifyObserver() { observer(); };
  };

  std::set<DynamicBindable *> observers;

public:
  DynamicBuffer(const tag_buffer::DESC_S *desc,
                const D3D11_SUBRESOURCE_DATA *pInitialData,
                IMTLD3D11Device *device)
      : TResourceBase<tag_buffer, IMTLDynamicBuffer>(desc, device) {
    auto metal = device->GetMTLDevice();
    buffer = transfer(metal->newBuffer(desc->ByteWidth, 0));
    if (pInitialData) {
      memcpy(buffer->contents(), pInitialData->pSysMem, desc->ByteWidth);
      buffer->didModifyRange({0, desc->ByteWidth});
    }
  }

  MTL::Buffer *GetCurrentBuffer() { return buffer.ptr(); };

  void GetBindable(IMTLBindable **ppResource,
                   std::function<void()> &&onBufferSwap) {
    *ppResource = new DynamicBindable(this, std::move(onBufferSwap));
    assert(observers.insert((DynamicBindable *)*ppResource).second &&
           "are you kidding me?");
  };

  void RotateBuffer(IMTLDynamicBufferPool *pool) {
    pool->ExchangeFromPool(&buffer);
    for (auto &observer : observers) {
      observer->NotifyObserver();
    }
  };

  void RemoveObserver(DynamicBindable *pBindable) {
    assert(observers.erase(pBindable) == 1 &&
           "it must be 1 unless the destructor called twice");
  }

  HRESULT PrivateQueryInterface(const IID &riid, void **ppvObject) {
    if (riid == __uuidof(IMTLDynamicBuffer)) {
      *ppvObject = ref_and_cast<IMTLDynamicBuffer>(this);
      return S_OK;
    }
    return E_FAIL;
  }
};

#pragma endregion

Com<ID3D11Buffer>
CreateDynamicBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                    const D3D11_SUBRESOURCE_DATA *pInitialData) {
  return new DynamicBuffer(pDesc, pInitialData, pDevice);
}

} // namespace dxmt
