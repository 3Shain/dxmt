#pragma once
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLTexture.hpp"
#include "d3d11_1.h"
#include "objc_pointer.hpp"

namespace dxmt {

template <typename VIEW_DESC>
MTL::Texture *newTextureView(MTL::Texture *source, MTL::PixelFormat newFormat,
                             const VIEW_DESC *s);

template <typename TEXTURE_DESC>
void initWithSubresourceData(MTL::Texture *target, const TEXTURE_DESC *desc,
                             const D3D11_SUBRESOURCE_DATA *subresources);\

template <typename TEXTURE_DESC>
Obj<MTL::TextureDescriptor> getTextureDescriptor(const TEXTURE_DESC* desc);

} // namespace dxmt