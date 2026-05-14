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

#include "DXBCParser/BlobContainer.h"
#include "DXBCParser/DXBCUtils.h"
#include "com/com_object.hpp"
#include "com/com_pointer.hpp"
#include "util_math.hpp"
#include "util_md5.hpp"
#include <cstring>
#include <vector>
#include "../d3d10/d3d10_blob.hpp"
#include "../airconv/dxbc_root_signature.hpp"

namespace dxmt {

#pragma region Serialization

template <typename ROOT_SIGNATURE_DESC>
HRESULT
SerializeVersionedRootSignatureImpl(
    const ROOT_SIGNATURE_DESC *pDesc, D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob
) {
  using ROOT_PARAMETER = GetType<ROOT_SIGNATURE_DESC>::ROOT_PARAMETER;
  using ROOT_DESCRIPTOR = GetType<ROOT_SIGNATURE_DESC>::ROOT_DESCRIPTOR;
  using DESCRIPTOR_RANGE = GetType<ROOT_SIGNATURE_DESC>::DESCRIPTOR_RANGE;

  struct segment {
    size_t offset;
    size_t size_aligned;
    void *ptr;
    segment(size_t offset, size_t size) {
      this->offset = offset;
      size_aligned = align(size, 4);
      ptr = malloc(size_aligned);
      memset(ptr, 0, size_aligned);
    }
    ~segment() {
      free(ptr);
    };
    segment(const segment &copy) = delete;
    segment(segment &&move) {
      ptr = move.ptr;
      size_aligned = move.size_aligned;
      offset = move.offset;
      move.ptr = nullptr;
      move.size_aligned = 0;
      move.offset = 0;
    };
  };
  std::vector<segment> segments;
  size_t total_offset = 0;

  auto add_segment = [&]<typename TYPE>(size_t count, TYPE **ptr) -> size_t {
    if (count == 0) {
      *ptr = nullptr;
      return total_offset;
    }
    size_t size = sizeof(TYPE) * count;
    segments.emplace_back(total_offset, size);
    total_offset += size;
    *ptr = reinterpret_cast<TYPE *>(segments.back().ptr);
    return segments.back().offset;
  };

  RawRootSignatureDesc *RootSig = nullptr;

  add_segment(1, &RootSig);
  if (!RootSig)
    return E_OUTOFMEMORY;

  RootSig->Version = (uint32_t)Version;
  RootSig->Flags = (uint32_t)pDesc->Flags;
  RootSig->NumParameters = pDesc->NumParameters;
  RootSig->NumStaticSamplers = pDesc->NumStaticSamplers;

  /* Root Parameters */

  RawRootParameter *Parameters = nullptr;
  RootSig->RootParametersOffset = add_segment(pDesc->NumParameters, &Parameters);
  if (pDesc->NumParameters && !Parameters)
    return E_OUTOFMEMORY;

  for (UINT i = 0; i < pDesc->NumParameters; i++) {
    const ROOT_PARAMETER *RootParameter = &pDesc->pParameters[i];
    RawRootParameter *Out = &Parameters[i];
    Out->ParameterType = (uint32_t)RootParameter->ParameterType;
    Out->ShaderVisibility = (uint32_t)RootParameter->ShaderVisibility;
    switch (RootParameter->ParameterType) {
    case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE: {
      RawRootDescriptorTable *OutTable = nullptr;
      Out->PayloadOffset = add_segment(1, &OutTable);
      if (!OutTable)
        return E_OUTOFMEMORY;
      OutTable->NumDescriptorRanges = RootParameter->DescriptorTable.NumDescriptorRanges;
      DESCRIPTOR_RANGE *OutRanges = nullptr;
      OutTable->DescriptorRangesOffset = add_segment(RootParameter->DescriptorTable.NumDescriptorRanges, &OutRanges);
      if (OutTable->NumDescriptorRanges && !OutRanges)
        return E_OUTOFMEMORY;
      for (UINT j = 0; j < RootParameter->DescriptorTable.NumDescriptorRanges; j++) {
        const DESCRIPTOR_RANGE *Range = &RootParameter->DescriptorTable.pDescriptorRanges[j];
        DESCRIPTOR_RANGE *OutRange = &OutRanges[j];
        OutRange->RangeType = Range->RangeType;
        OutRange->NumDescriptors = Range->NumDescriptors;
        OutRange->BaseShaderRegister = Range->BaseShaderRegister;
        OutRange->RegisterSpace = Range->RegisterSpace;
        OutRange->OffsetInDescriptorsFromTableStart = Range->OffsetInDescriptorsFromTableStart;
        SetFlag(OutRange, Range);
      }
      break;
    }
    case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS: {
      D3D12_ROOT_CONSTANTS *OutConstants = nullptr;
      Out->PayloadOffset = add_segment(1, &OutConstants);
      if (!OutConstants)
        return E_OUTOFMEMORY;
      *OutConstants = RootParameter->Constants;
      break;
    }
    case D3D12_ROOT_PARAMETER_TYPE_CBV:
    case D3D12_ROOT_PARAMETER_TYPE_SRV:
    case D3D12_ROOT_PARAMETER_TYPE_UAV: {
      ROOT_DESCRIPTOR *OutDescriptor = nullptr;
      Out->PayloadOffset = add_segment(1, &OutDescriptor);
      if (!OutDescriptor)
        return E_OUTOFMEMORY;
      OutDescriptor->ShaderRegister = RootParameter->Descriptor.ShaderRegister;
      OutDescriptor->RegisterSpace = RootParameter->Descriptor.RegisterSpace;
      SetFlag(OutDescriptor, &RootParameter->Descriptor);
      break;
    }
    }
  }

  /* Static Samplers */

  D3D12_STATIC_SAMPLER_DESC *StaticSamplers = nullptr;
  RootSig->StaticSamplersOffset = add_segment(pDesc->NumStaticSamplers, &StaticSamplers);
  if (pDesc->NumStaticSamplers) {
    if (!StaticSamplers)
      return E_OUTOFMEMORY;
    memcpy(StaticSamplers, pDesc->pStaticSamplers, sizeof(D3D12_STATIC_SAMPLER_DESC) * pDesc->NumStaticSamplers);
  }

  struct DXBC_ROOT_SIGNATURE_BLOB_HEADER {
    microsoft::DXBCHeader DXBCHeader;
    UINT RTSOBlobOffset;
    microsoft::DXBCBlobHeader RTSOBlobHeader;
  };

  auto TotalSize = sizeof(DXBC_ROOT_SIGNATURE_BLOB_HEADER) + total_offset;

  ID3D10Blob *blob = nullptr;
  HRESULT hr = CreateBlobFromMalloc(TotalSize, &blob);
  if (FAILED(hr))
    return hr;
  char *OutBlob = reinterpret_cast<char *>(blob->GetBufferPointer());

  DXBC_ROOT_SIGNATURE_BLOB_HEADER *pHeader = (DXBC_ROOT_SIGNATURE_BLOB_HEADER *)OutBlob;
  pHeader->DXBCHeader.DXBCHeaderFourCC = microsoft::DXBC_FOURCC_NAME;
  pHeader->DXBCHeader.BlobCount = 1;
  pHeader->DXBCHeader.Version = {1, 0};
  pHeader->DXBCHeader.ContainerSizeInBytes = TotalSize;
  pHeader->DXBCHeader.Hash = {};

  pHeader->RTSOBlobOffset = sizeof(DXBC_ROOT_SIGNATURE_BLOB_HEADER) - sizeof(microsoft::DXBCBlobHeader);
  pHeader->RTSOBlobHeader.BlobFourCC = microsoft::DXBC_RootSignature;
  pHeader->RTSOBlobHeader.BlobSize = total_offset;

  for (auto &seg : segments) {
    memcpy(OutBlob + sizeof(DXBC_ROOT_SIGNATURE_BLOB_HEADER) + seg.offset, seg.ptr, seg.size_aligned);
  }

  auto hash = md5::hashDxbcBinary(OutBlob, TotalSize);
  memcpy(pHeader->DXBCHeader.Hash.Digest, hash.data.data(), sizeof(md5::Digest));

  *ppBlob = blob;
  return hr;
};

extern "C" HRESULT WINAPI
D3D12SerializeVersionedRootSignature(
    const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *pRootSignatureDesc, ID3DBlob **ppBlob, ID3DBlob **ppErrorBlob
) {
  // InitReturnPtr(ppBlob); // deliberately, don't touch it
  InitReturnPtr(ppErrorBlob);

  if (!ppBlob)
    return E_INVALIDARG;

  switch (pRootSignatureDesc->Version) {
  default:
    break;
  case D3D_ROOT_SIGNATURE_VERSION_1:
    return SerializeVersionedRootSignatureImpl<D3D12_ROOT_SIGNATURE_DESC>(
        &pRootSignatureDesc->Desc_1_0, D3D_ROOT_SIGNATURE_VERSION_1, ppBlob, ppErrorBlob
    );
  case D3D_ROOT_SIGNATURE_VERSION_1_1:
    return SerializeVersionedRootSignatureImpl<D3D12_ROOT_SIGNATURE_DESC1>(
        &pRootSignatureDesc->Desc_1_1, D3D_ROOT_SIGNATURE_VERSION_1_1, ppBlob, ppErrorBlob
    );
  }

  return E_INVALIDARG;
}

extern "C" HRESULT WINAPI
D3D12SerializeRootSignature(
    const D3D12_ROOT_SIGNATURE_DESC *pRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION Version, ID3DBlob **ppBlob,
    ID3DBlob **ppErrorBlob
) {
  // InitReturnPtr(ppBlob); // deliberately, don't touch it
  InitReturnPtr(ppErrorBlob);

  if (!ppBlob || Version != D3D_ROOT_SIGNATURE_VERSION_1_0)
    return E_INVALIDARG;

  D3D12_VERSIONED_ROOT_SIGNATURE_DESC versioned_desc;
  versioned_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
  versioned_desc.Desc_1_0 = *pRootSignatureDesc;
  return D3D12SerializeVersionedRootSignature(&versioned_desc, ppBlob, ppErrorBlob);
}

#pragma endregion
#pragma region Deserialization

template <typename Base> class MTLD3D12RootSignatureDeserializer : public ComObject<Base> {
public:
  virtual HRESULT STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    if (riid == __uuidof(Base)) {
      *ppvObject = ref_and_cast<Base>(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  virtual const D3D12_ROOT_SIGNATURE_DESC *STDMETHODCALLTYPE
  GetRootSignatureDesc() {
    return &impl_.desc_1_0_.Desc_1_0;
  }

  virtual HRESULT STDMETHODCALLTYPE
  GetRootSignatureDescAtVersion(
      D3D_ROOT_SIGNATURE_VERSION Version, const D3D12_VERSIONED_ROOT_SIGNATURE_DESC **ppDesc
  ) {
    switch (Version) {
    case D3D_ROOT_SIGNATURE_VERSION_1_0:
      *ppDesc = &impl_.desc_1_0_;
      break;
    case D3D_ROOT_SIGNATURE_VERSION_1_1:
      *ppDesc = &impl_.desc_1_1_;
      break;
    default:
      return E_INVALIDARG;
    }
    return S_OK;
  }

  virtual const D3D12_VERSIONED_ROOT_SIGNATURE_DESC *STDMETHODCALLTYPE
  GetUnconvertedRootSignatureDesc() {
    switch (impl_.raw_root_sig_->Version) {
    case D3D_ROOT_SIGNATURE_VERSION_1_0:
      return &impl_.desc_1_0_;
    default:
      return &impl_.desc_1_1_;
    }
  }

  MTLD3D12RootSignatureDeserializer() {}

  HRESULT
  Deserialize(const void *pBytecode, SIZE_T DataSize) {

    return impl_.Deserialize(pBytecode, DataSize);
  }

private:
  RootSignatureDeserializer impl_;
};

extern "C" HRESULT WINAPI
D3D12CreateVersionedRootSignatureDeserializer(
    const void *pBytecode, SIZE_T DataSize, REFIID iid, void **ppDeserializer
) {
  InitReturnPtr(ppDeserializer);

  const void *pRawRootSig;
  UINT RawRootSigSize;
  HRESULT hr = microsoft::DXBCGetRootSignature(pBytecode, &pRawRootSig, &RawRootSigSize);
  if (FAILED(hr))
    return hr;

  auto deserializer = Com(new MTLD3D12RootSignatureDeserializer<ID3D12VersionedRootSignatureDeserializer>());
  hr = deserializer->Deserialize(pRawRootSig, RawRootSigSize);
  if (FAILED(hr))
    return hr;
  return deserializer->QueryInterface(iid, ppDeserializer);
}

extern "C" HRESULT WINAPI
D3D12CreateRootSignatureDeserializer(const void *pBytecode, SIZE_T DataSize, REFIID iid, void **ppDeserializer) {
  InitReturnPtr(ppDeserializer);

  const void *pRawRootSig;
  UINT RawRootSigSize;
  HRESULT hr = microsoft::DXBCGetRootSignature(pBytecode, &pRawRootSig, &RawRootSigSize);
  if (FAILED(hr))
    return hr;

  auto deserializer = Com(new MTLD3D12RootSignatureDeserializer<ID3D12RootSignatureDeserializer>());
  hr = deserializer->Deserialize(pRawRootSig, RawRootSigSize);
  if (FAILED(hr))
    return hr;
  return deserializer->QueryInterface(iid, ppDeserializer);
}

#pragma endregion

} // namespace dxmt