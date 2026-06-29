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

#include "d3d12_pageable.hpp"

namespace dxmt {

class MTLD3D12CommandAllocatorImpl : public MTLD3D12Pageable<MTLD3D12CommandAllocator> {
  friend class MTLD3D12GraphicsCommandListImpl;

  D3D12_COMMAND_LIST_TYPE type_;

public:
  MTLD3D12CommandAllocatorImpl(MTLD3D12Device *pDevice, D3D12_COMMAND_LIST_TYPE Type);

  ~MTLD3D12CommandAllocatorImpl() {}

  HRESULT
  Initialize();

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject);

  HRESULT STDMETHODCALLTYPE Reset();

  HRESULT STDMETHODCALLTYPE CreateCommandList(
      UINT NodeMask, D3D12_COMMAND_LIST_TYPE Type, ID3D12PipelineState *pInitialPipelineState, REFIID riid,
      void **ppCommandList
  );
};

} // namespace dxmt