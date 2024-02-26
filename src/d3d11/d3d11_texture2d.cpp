#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_texture.hpp"

#include "d3d11_view.hpp"

#include <tuple>
#include <utility>

namespace dxmt {

StagingTexture2D::StagingTexture2D(IMTLD3D11Device *pDevice, Data,
                                   const Description *desc,
                                   const D3D11_SUBRESOURCE_DATA *pInit) {
  auto tex_desc = TranslateTextureDescriptor(desc);
  staging_texture_ =
      transfer(pDevice->GetMTLDevice()->newTexture(tex_desc.ptr()));
  if (pInit != nullptr) {
    initWithSubresourceData(staging_texture_.ptr(), desc, pInit);
  }
}

ImmutableTexture2D::ImmutableTexture2D(IMTLD3D11Device *pDevice, Data data,
                                       const Description *desc,
                                       const D3D11_SUBRESOURCE_DATA *pInit) {
  auto descriptor = TranslateTextureDescriptor(desc);
  texture_ = transfer(pDevice->GetMTLDevice()->newTexture(descriptor.ptr()));
  descriptor->setUsage(0);
  descriptor->setStorageMode(MTL::StorageModeShared);
  descriptor->setHazardTrackingMode(MTL::HazardTrackingModeUntracked);
  descriptor->setCpuCacheMode(MTL::CPUCacheModeWriteCombined);
  staging_ = transfer(pDevice->GetMTLDevice()->newTexture(descriptor.ptr()));
  initWithSubresourceData(staging_.ptr(), desc, pInit);
}

HRESULT ImmutableTexture2D::CreateShaderResourceView(
    const D3D11_SHADER_RESOURCE_VIEW_DESC *desc,
    ID3D11ShaderResourceView **ppView) {

  D3D11_SHADER_RESOURCE_VIEW_DESC true_desc;

  if (!desc) {
    true_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    // TODO: 2DMS 2DARRAY 2DMSARRAY..
    true_desc.Texture2D.MipLevels =
        (uint32_t)this->texture_->mipmapLevelCount();
    true_desc.Texture2D.MostDetailedMip = 0;
  } else {

    if (desc->ViewDimension != D3D11_SRV_DIMENSION_TEXTURE2D) {
      // TODO: 2DMS 2DARRAY 2DMSARRAY..
      return E_INVALIDARG;
    }
    true_desc = *desc;
  }
  auto textureView = transfer(newTextureView<D3D11_TEX2D_SRV>(
      this->texture_.ptr(), this->texture_->pixelFormat(),
      &true_desc.Texture2D));
  *ppView =
      ref(new TMTLD3D11ShaderResourceView<ImmutableTexture2D, InitializeView>(
          __device__, __resource__, &true_desc,
          [view = std::move(textureView), this](DXMTCommandStream *cs) {
            this->EnsureInitialized(cs);
            return view.ptr();
          }));
  return S_OK;
}

DefaultTexture2D::DefaultTexture2D(IMTLD3D11Device *pDevice, Data data,
                                   const Description *desc,
                                   const D3D11_SUBRESOURCE_DATA *pInit) {
  auto descriptor = TranslateTextureDescriptor(desc);
  texture_ = transfer(pDevice->GetMTLDevice()->newTexture(descriptor.ptr()));
  descriptor->setUsage(0);
  descriptor->setStorageMode(MTL::StorageModeShared);
  descriptor->setHazardTrackingMode(MTL::HazardTrackingModeUntracked);
  descriptor->setCpuCacheMode(MTL::CPUCacheModeWriteCombined);
  staging_ = transfer(pDevice->GetMTLDevice()->newTexture(descriptor.ptr()));
  initWithSubresourceData(staging_.ptr(), desc, pInit);
}

HRESULT DefaultTexture2D::CreateDepthStencilView(
    const D3D11_DEPTH_STENCIL_VIEW_DESC *desc,
    ID3D11DepthStencilView **ppView) {

  D3D11_DEPTH_STENCIL_VIEW_DESC true_desc;

  if (!desc) {
    true_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    // TODO: 2DMS 2DARRAY 2DMSARRAY..
    true_desc.Texture2D.MipSlice = 0;
    true_desc.Flags = 0;
  } else {

    if (desc->ViewDimension != D3D11_DSV_DIMENSION_TEXTURE2D) {
      // TODO: 2DMS 2DARRAY 2DMSARRAY..
      return E_INVALIDARG;
    }
    true_desc = *desc;
  }
  auto textureView = transfer(newTextureView<D3D11_TEX2D_DSV>(
      this->texture_.ptr(), this->texture_->pixelFormat(),
      &true_desc.Texture2D));
  *ppView = ref(new TMTLD3D11DepthStencilView<
                DefaultTexture2D, std::tuple<InitializeView, MTL::PixelFormat>>(
      __device__, __resource__, &true_desc,
      std::tuple<InitializeView, MTL::PixelFormat>(
          [view = textureView, this](DXMTCommandStream *cs) {
            this->EnsureInitialized(cs);
            return view.ptr();
          },
          textureView->pixelFormat())));
  return S_OK;
}

template <>
ShaderResourceBinding *
TMTLD3D11ShaderResourceView<ImmutableTexture2D, InitializeView>::Pin(
    DXMTCommandStream *cs) {
  return new SimpleLazyTextureShaderResourceBinding(
      [c = data_(cs)]() { return c; });
}

template <>
DepthStencilBinding *TMTLD3D11DepthStencilView<
    DefaultTexture2D,
    std::tuple<InitializeView, MTL::PixelFormat>>::Pin(DXMTCommandStream *cs) {
  auto &w = std::get<0>(data_);
  // return new SimpleLazyTextureShaderResourceBinding(
  //     [c = data_(cs)]() { return c; });
  auto texture = w(cs);
  return new SimpleLazyTextureDepthStencilBinding(
      [texture]() { return texture; }, 0, 0,
      /** todo: */ desc_.Flags & D3D11_DSV_READ_ONLY_DEPTH,
      desc_.Flags & D3D11_DSV_READ_ONLY_STENCIL);
};

template <>
MTL::PixelFormat TMTLD3D11DepthStencilView<
    DefaultTexture2D,
    std::tuple<InitializeView, MTL::PixelFormat>>::GetPixelFormat() {
  return std::get<1>(data_);
};

}; // namespace dxmt