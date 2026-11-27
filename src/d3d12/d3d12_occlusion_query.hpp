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

#pragma once
#include "d3d12_device.hpp"

namespace dxmt {

class CommandListOcclusionQueryState {

  struct OcclusionQueryState {
    uint64_t offset_start = 0;
    uint64_t offset_end = 0;
    bool is_resolvable = false;
  };

  std::map<std::pair<MTLD3D12QueryHeap *, UINT>, OcclusionQueryState> queries_;

public:
  bool Begin(MTLD3D12QueryHeap *pHeap, UINT Index, UINT64 Offset);
  bool End(MTLD3D12QueryHeap *pHeap, UINT Index, UINT64 Offset);
  bool GetResolveInfo(MTLD3D12QueryHeap *pHeap, UINT Index, OcclusionQueryResolveInfo &InfoOut);
  bool HasOutstandingQuery();
  bool Flush(std::function<void(OcclusionQueryResolveInfo)> fn);
};
} // namespace dxmt
