#pragma once

#include <stddef.h>
#include <vector>
#include "d3d11_3.h"

namespace dxmt {
class ResourceBindingState {
public:
  class SubresourceBindingState {
  private:
    size_t bound_as_srv_count_;
    size_t bound_as_uav_count_;
    size_t bound_as_rtv_count_;
  };

  ResourceBindingState(size_t subresource_count) noexcept;

private:
  std::vector<SubresourceBindingState> subresources_;

  bool bound_as_index_buffer_ = false;
  bool bound_as_depth_stencil_view_ = false;
  size_t bound_as_vertex_buffer_count_ = 0;
  size_t bound_as_stream_out_count_ = 0;
  size_t bound_as_constant_buffer_ = 0;
  size_t referenced_by_view_count_ = 0;
};

class ResourceSubset {
public:
  ResourceSubset(size_t numMips, size_t mostDetailedMipLevel,
                 size_t numArraySlices, size_t firstArraySlice,
                 size_t numPlanes, size_t firstPlane) noexcept;


  ResourceSubset(const D3D11_SHADER_RESOURCE_VIEW_DESC &Desc) noexcept;
  ResourceSubset(const D3D11_UNORDERED_ACCESS_VIEW_DESC &Desc) noexcept;
  ResourceSubset(const D3D11_RENDER_TARGET_VIEW_DESC &Desc) noexcept;

  ResourceSubset(const D3D11_SHADER_RESOURCE_VIEW_DESC1 &Desc) noexcept;
  ResourceSubset(const D3D11_UNORDERED_ACCESS_VIEW_DESC1 &Desc) noexcept;
  ResourceSubset(const D3D11_RENDER_TARGET_VIEW_DESC1 &Desc) noexcept;
  ResourceSubset(const D3D11_DEPTH_STENCIL_VIEW_DESC &Desc) noexcept;



private:
  size_t begin_array_;
  size_t end_array_;
  size_t begin_mip_;
  size_t end_mip_;
  size_t begin_plane_;
  size_t end_plane_;
};
} // namespace dxmt