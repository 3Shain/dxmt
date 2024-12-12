#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_private.h"
#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "dxmt_buffer_pool.hpp"
#include "dxmt_format.hpp"
#include "dxmt_texture.hpp"
#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

#pragma region DynamicTexture

class DynamicTexture2D : public TResourceBase<tag_texture_2d, IMTLDynamicBuffer> {
private:
  Rc<Texture> texture_;
  Rc<TextureAllocation> allocation;
  size_t bytes_per_image_;
  size_t bytes_per_row_;

  std::unique_ptr<DynamicTexturePool2> pool;

  using SRVBase = TResourceViewBase<tag_shader_resource_view<DynamicTexture2D>>;

  class SRV : public SRVBase {
    TextureViewKey view_key;

  public:
    SRV(const tag_shader_resource_view<>::DESC1 *pDesc, DynamicTexture2D *pResource, MTLD3D11Device *pDevice,
        TextureViewKey view_key) :
        SRVBase(pDesc, pResource, pDevice),
        view_key(view_key) {}

    ~SRV() {}

    Rc<Buffer> buffer() final { return {}; };
    Rc<Texture> texture() final { return this->resource->texture_; };
    unsigned viewId() final { return view_key;};
    BufferSlice bufferSlice() final { return {};}
  };

public:
  DynamicTexture2D(
      const tag_texture_2d::DESC1 *pDesc, Obj<MTL::TextureDescriptor> &&descriptor,
      const D3D11_SUBRESOURCE_DATA *pInitialData, UINT bytes_per_image, UINT bytes_per_row, MTLD3D11Device *device
  ) :
      TResourceBase<tag_texture_2d, IMTLDynamicBuffer>(*pDesc, device),
      bytes_per_image_(bytes_per_image),
      bytes_per_row_(bytes_per_row) {
    texture_ = new Texture(bytes_per_image, bytes_per_row, std::move(descriptor), device->GetMTLDevice());
    Flags<TextureAllocationFlag> flags;
    if (!m_parent->IsTraced() && pDesc->Usage == D3D11_USAGE_DYNAMIC)
      flags.set(TextureAllocationFlag::CpuWriteCombined);
    // if (pDesc->Usage != D3D11_USAGE_DEFAULT)
    if (pDesc->Usage == D3D11_USAGE_IMMUTABLE)
      flags.set(TextureAllocationFlag::GpuReadonly);
    if (pDesc->Usage != D3D11_USAGE_DYNAMIC)
      flags.set(TextureAllocationFlag::GpuManaged);
    pool = std::make_unique<DynamicTexturePool2>(texture_.ptr(), flags);
    RotateBuffer(device);
    auto _ = texture_->rename(Rc(allocation));
    D3D11_ASSERT(_.ptr() == nullptr);

    if (pInitialData) {
      D3D11_ASSERT(pInitialData->SysMemPitch == bytes_per_row);
      D3D11_ASSERT(allocation->mappedMemory);
      memcpy(allocation->mappedMemory, pInitialData->pSysMem, bytes_per_image);
    }
  }

  void *
  GetMappedMemory(UINT *pBytesPerRow, UINT *pBytesPerImage) override {
    *pBytesPerRow = bytes_per_row_;
    *pBytesPerImage = bytes_per_image_;
    assert(allocation->mappedMemory);
    return allocation->mappedMemory;
  };

  UINT
  GetSize(UINT *pBytesPerRow, UINT *pBytesPerImage) override {
    *pBytesPerRow = bytes_per_row_;
    *pBytesPerImage = bytes_per_image_;
    return bytes_per_image_;
  };

  dxmt::Rc<dxmt::Buffer> __buffer_dyn() final { return {}; };
  dxmt::Rc<dxmt::Texture> __texture_dyn()final { return texture_; };
  bool __isBuffer_dyn() final { return false; };
  dxmt::Rc<dxmt::BufferAllocation> __bufferAllocated() final { return {}; };
  dxmt::Rc<dxmt::TextureAllocation> __textureAllocated() final { return allocation; };

  Rc<Buffer> buffer() final { return {}; };
  Rc<Texture> texture() final { return this->texture_; };
  BufferSlice bufferSlice() final { return {};}
  Com<IMTLDynamicBuffer> dynamic() final { return this; }
  Com<IMTLD3D11Staging> staging() final { return nullptr; }

  void
  RotateBuffer(MTLD3D11Device *exch) override {
    auto &queue = exch->GetDXMTDevice().queue();
    if(allocation.ptr()) {
      pool->discard(std::move(allocation), queue.CurrentSeqId());
    }
    allocation = pool->allocate(queue.CoherentSeqId());
  };

  D3D11_BIND_FLAG
  GetBindFlag() override {
    return (D3D11_BIND_FLAG)desc.BindFlags;
  }

  HRESULT
  CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc, ID3D11ShaderResourceView1 **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC1 finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc, &finalDesc))) {
      return E_INVALIDARG;
    }
    if (finalDesc.ViewDimension != D3D10_SRV_DIMENSION_TEXTURE2D) {
      ERR("Only 2d texture SRV can be created on dynamic texture");
      return E_FAIL;
    }
    if (finalDesc.Texture2D.MostDetailedMip != 0 || finalDesc.Texture2D.MipLevels != 1) {
      ERR("2d texture SRV must be non-mipmapped on dynamic texture");
      return E_FAIL;
    }
    if (!ppView) {
      return S_FALSE;
    }
    MTL_DXGI_FORMAT_DESC format;
    if (FAILED(MTLQueryDXGIFormat(m_parent->GetMTLDevice(), finalDesc.Format, format))) {
      return E_FAIL;
    }

    auto view_key = texture_->createView(
        {.format = format.PixelFormat,
         .type = MTL::TextureType2D,
         .firstMiplevel = 0,
         .miplevelCount = 1,
         .firstArraySlice = 0,
         .arraySize = 1}
    );

    auto srv = ref(new SRV(&finalDesc, this, m_parent, view_key));

    *ppView = srv;
    return S_OK;
  };
};

#pragma endregion

HRESULT
CreateDynamicTexture2D(
    MTLD3D11Device *pDevice, const D3D11_TEXTURE2D_DESC1 *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData,
    ID3D11Texture2D1 **ppTexture
) {
  MTL_DXGI_FORMAT_DESC format;
  MTLQueryDXGIFormat(pDevice->GetMTLDevice(), pDesc->Format, format);
  if (format.Flag & MTL_DXGI_FORMAT_BC) {
    return E_FAIL;
  }
  if (format.PixelFormat == MTL::PixelFormatInvalid) {
    return E_FAIL;
  }
  Obj<MTL::TextureDescriptor> textureDescriptor;
  D3D11_TEXTURE2D_DESC1 finalDesc;
  if (FAILED(CreateMTLTextureDescriptor(pDevice, pDesc, &finalDesc, &textureDescriptor))) {
    return E_INVALIDARG;
  }
  uint32_t bytesPerRow, bytesPerImage, bufferLen;
  if (FAILED(GetLinearTextureLayout(pDevice, &finalDesc, 0, &bytesPerRow, &bytesPerImage, &bufferLen))) {
    return E_FAIL;
  }

  *ppTexture = reinterpret_cast<ID3D11Texture2D1 *>(
      ref(new DynamicTexture2D(pDesc, std::move(textureDescriptor), pInitialData, bufferLen, bytesPerRow, pDevice))
  );
  return S_OK;
}

} // namespace dxmt
