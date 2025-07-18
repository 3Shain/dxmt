#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "d3d11_texture.hpp"
#include "dxmt_buffer.hpp"
#include "dxmt_dynamic.hpp"
#include "dxmt_staging.hpp"
#include "dxmt_texture.hpp"
#include "d3d11_resource.hpp"
#include "d3d11_enumerable.hpp"

namespace dxmt {

#pragma region DynamicTexture

struct Subresource {
  Rc<Buffer> buffer;
  Rc<DynamicBuffer> dynamic;
  uint32_t bytes_per_row;
  uint32_t bytes_per_depth;
};

template <typename tag_texture>
class DynamicTexture : public TResourceBase<tag_texture, IMTLMinLODClampable> {
private:
  Rc<Texture> underlying_texture_;
  std::vector<Subresource> subresources_;
  float min_lod = 0.0;

  using SRVBase =
      TResourceViewBase<tag_shader_resource_view<DynamicTexture<tag_texture>>>;
  class TextureSRV : public SRVBase {
  private:
    TextureViewKey view_key_;

  public:
    TextureSRV(TextureViewKey view_key,
               const tag_shader_resource_view<>::DESC1 *pDesc,
               DynamicTexture *pResource, MTLD3D11Device *pDevice)
        : SRVBase(pDesc, pResource, pDevice), view_key_(view_key) {}

    Rc<Buffer> buffer() final { return {}; };
    Rc<Texture> texture() final { return this->resource->underlying_texture_; };
    unsigned viewId() final { return view_key_;};
    BufferSlice bufferSlice() final { return {};}
  };

public:
  DynamicTexture(const tag_texture::DESC1 *pDesc, Rc<Texture> &&u_texture, MTLD3D11Device *pDevice, std::vector<Subresource> && subresources) :
      TResourceBase<tag_texture, IMTLMinLODClampable>(*pDesc, pDevice),
      underlying_texture_(std::move(u_texture)),
      subresources_(std::move(subresources)) {}

  Rc<Buffer> buffer() final { return {}; };
  Rc<Texture> texture() final { return this->underlying_texture_; };
  BufferSlice bufferSlice() final { return {};}
  Rc<StagingResource> staging(UINT) final { return nullptr; }
  Rc<DynamicBuffer> dynamicBuffer(UINT*, UINT*) final { return {}; }
  Rc<DynamicLinearTexture> dynamicLinearTexture(UINT*, UINT*) final { return {}; };
  Rc<DynamicBuffer> dynamicTexture(UINT subresource, UINT *bytes_per_row,
                                   UINT *bytes_per_depth) final {
    if (subresource < subresources_.size()) {
      auto &sub = subresources_[subresource];
      *bytes_per_row = sub.bytes_per_row;
      *bytes_per_depth = sub.bytes_per_depth;
      return sub.dynamic;
    }
    return {};
  };

  HRESULT
  STDMETHODCALLTYPE
  CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC1 *pDesc,
                           ID3D11ShaderResourceView1 **ppView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC1 finalDesc;
    if (FAILED(ExtractEntireResourceViewDescription(&this->desc, pDesc,
                                                    &finalDesc))) {
      ERR("DynamicTexture: Failed to create SRV descriptor");
      return E_INVALIDARG;
    }
    TextureViewDescriptor descriptor;
    uint32_t arraySize;
    if constexpr (std::is_same_v<typename tag_texture::DESC1, D3D11_TEXTURE3D_DESC1>) {
      arraySize = this->desc.Depth;
    } else {
      arraySize = this->desc.ArraySize;
    }
    if (FAILED(InitializeAndNormalizeViewDescriptor(
            this->m_parent, this->desc.MipLevels, arraySize, this->underlying_texture_.ptr(), finalDesc, descriptor
        ))) {
      ERR("DynamicTexture: Failed to create texture SRV");
      return E_FAIL;
    }
    if (!ppView) {
      return S_FALSE;
    }
    TextureViewKey key = underlying_texture_->createView(descriptor);
    *ppView = ref(new TextureSRV(key, &finalDesc, this, this->m_parent));
    return S_OK;
  };

  void SetMinLOD(float MinLod) override { min_lod = MinLod; }

  float GetMinLOD() override { return min_lod; }
};

template <typename tag>
HRESULT CreateDynamicTextureInternal(MTLD3D11Device *pDevice,
                                    const typename tag::DESC1 *pDesc,
                                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                                    typename tag::COM_IMPL **ppTexture) {
  WMTTextureInfo info;
  typename tag::DESC1 finalDesc;
  if (FAILED(CreateMTLTextureDescriptor(pDevice, pDesc, &finalDesc, &info))) {
    return E_INVALIDARG;
  }

  std::vector<Subresource> subresources;
  for (auto &sub : EnumerateSubresources(finalDesc)) {
    uint32_t bpr, bpi, buf_len;
    if (FAILED(GetLinearTextureLayout(pDevice, finalDesc, sub.MipLevel, bpr, bpi, buf_len))) {
      return E_FAIL;
    }
    Flags<BufferAllocationFlag> buffer_flags;
    buffer_flags.set(BufferAllocationFlag::CpuWriteCombined, BufferAllocationFlag::SuballocateFromOnePage);
    Subresource subresource;
    subresource.buffer = new Buffer(buf_len, pDevice->GetMTLDevice());
    subresource.buffer->rename(subresource.buffer->allocate(buffer_flags));
    subresource.dynamic = new DynamicBuffer(subresource.buffer.ptr(), buffer_flags);
    subresource.bytes_per_row = bpr;
    subresource.bytes_per_depth = bpi;

    D3D11_ASSERT(subresources.size() == sub.SubresourceId);
    if (pInitialData) {
      // FIXME: should buffer be initialized? in theory they won't be read anyway
    }
    subresources.push_back(std::move(subresource));
  }

  auto texture = Rc<Texture>(new Texture(info, pDevice->GetMTLDevice()));
  Flags<TextureAllocationFlag> flags;
  flags.set(TextureAllocationFlag::GpuManaged);
  if (pInitialData) {
    auto default_allocation = texture->allocate(flags);
    InitializeTextureData(pDevice, default_allocation->texture(), finalDesc, pInitialData);
    texture->rename(std::move(default_allocation));
    *ppTexture =
        reinterpret_cast<typename tag::COM_IMPL *>(
          ref(new DynamicTexture<tag>(&finalDesc, std::move(texture), pDevice, std::move(subresources))));
  } else {
    texture->rename(texture->allocate(flags));
    *ppTexture = reinterpret_cast<typename tag::COM_IMPL *>(
      ref(new DynamicTexture<tag>(&finalDesc, std::move(texture), pDevice, std::move(subresources))));
  }
  return S_OK;
}

HRESULT
CreateDynamicTexture1D(MTLD3D11Device *pDevice,
                      const D3D11_TEXTURE1D_DESC *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                      ID3D11Texture1D **ppTexture) {
  return CreateDynamicTextureInternal<tag_texture_1d>(pDevice, pDesc,
                                                     pInitialData, ppTexture);
}

HRESULT
CreateDynamicTexture2D(MTLD3D11Device *pDevice,
                      const D3D11_TEXTURE2D_DESC1 *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                      ID3D11Texture2D1 **ppTexture) {
  return CreateDynamicTextureInternal<tag_texture_2d>(pDevice, pDesc,
                                                     pInitialData, ppTexture);
}

HRESULT
CreateDynamicTexture3D(MTLD3D11Device *pDevice,
                      const D3D11_TEXTURE3D_DESC1 *pDesc,
                      const D3D11_SUBRESOURCE_DATA *pInitialData,
                      ID3D11Texture3D1 **ppTexture) {
  return CreateDynamicTextureInternal<tag_texture_3d>(pDevice, pDesc,
                                                     pInitialData, ppTexture);
}

#pragma endregion

} // namespace dxmt