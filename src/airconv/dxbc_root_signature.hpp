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
#include "dxmt_d3d12.h"

#include <cstring>
#include <vector>
#include <cstdint>

namespace dxmt {

struct RawRootSignatureDesc {
  uint32_t Version;
  uint32_t NumParameters;
  uint32_t RootParametersOffset;
  uint32_t NumStaticSamplers;
  uint32_t StaticSamplersOffset;
  uint32_t Flags;
};

struct RawRootParameter {
  uint32_t ParameterType;
  uint32_t ShaderVisibility;
  uint32_t PayloadOffset;
};

struct RawRootDescriptorTable {
  uint32_t NumDescriptorRanges;
  uint32_t DescriptorRangesOffset;
};

template <typename TO, typename FROM>
void
SetFlag(TO *A, const FROM *B) {}

template <>
inline void
SetFlag(D3D12_ROOT_DESCRIPTOR1 *A, const D3D12_ROOT_DESCRIPTOR *B) {
  A->Flags = D3D12_ROOT_DESCRIPTOR_FLAG_DATA_VOLATILE;
};

template <>
inline void
SetFlag(D3D12_ROOT_DESCRIPTOR1 *A, const D3D12_ROOT_DESCRIPTOR1 *B) {
  A->Flags = B->Flags;
};

template <>
inline void
SetFlag(D3D12_DESCRIPTOR_RANGE1 *A, const D3D12_DESCRIPTOR_RANGE *B) {
  if (B->RangeType != D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
    A->Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
  else
    A->Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
};

template <>
inline void
SetFlag(D3D12_DESCRIPTOR_RANGE1 *A, const D3D12_DESCRIPTOR_RANGE1 *B) {
  A->Flags = B->Flags;
};

template <typename T> struct GetType {
  using ROOT_PARAMETER = std::decay_t<decltype(std::declval<T>().pParameters[0])>;
  using ROOT_DESCRIPTOR = decltype(std::declval<T>().pParameters[0].Descriptor);
  using DESCRIPTOR_RANGE =
      std::decay_t<decltype(std::declval<T>().pParameters[0].DescriptorTable.pDescriptorRanges[0])>;
};

class RootSignatureDeserializer {
public:
  RootSignatureDeserializer() {}

  template <typename T>
  T *
  AllocateBuffer(size_t Count, size_t Size = sizeof(T)) {
    if (Count == 0)
      return nullptr;
    T *ptr = (T *)malloc(Size * Count);
    if (ptr)
      malloc_ptrs_.push_back(ptr);
    return ptr;
  }

  template <typename ROOT_SIGNATURE_DESC>
  HRESULT
  DeserializeVersionedRootSignatureImpl(ROOT_SIGNATURE_DESC &Output) {
    using ROOT_PARAMETER = GetType<ROOT_SIGNATURE_DESC>::ROOT_PARAMETER;
    using ROOT_DESCRIPTOR = GetType<ROOT_SIGNATURE_DESC>::ROOT_DESCRIPTOR;
    using DESCRIPTOR_RANGE = GetType<ROOT_SIGNATURE_DESC>::DESCRIPTOR_RANGE;

    Output.Flags = (D3D12_ROOT_SIGNATURE_FLAGS)raw_root_sig_->Flags;
    /* Root Parameters */
    auto Parameters = (const RawRootParameter *)((char *)raw_root_sig_ + raw_root_sig_->RootParametersOffset);
    auto pParameters = AllocateBuffer<ROOT_PARAMETER>(raw_root_sig_->NumParameters);
    if (raw_root_sig_->NumParameters && !pParameters)
      return E_OUTOFMEMORY;
    for (unsigned i = 0; i < raw_root_sig_->NumParameters; i++) {
      ROOT_PARAMETER &ParameterOut = pParameters[i];
      auto RootParameter = Parameters[i];
      auto ParameterType = RootParameter.ParameterType;
      auto ShaderVisibility = (D3D12_SHADER_VISIBILITY)RootParameter.ShaderVisibility;
      ParameterOut.ParameterType = (D3D12_ROOT_PARAMETER_TYPE)ParameterType;
      ParameterOut.ShaderVisibility = ShaderVisibility;
      switch (ParameterType) {
      case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
        auto Table = (const RawRootDescriptorTable *)((char *)raw_root_sig_ + RootParameter.PayloadOffset);
        ParameterOut.DescriptorTable.NumDescriptorRanges = Table->NumDescriptorRanges;
        auto Ranges = (DESCRIPTOR_RANGE *)((char *)raw_root_sig_ + Table->DescriptorRangesOffset);
        ParameterOut.DescriptorTable.pDescriptorRanges = Ranges;

        break;
      }
      case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS: {
        ParameterOut.Constants = *(D3D12_ROOT_CONSTANTS *)((char *)raw_root_sig_ + RootParameter.PayloadOffset);
        break;
      }
      case D3D12_ROOT_PARAMETER_TYPE_CBV:
      case D3D12_ROOT_PARAMETER_TYPE_SRV:
      case D3D12_ROOT_PARAMETER_TYPE_UAV: {
        ParameterOut.Descriptor = *(ROOT_DESCRIPTOR *)((char *)raw_root_sig_ + RootParameter.PayloadOffset);
        break;
      }
      }
    }
    Output.NumParameters = raw_root_sig_->NumParameters;
    Output.pParameters = pParameters;

    /* Static Samplers */
    Output.NumStaticSamplers = raw_root_sig_->NumStaticSamplers;
    Output.pStaticSamplers =
        Output.NumStaticSamplers
            ? (const D3D12_STATIC_SAMPLER_DESC *)((char *)raw_root_sig_ + raw_root_sig_->StaticSamplersOffset)
            : nullptr;

    return S_OK;
  };

  template <typename ROOT_SIGNATURE_DESC_FROM, typename ROOT_SIGNATURE_DESC_TO>
  HRESULT
  ConvertVersionedRootSignature(ROOT_SIGNATURE_DESC_FROM &From, ROOT_SIGNATURE_DESC_TO &To) {
    using ROOT_PARAMETER_TO = GetType<ROOT_SIGNATURE_DESC_TO>::ROOT_PARAMETER;
    using DESCRIPTOR_RANGE_TO = GetType<ROOT_SIGNATURE_DESC_TO>::DESCRIPTOR_RANGE;

    auto pParametersTo = AllocateBuffer<ROOT_PARAMETER_TO>(From.NumParameters);
    if (From.NumParameters && !pParametersTo)
      return E_OUTOFMEMORY;

    To.Flags = From.Flags;
    To.NumParameters = From.NumParameters;
    To.pParameters = pParametersTo;
    To.NumStaticSamplers = From.NumStaticSamplers;
    To.pStaticSamplers = From.pStaticSamplers;

    for (unsigned i = 0; i < From.NumParameters; i++) {
      const auto &ParameterFrom = From.pParameters[i];
      auto &ParameterTo = pParametersTo[i];
      ParameterTo.ParameterType = ParameterFrom.ParameterType;
      ParameterTo.ShaderVisibility = ParameterFrom.ShaderVisibility;
      switch (ParameterFrom.ParameterType) {
      case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
        ParameterTo.DescriptorTable.NumDescriptorRanges = ParameterFrom.DescriptorTable.NumDescriptorRanges;

        auto pRanges = AllocateBuffer<DESCRIPTOR_RANGE_TO>(ParameterTo.DescriptorTable.NumDescriptorRanges);
        if (!pRanges)
          return E_OUTOFMEMORY;
        ParameterTo.DescriptorTable.pDescriptorRanges = pRanges;

        for (unsigned j = 0; j < ParameterTo.DescriptorTable.NumDescriptorRanges; j++) {
          const auto &RangeFrom = ParameterFrom.DescriptorTable.pDescriptorRanges[j];
          auto &RangeTo = pRanges[j];
          RangeTo.OffsetInDescriptorsFromTableStart = RangeFrom.OffsetInDescriptorsFromTableStart;
          RangeTo.NumDescriptors = RangeFrom.NumDescriptors;
          RangeTo.BaseShaderRegister = RangeFrom.BaseShaderRegister;
          RangeTo.RegisterSpace = RangeFrom.RegisterSpace;
          RangeTo.RangeType = RangeFrom.RangeType;
          SetFlag(&RangeTo, &RangeFrom);
        }

        break;
      }
      case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
        ParameterTo.Constants = ParameterFrom.Constants;
        break;
      case D3D12_ROOT_PARAMETER_TYPE_CBV:
      case D3D12_ROOT_PARAMETER_TYPE_SRV:
      case D3D12_ROOT_PARAMETER_TYPE_UAV:
        SetFlag(&ParameterTo.Descriptor, &ParameterFrom.Descriptor);
        ParameterTo.Descriptor.RegisterSpace = ParameterFrom.Descriptor.RegisterSpace;
        ParameterTo.Descriptor.ShaderRegister = ParameterFrom.Descriptor.ShaderRegister;
        break;
      }
    }

    return S_OK;
  }

  HRESULT
  Deserialize(const void *pBytecode, SIZE_T DataSize) {
    raw_root_sig_ = AllocateBuffer<RawRootSignatureDesc>(1, DataSize);
    if (!raw_root_sig_)
      return E_OUTOFMEMORY;

    memcpy(raw_root_sig_, pBytecode, DataSize);

    desc_1_0_.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
    desc_1_1_.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;

    HRESULT hr;
    switch (raw_root_sig_->Version) {
    case D3D_ROOT_SIGNATURE_VERSION_1_0: {
      hr = DeserializeVersionedRootSignatureImpl(desc_1_0_.Desc_1_0);
      if (FAILED(hr))
        return hr;
      hr = ConvertVersionedRootSignature(desc_1_0_.Desc_1_0, desc_1_1_.Desc_1_1);
      if (FAILED(hr))
        return hr;
      break;
    }
    case D3D_ROOT_SIGNATURE_VERSION_1_1: {
      hr = DeserializeVersionedRootSignatureImpl(desc_1_1_.Desc_1_1);
      if (FAILED(hr))
        return hr;
      hr = ConvertVersionedRootSignature(desc_1_1_.Desc_1_1, desc_1_0_.Desc_1_0);
      if (FAILED(hr))
        return hr;
      break;
    }
    default:
      hr = E_INVALIDARG;
    }

    return hr;
  }

  ~RootSignatureDeserializer() {
    for (auto ptr : malloc_ptrs_) {
      free(ptr);
    }
    malloc_ptrs_.clear();
    raw_root_sig_ = nullptr;
  }

  D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc_1_0_;
  D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc_1_1_;
  RawRootSignatureDesc *raw_root_sig_;

private:
  std::vector<void *> malloc_ptrs_;
};

} // namespace dxmt