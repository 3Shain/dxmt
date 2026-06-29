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
#include "d3d12_device_child.hpp"

namespace dxmt {
template <typename... Base> class MTLD3D12Pageable : public MTLD3D12DeviceChild<Base...> {
public:
  MTLD3D12Pageable(MTLD3D12Device *pDevice) : MTLD3D12DeviceChild<Base...>(pDevice) {}
};

using MTLD3D12PageableCommon = MTLD3D12Pageable<ID3D12Pageable>;

}; // namespace dxmt