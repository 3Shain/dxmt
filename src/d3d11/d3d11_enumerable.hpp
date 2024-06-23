#pragma once
#include "dxmt_names.hpp"
#include "tl/generator.hpp"

namespace dxmt {

using namespace ::tl;

struct SUBRESOURCE_ENTRY {
  uint32_t SubresourceId;
  uint32_t MipLevel;
  uint32_t ArraySlice;
};

generator<SUBRESOURCE_ENTRY>
EnumerateSubresources(const D3D11_TEXTURE1D_DESC &Desc);
generator<SUBRESOURCE_ENTRY>
EnumerateSubresources(const D3D11_TEXTURE2D_DESC &Desc);
generator<SUBRESOURCE_ENTRY>
EnumerateSubresources(const D3D11_TEXTURE3D_DESC &Desc);

}
; // namespace dxmt