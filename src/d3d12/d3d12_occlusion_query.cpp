/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "d3d12_occlusion_query.hpp"

namespace dxmt {

bool
CommandListOcclusionQueryState::Begin(MTLD3D12QueryHeap *pHeap, UINT Index, UINT64 Offset) {

  auto iter = queries_.find({pHeap, Index});
  if (iter != queries_.end()) {
    auto &state = iter->second;
    if (state.is_resolvable) {
      DEBUG("BeginQuery: discarded unresolved data");
      state.is_resolvable = false;
      state.offset_start = Offset;
      return true;
    }
    // duplicated Begin()
    return false;
  }
  OcclusionQueryState state;
  state.offset_start = Offset;
  state.offset_end = 0;
  state.is_resolvable = false;
  queries_.insert({{pHeap, Index}, state});
  return true;
}

bool
CommandListOcclusionQueryState::End(MTLD3D12QueryHeap *pHeap, UINT Index, UINT64 Offset) {
  auto iter = queries_.find({pHeap, Index});
  if (iter == queries_.end()) {
    // no Begin()
    return false;
  }
  auto &state = iter->second;
  state.offset_end = Offset;
  state.is_resolvable = true;
  return true;
}

bool
CommandListOcclusionQueryState::GetResolveInfo(
    MTLD3D12QueryHeap *pHeap, UINT Index, OcclusionQueryResolveInfo &InfoOut
) {
  auto iter = queries_.find({pHeap, Index});
  if (iter == queries_.end()) {
    InfoOut.visibility_offset_start = ~0ull;
    InfoOut.visibility_offset_end = ~0ull;
    InfoOut.resolve_dst = 0;
    return true;
  }
  auto &state = iter->second;
  if (state.is_resolvable) {
    InfoOut.visibility_offset_start = state.offset_start;
    InfoOut.visibility_offset_end = state.offset_end;
    InfoOut.resolve_dst = pHeap->query_heap_address + Index * sizeof(uint64_t) * 2;
    queries_.erase(iter);
    return true;
  }
  // no End()
  return false;
}

bool
CommandListOcclusionQueryState::HasOutstandingQuery() {
  return std::any_of(queries_.rbegin(), queries_.rend(), [](auto pair) { return !pair.second.is_resolvable; });
}

bool
CommandListOcclusionQueryState::Flush(std::function<void(OcclusionQueryResolveInfo)> fn) {
  std::erase_if(queries_, [fn = std::move(fn)](auto pair) {
    auto &[heap, index] = pair.first;
    auto &state = pair.second;
    if (state.is_resolvable) {
      OcclusionQueryResolveInfo InfoOut;
      InfoOut.visibility_offset_start = state.offset_start;
      InfoOut.visibility_offset_end = state.offset_end;
      InfoOut.resolve_dst = heap->query_heap_address + index * sizeof(uint64_t) * 2;
      fn(InfoOut);
      return true;
    }
    return false;
  });
  return queries_.size() == 0;
}

} // namespace dxmt