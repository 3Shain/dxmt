#pragma once
#include "Metal/MTLCommandEncoder.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include <cstdint>
#include "ftl.hpp"

namespace dxmt {

enum DXMT_RESOURCE_RESIDENCY : uint32_t {
  DXMT_RESOURCE_RESIDENCY_NULL = 0,
  DXMT_RESOURCE_RESIDENCY_VERTEX_READ = 1 << 0,
  DXMT_RESOURCE_RESIDENCY_VERTEX_WRITE = 1 << 1,
  DXMT_RESOURCE_RESIDENCY_FRAGMENT_READ = 1 << 2,
  DXMT_RESOURCE_RESIDENCY_FRAGMENT_WRITE = 1 << 3,
  DXMT_RESOURCE_RESIDENCY_OBJECT_READ = 1 << 4,
  DXMT_RESOURCE_RESIDENCY_OBJECT_WRITE = 1 << 5,
  DXMT_RESOURCE_RESIDENCY_MESH_READ = 1 << 6,
  DXMT_RESOURCE_RESIDENCY_MESH_WRITE = 1 << 7,
  DXMT_RESOURCE_RESIDENCY_READ = DXMT_RESOURCE_RESIDENCY_VERTEX_READ | DXMT_RESOURCE_RESIDENCY_FRAGMENT_READ |
                                 DXMT_RESOURCE_RESIDENCY_OBJECT_READ | DXMT_RESOURCE_RESIDENCY_MESH_READ,
  DXMT_RESOURCE_RESIDENCY_WRITE = DXMT_RESOURCE_RESIDENCY_VERTEX_WRITE | DXMT_RESOURCE_RESIDENCY_FRAGMENT_WRITE |
                                  DXMT_RESOURCE_RESIDENCY_OBJECT_WRITE | DXMT_RESOURCE_RESIDENCY_MESH_WRITE,
};

struct DXMT_RESOURCE_RESIDENCY_STATE {
  uint64_t last_encoder_id = 0;
  DXMT_RESOURCE_RESIDENCY last_residency_mask = DXMT_RESOURCE_RESIDENCY_NULL;
};

constexpr bool
CheckResourceResidency(DXMT_RESOURCE_RESIDENCY_STATE &state, uint64_t encoder_id, DXMT_RESOURCE_RESIDENCY &requested) {
  if (encoder_id > state.last_encoder_id) {
    state.last_residency_mask = requested;
    state.last_encoder_id = encoder_id;
    requested = state.last_residency_mask;
    return true;
  }
  if (encoder_id == state.last_encoder_id) {
    if ((state.last_residency_mask & requested) == requested) {
      // it's already resident
      return false;
    }
    state.last_residency_mask = requested | state.last_residency_mask;
    requested = state.last_residency_mask;
    return true;
  }
  // invalid
  return false;
};

constexpr MTL::ResourceUsage
GetUsageFromResidencyMask(DXMT_RESOURCE_RESIDENCY mask) {
  return ((mask & DXMT_RESOURCE_RESIDENCY_READ) ? MTL::ResourceUsageRead : MTL::ResourceUsage(0)) |
         ((mask & DXMT_RESOURCE_RESIDENCY_WRITE) ? MTL::ResourceUsageWrite : MTL::ResourceUsage(0));
}

constexpr MTL::RenderStages
GetStagesFromResidencyMask(DXMT_RESOURCE_RESIDENCY mask) {
  return ((mask & (DXMT_RESOURCE_RESIDENCY_FRAGMENT_READ | DXMT_RESOURCE_RESIDENCY_FRAGMENT_WRITE))
              ? MTL::RenderStageFragment
              : MTL::RenderStages(0)) |
         ((mask & (DXMT_RESOURCE_RESIDENCY_VERTEX_READ | DXMT_RESOURCE_RESIDENCY_VERTEX_WRITE)) ? MTL::RenderStageVertex
                                                                                                :  MTL::RenderStages(0)) |
         ((mask & (DXMT_RESOURCE_RESIDENCY_OBJECT_READ | DXMT_RESOURCE_RESIDENCY_OBJECT_WRITE)) ? MTL::RenderStageObject
                                                                                                : MTL::RenderStages(0)) |
         ((mask & (DXMT_RESOURCE_RESIDENCY_MESH_READ | DXMT_RESOURCE_RESIDENCY_MESH_WRITE)) ? MTL::RenderStageMesh : MTL::RenderStages(0));
}

} // namespace dxmt
