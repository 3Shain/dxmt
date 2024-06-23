#pragma once
#include "Metal/MTLTexture.hpp"
#include "d3d11_1.h"
#include "d3d11_device.hpp"
#include "objc_pointer.hpp"

namespace dxmt {
template <typename TEXTURE_DESC>
void initWithSubresourceData(MTL::Texture *target, const TEXTURE_DESC *desc,
                             const D3D11_SUBRESOURCE_DATA *subresources);
} // namespace dxmt