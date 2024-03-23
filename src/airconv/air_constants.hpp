#include "enum.h"
#include <cstdint>
#pragma once

namespace dxmt::air {

BETTER_ENUM(EType, uint32_t, uint = 0x21, uint2 = 0x22, uint3 = 0x23,
            uint4 = 0x24, _bool = 0x35, _float = 0x03, float2 = 0x04,
            float3 = 0x05, float4 = 0x06, sampler = 0x3b,
            texture = 0x3a /* considerred opaque pointer */, ptr = 0x3c)

inline const char *getAirTypeName(EType type) {
  auto str = type._to_string();
  if (str[0] == '_') {
    return &str[1];
  }
  return str;
}

BETTER_ENUM(EInput, uint32_t, amplification_count, amplification_id,
            barycentric_coord, render_target, front_facing, point_coord,
            position, primitive_id, render_target_array_index, sample_id,
            sample_mask, thread_index_in_quadgroup, threads_per_simdgroup,
            color /* ? programmable blending */, thread_index_in_simdgroup,
            viewport_array_index, base_vertex, base_instance, instance_id,
            vertex_id,
            fragment_input /* user defined fragment input [[user(...)]] */,
            vertex_input /* user defined vertex input [[attribute(i)]] */,
            /* kernel function attributes */
            dispatch_quadgroups_per_threadgroup,
            dispatch_simdgroups_per_threadgroup,
            dispatch_threads_per_threadgroup, stage_in_grid_origin,
            stage_in_grid_size, quadgroup_index_in_threadgroup,
            simdgroup_index_in_threadgroup, simdgroups_per_threadgroup,
            thread_index_in_threadgroup, thread_position_in_grid,
            thread_position_in_threadgroup, threadgroup_position_in_grid,
            threadgroups_per_grid, threads_per_grid,
            //   uint threads_per_simdgroup [[threads_per_simdgroup]], //
            //   samething as thread_execution_width
            threads_per_threadgroup)

BETTER_ENUM(EOutput, uint32_t, render_target, depth, sample_mask, stencil,
            clip_distance, position, point_size, render_target_array_index,
            viewport_array_index,
            vertex_output /* user defined vertex output [[user(...)]] */)

BETTER_ENUM(EDepthArgument, uint32_t, any, greater, less)

} // namespace dxmt::air