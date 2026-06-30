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

constexpr auto kCPUHeapSize = 0x400000u;
constexpr auto kGPUHeapSize = 0x400000u;

inline std::size_t
align_forward_adjustment(const void *const ptr, const std::size_t &alignment) noexcept {
  const auto iptr = reinterpret_cast<std::uintptr_t>(ptr);
  const auto aligned = (iptr - 1u + alignment) & -alignment;
  return aligned - iptr;
}

inline void *
ptr_add(const void *const p, const std::uintptr_t &amount) noexcept {
  return reinterpret_cast<void *>(reinterpret_cast<std::uintptr_t>(p) + amount);
}

class MTLD3D12CommandAllocatorImpl : public MTLD3D12Pageable<MTLD3D12CommandAllocator> {
  friend class MTLD3D12GraphicsCommandListImpl;

  D3D12_COMMAND_LIST_TYPE type_;
  
  void *cpu_heap_ = nullptr;
  size_t cpu_heap_offset_;

  WMT::Reference<WMT::Buffer> gpu_heap_buffer_;
  void *gpu_heap_ = nullptr;
  size_t gpu_heap_offset_;
  uint64_t gpu_heap_buffer_address_;

  EncoderData *encoder_last;
  EncoderData *encoder_current;
  size_t encoder_count_;

  small_vector<EncoderData, 64> encoder_lists_;

public:
  MTLD3D12CommandAllocatorImpl(MTLD3D12Device *pDevice, D3D12_COMMAND_LIST_TYPE Type);

  ~MTLD3D12CommandAllocatorImpl() {
    free(cpu_heap_);
    cpu_heap_ = nullptr;
    gpu_heap_buffer_ = {};
    free(gpu_heap_);
    gpu_heap_ = nullptr;
  }

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

  void
  InvalidateCurrentPass() {
    if (!encoder_current)
      return;
    encoder_last->next = encoder_current;
    encoder_last = encoder_current;

    encoder_current = nullptr;
    encoder_count_++;
  }

  HRESULT
  StartRecord(EncoderData **pStartEncoder) {
    if (encoder_last)
      return E_INVALIDARG;
    *pStartEncoder = encoder_last = &encoder_lists_.emplace_back(EncoderType::Null, nullptr);
    return S_OK;
  }

  HRESULT
  EndRecord(size_t *pEncoderCount) {
    if (!encoder_last)
      return E_FAIL;
    if (encoder_current)
      InvalidateCurrentPass();
    encoder_last = nullptr;
    *pEncoderCount = encoder_count_;
    encoder_count_ = 0;
    return S_OK;
  }

  void *
  AllocateCPUHeap(size_t Length, size_t Alignment) {
    std::size_t adjustment = align_forward_adjustment((void *)cpu_heap_offset_, Alignment);
    auto aligned = cpu_heap_offset_ + adjustment;
    cpu_heap_offset_ = aligned + Length;
    assert(cpu_heap_offset_ < kCPUHeapSize);
    return ptr_add(cpu_heap_, aligned);
  }

  template <typename T>
  T *
  AllocatePass() {
    auto p = (new (AllocateCPUHeap(sizeof(T), alignof(T))) T());
    encoder_current = p;
    return p;
  };

  template <typename cmd_struct>
  cmd_struct &
  EncodeRenderCommand() {
    assert(encoder_current->type == EncoderType::Render);
    auto encoder = static_cast<RenderEncoderData *>(encoder_current);
    auto storage = (cmd_struct *)AllocateCPUHeap(sizeof(cmd_struct), 16);
    encoder->cmd_tail->next.set(storage);
    encoder->cmd_tail = (wmtcmd_base *)storage;
    storage->next.set(nullptr);
    return *storage;
  }

  std::tuple<void *, size_t>
  AllocateGPUHeap(size_t Length, size_t Alignment) {
    if (!Length)
      return {nullptr, 0};
    std::size_t adjustment = align_forward_adjustment((void *)gpu_heap_offset_, Alignment);
    auto aligned = gpu_heap_offset_ + adjustment;
    gpu_heap_offset_ = aligned + Length;
    assert(gpu_heap_offset_ < kGPUHeapSize);
    return {ptr_add(gpu_heap_, aligned), aligned};
  }
};

} // namespace dxmt