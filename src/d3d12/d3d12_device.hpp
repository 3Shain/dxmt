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
#include "d3d12.h"
#include "dxgi1_2.h"
#include "dxgi_interfaces.h"
#include "airconv_public.h"
#include "log/log.hpp"

#define IMPLEMENT_ME                                                                                                   \
  do {                                                                                                                 \
    Logger::err(str::format(__FILE__, ":", __FUNCTION__, "(", __LINE__, ") is not implemented."));                     \
    abort();                                                                                                           \
    __builtin_unreachable();                                                                                           \
  } while (0);

namespace dxmt {

class MTLD3D12Device : public ID3D12Device1 {
public:
};

HRESULT CreateD3D12Device(IMTLDXGIAdapter *adapter, REFIID riid, void **ppDevice);

} // namespace dxmt