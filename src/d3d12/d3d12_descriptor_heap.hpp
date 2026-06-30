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
#include <cstdint>

#if UINTPTR_MAX == 0xffffffffffffffffULL
#define DXMT_USE_EMBEDDED_HEAP_POINTER
#endif

namespace dxmt {

struct EMBEDDED_DESCRIPTOR_HANDLE {
#ifndef DXMT_USE_EMBEDDED_HEAP_POINTER
  SIZE_T Tag        : 5;
  SIZE_T Descriptor : 20;
  SIZE_T Heap       : 7;
#else
  SIZE_T Tag        : 5;
  SIZE_T Descriptor : 20;
  SIZE_T Heap       : 39;

  // assume pointer is 8-byte aligned, providing 3 free bits
  template <typename T>
  T *
  extract() {
    return reinterpret_cast<T *>((Heap << 8) | (Tag << 3));
  }

  EMBEDDED_DESCRIPTOR_HANDLE(const void *heap, SIZE_T index) {
    Heap = (SIZE_T)heap >> 8;
    Tag = (SIZE_T)heap >> 3;
    Descriptor = index;
  }
#endif

  EMBEDDED_DESCRIPTOR_HANDLE(D3D12_CPU_DESCRIPTOR_HANDLE Handle) {
    union {
      D3D12_CPU_DESCRIPTOR_HANDLE d3d12;
      EMBEDDED_DESCRIPTOR_HANDLE impl;
    } _;
    _.d3d12 = Handle;
    *this = _.impl;
  }

  operator D3D12_CPU_DESCRIPTOR_HANDLE() const {
    union {
      D3D12_CPU_DESCRIPTOR_HANDLE d3d12;
      EMBEDDED_DESCRIPTOR_HANDLE impl;
    } _;
    _.impl = *this;
    return _.d3d12;
  };

  EMBEDDED_DESCRIPTOR_HANDLE() = default;
};

static_assert(sizeof(EMBEDDED_DESCRIPTOR_HANDLE) == sizeof(D3D12_CPU_DESCRIPTOR_HANDLE));

class MTLD3D12DescriptorHeap : public ID3D12DescriptorHeap {
public:
};

class MTLD3D12SamplerDescriptorHeap : public ID3D12DescriptorHeap {
public:
};

class MTLD3D12RenderTargetDescriptorHeap : public ID3D12DescriptorHeap {
public:
};

} // namespace dxmt