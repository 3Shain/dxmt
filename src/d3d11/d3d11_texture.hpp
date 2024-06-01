#pragma once
#include "Metal/MTLTexture.hpp"
#include "d3d11_1.h"
#include "d3d11_device.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

template <typename VIEW_DESC>
MTL::Texture *newTextureView(IMTLD3D11Device *pDevice, MTL::Texture *source,
                             const VIEW_DESC *s);

template <typename TEXTURE_DESC>
void initWithSubresourceData(MTL::Texture *target, const TEXTURE_DESC *desc,
                             const D3D11_SUBRESOURCE_DATA *subresources);

template <typename TEXTURE_DESC>
Obj<MTL::TextureDescriptor> getTextureDescriptor(IMTLD3D11Device *pDevice,
                                                 const TEXTURE_DESC *pDesc);
} // namespace dxmt