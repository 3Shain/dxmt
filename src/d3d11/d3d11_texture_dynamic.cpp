#include "com/com_pointer.hpp"
#include "d3d11_device.hpp"
#include "d3d11_texture.hpp"
#include "dxmt_dynamic.hpp"
#include "dxmt_staging.hpp"
#include "dxmt_texture.hpp"
#include "d3d11_resource.hpp"

namespace dxmt {

#pragma region DynamicTexture

template <typename tag_texture>
class DynamicTexture : public TResourceBase<tag_texture, IMTLMinLODClampable> {
private:
  Rc<Texture> underlying_texture_;
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
  DynamicTexture(const tag_texture::DESC1 *pDesc, Rc<Texture> &&u_texture, MTLD3D11Device *pDevice) :
      TResourceBase<tag_texture, IMTLMinLODClampable>(*pDesc, pDevice),
      underlying_texture_(std::move(u_texture)) {}

  Rc<Buffer> buffer() final { return {}; };
  Rc<Texture> texture() final { return this->underlying_texture_; };
  BufferSlice bufferSlice() final { return {};}
  Rc<StagingResource> staging(UINT) final { return nullptr; }
  Rc<DynamicBuffer> dynamicBuffer(UINT*, UINT*) final { return {}; }
  Rc<DynamicLinearTexture> dynamicLinearTexture(UINT*, UINT*) final { return {}; };

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
  auto texture = Rc<Texture>(new Texture(info, pDevice->GetMTLDevice()));
  Flags<TextureAllocationFlag> flags;
  flags.set(TextureAllocationFlag::GpuManaged);
  if (finalDesc.Usage == D3D11_USAGE_IMMUTABLE)
    flags.set(TextureAllocationFlag::GpuReadonly);
  if (pInitialData) {
    auto default_allocation = texture->allocate(flags);
    InitializeTextureData(pDevice, default_allocation->texture(), finalDesc, pInitialData);
    texture->rename(std::move(default_allocation));
    *ppTexture =
        reinterpret_cast<typename tag::COM_IMPL *>(
          ref(new DynamicTexture<tag>(&finalDesc, std::move(texture), pDevice)));
  } else {
    texture->rename(texture->allocate(flags));
    *ppTexture = reinterpret_cast<typename tag::COM_IMPL *>(
      ref(new DynamicTexture<tag>(&finalDesc, std::move(texture), pDevice)));
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