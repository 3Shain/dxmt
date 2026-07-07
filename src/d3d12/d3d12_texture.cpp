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

#include "d3d12_device.hpp"
#include "d3d12_pageable.hpp"
#include "dxmt_format.hpp"
#include "com/com_pointer.hpp"

namespace dxmt {

HRESULT
PopulateWMTTextureInfo(WMT::Device Device, WMTTextureInfo &InfoOut, const D3D12_RESOURCE_DESC &Desc) {
  MTL_DXGI_FORMAT_DESC Format;
  HRESULT hr = MTLQueryDXGIFormat(Device, Desc.Format, Format);
  if (FAILED(hr))
    return hr;

  InfoOut.pixel_format = Format.PixelFormat;

  if (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) {
    switch (Format.PixelFormat) {
    case WMTPixelFormatR32Uint:
    case WMTPixelFormatR32Sint:
    case WMTPixelFormatR32Float:
      InfoOut.pixel_format = WMTPixelFormatDepth32Float;
      break;
    case WMTPixelFormatR16Uint:
    case WMTPixelFormatR16Sint:
    case WMTPixelFormatR16Float:
    case WMTPixelFormatR16Unorm:
    case WMTPixelFormatR16Snorm:
      InfoOut.pixel_format = WMTPixelFormatDepth16Unorm;
      break;
    default:
      break;
    }
  }

  switch (Desc.Dimension) {
  default:
    return E_INVALIDARG;
  case D3D12_RESOURCE_DIMENSION_TEXTURE1D: {
    if (Format.Flag & (MTL_DXGI_FORMAT_BC | MTL_DXGI_FORMAT_DEPTH_PLANER | MTL_DXGI_FORMAT_STENCIL_PLANER))
      return E_INVALIDARG;
    InfoOut.width = Desc.Width;
    InfoOut.height = 1;
    InfoOut.depth = 1;
    InfoOut.array_length = Desc.DepthOrArraySize;
    if (Desc.DepthOrArraySize > 1)
      InfoOut.type = WMTTextureType2DArray;
    else
      InfoOut.type = WMTTextureType2D;
    InfoOut.sample_count = 1;
    break;
  }
  case D3D12_RESOURCE_DIMENSION_TEXTURE2D: {
    InfoOut.width = Desc.Width;
    InfoOut.height = Desc.Height;
    InfoOut.depth = 1;
    InfoOut.array_length = Desc.DepthOrArraySize;
    if (Desc.SampleDesc.Count == 0)
      return E_INVALIDARG;
    if (Desc.SampleDesc.Count > 1) {
      if (Desc.DepthOrArraySize > 1)
        InfoOut.type = WMTTextureType2DMultisampleArray;
      else
        InfoOut.type = WMTTextureType2DMultisample;
      InfoOut.sample_count = Desc.SampleDesc.Count;
    } else {
      if (Desc.DepthOrArraySize > 1)
        InfoOut.type = WMTTextureType2DArray;
      else
        InfoOut.type = WMTTextureType2D;
      InfoOut.sample_count = 1;
    }
    break;
  }
  case D3D12_RESOURCE_DIMENSION_TEXTURE3D: {
    InfoOut.width = Desc.Width;
    InfoOut.height = Desc.Height;
    InfoOut.depth = Desc.DepthOrArraySize;
    InfoOut.array_length = 1;
    InfoOut.type = WMTTextureType3D;
    InfoOut.sample_count = 1;
    break;
  }
  }
  if (Desc.MipLevels)
    InfoOut.mipmap_level_count = Desc.MipLevels;
  else
    InfoOut.mipmap_level_count = 32 - __builtin_clz(InfoOut.width | InfoOut.height | InfoOut.depth);

  WMTTextureUsage Usage = WMTTextureUsagePixelFormatView;
  if (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    Usage |= WMTTextureUsageRenderTarget;
  if (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    Usage |= WMTTextureUsageRenderTarget;
  if (Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
    Usage |= WMTTextureUsageShaderRead | WMTTextureUsageShaderWrite;
  if (!(Desc.Flags & D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE))
    Usage |= WMTTextureUsageShaderRead;
  InfoOut.usage = Usage;

  // TODO: decide storage mode
  InfoOut.options = WMTResourceHazardTrackingModeUntracked;

  return S_OK;
};

WMTPixelFormat
EncodeComponentMapping(UINT Shader4ComponentMapping, WMTPixelFormat Format) {

#define DX2MTL_SWIZZLE(x) ((x) + 2) % 6

  auto MappingR = DX2MTL_SWIZZLE(Shader4ComponentMapping & 0x7);
  auto MappingG = DX2MTL_SWIZZLE((Shader4ComponentMapping >> 3) & 0x7);
  auto MappingB = DX2MTL_SWIZZLE((Shader4ComponentMapping >> 6) & 0x7);
  auto MappingA = DX2MTL_SWIZZLE((Shader4ComponentMapping >> 9) & 0x7);
  auto DecodedR = GET_SWIZZLE_RED(Format);
  auto DecodedG = GET_SWIZZLE_GREEN(Format);
  auto DecodedB = GET_SWIZZLE_BLUE(Format);
  auto DecodedA = GET_SWIZZLE_ALPHA(Format);
  uint32_t Map[6] = {0, 1, DecodedR, DecodedG, DecodedB, DecodedA};
  return WMTPixelFormat(ENCODE_FORMAT_SWIZZLE(Format, Map[MappingR], Map[MappingG], Map[MappingB], Map[MappingA]));
}

class MTLD3D12Texture : public MTLD3D12Pageable<MTLD3D12Resource> {
  D3D12_RESOURCE_DESC desc_;

public:
  MTLD3D12Texture(MTLD3D12Device *pDevice) : MTLD3D12Pageable<MTLD3D12Resource>(pDevice) {}

  HRESULT
  Initialize(
      const D3D12_HEAP_PROPERTIES *pHeapProps, D3D12_HEAP_FLAGS HeapFlags, const D3D12_RESOURCE_DESC *pDesc,
      D3D12_RESOURCE_STATES InitialState, MTLD3D12Heap *pHeap
  ) {
    // TODO: validate and normalize
    desc_ = *pDesc;

    switch (InitialState) {
    case D3D12_RESOURCE_STATE_RENDER_TARGET: {
      if (desc_.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
        break;
      return E_INVALIDARG;
    }
    default:
      break;
    }

    WMTTextureInfo texture_info;
    HRESULT hr = PopulateWMTTextureInfo(device_->GetMTLDevice(), texture_info, desc_);
    if (FAILED(hr))
      return hr;

    texture = new Texture(texture_info, device_->GetMTLDevice());
    Flags<TextureAllocationFlag> flags = {};
    texture->rename(texture->allocate(flags));
    device_->RegisterResidency(texture->current()->texture());

    return S_OK;
  };

  ~MTLD3D12Texture() {
    device_->UnregisterResidency(texture->current()->texture());
  }

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) || riid == __uuidof(ID3D12DeviceChild) ||
        riid == __uuidof(ID3D12Pageable) || riid == __uuidof(ID3D12Resource)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D12Resource), riid)) {
      WARN("D3D12Texture: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  virtual HRESULT STDMETHODCALLTYPE
  Map(UINT Subresource, const D3D12_RANGE *pReadRange, void **ppData) {
    return E_NOTIMPL;
  };

  virtual void STDMETHODCALLTYPE Unmap(UINT Subresource, const D3D12_RANGE *pWrittenRange) {};

  virtual D3D12_RESOURCE_DESC *STDMETHODCALLTYPE
  GetDesc(D3D12_RESOURCE_DESC *__ret) {
    *__ret = desc_;
    return __ret;
  };

  virtual D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE
  GetGPUVirtualAddress() {
    return 0;
  };

  virtual HRESULT STDMETHODCALLTYPE
  WriteToSubresource(
      UINT DstSubresource, const D3D12_BOX *pDstBox, const void *pSrcData, UINT SrcRowPitch, UINT SrcSlicePitch
  ) {
    return E_NOTIMPL;
  };

  virtual HRESULT STDMETHODCALLTYPE
  ReadFromSubresource(
      void *pDstData, UINT DstRowPitch, UINT DstSlicePitch, UINT SrcSubresource, const D3D12_BOX *pSrcBox
  ) {
    return E_NOTIMPL;
  };

  virtual HRESULT STDMETHODCALLTYPE
  GetHeapProperties(D3D12_HEAP_PROPERTIES *pHeapProps, D3D12_HEAP_FLAGS *pFlags) {
    return E_NOTIMPL;
  };

  virtual HRESULT STDMETHODCALLTYPE
  CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE Descriptor) {
    HRESULT hr;
    D3D12_SHADER_RESOURCE_VIEW_DESC ViewDesc;
    if (!pDesc) {
      hr = ExtractEntireResourceViewDescription(desc_, &ViewDesc);
      if (FAILED(hr))
        return hr;
    } else {
      ViewDesc = *pDesc;
    }

    auto [Heap, Index] = GetShaderVisibleDescriptorHeap(device_, Descriptor);
    TextureViewKey View = texture->fullView;

    TextureViewDescriptor view_descriptor;
    MTL_DXGI_FORMAT_DESC metal_format;
    hr = MTLQueryDXGIFormat(device_->GetMTLDevice(), ViewDesc.Format, metal_format);
    if (FAILED(hr))
      return hr;

    view_descriptor.format = EncodeComponentMapping(ViewDesc.Shader4ComponentMapping, metal_format.PixelFormat);

    if (texture->pixelFormat() == WMTPixelFormatDepth32Float || texture->pixelFormat() == WMTPixelFormatDepth16Unorm) {
      view_descriptor.format = EncodeComponentMapping(ViewDesc.Shader4ComponentMapping, texture->pixelFormat());
    }

    FLOAT ResourceMinLODClamp = 0;

    switch (ViewDesc.ViewDimension) {
    case D3D12_SRV_DIMENSION_TEXTURE1D: {
      view_descriptor.type = WMTTextureType2D; // FIXME: lowering to 2d array
      view_descriptor.firstMiplevel = ViewDesc.Texture1D.MostDetailedMip;
      if (ViewDesc.Texture1D.MipLevels == ~0u)
        view_descriptor.miplevelCount = desc_.MipLevels - ViewDesc.Texture1D.MostDetailedMip;
      else
        view_descriptor.miplevelCount = ViewDesc.Texture1D.MipLevels;
      view_descriptor.firstArraySlice = 0;
      view_descriptor.arraySize = 1;
      View = texture->createView(view_descriptor);
      ResourceMinLODClamp = ViewDesc.Texture1D.ResourceMinLODClamp;
      break;
    }
    case D3D12_SRV_DIMENSION_TEXTURE1DARRAY: {
      view_descriptor.type = WMTTextureType2DArray;
      view_descriptor.firstMiplevel = ViewDesc.Texture1DArray.MostDetailedMip;
      if (ViewDesc.Texture1DArray.MipLevels == ~0u)
        view_descriptor.miplevelCount = desc_.MipLevels - ViewDesc.Texture1DArray.MostDetailedMip;
      else
        view_descriptor.miplevelCount = ViewDesc.Texture1DArray.MipLevels;
      view_descriptor.firstArraySlice = ViewDesc.Texture1DArray.FirstArraySlice;
      if (ViewDesc.Texture1DArray.ArraySize == ~0u)
        view_descriptor.arraySize = desc_.DepthOrArraySize - ViewDesc.Texture1DArray.FirstArraySlice;
      else
        view_descriptor.arraySize = ViewDesc.Texture1DArray.ArraySize;
      View = texture->createView(view_descriptor);
      ResourceMinLODClamp = ViewDesc.Texture1DArray.ResourceMinLODClamp;
      break;
    }
    case D3D12_SRV_DIMENSION_TEXTURE2D: {
      view_descriptor.type = WMTTextureType2D; // FIXME: lowering to 2d array
      view_descriptor.firstMiplevel = ViewDesc.Texture2D.MostDetailedMip;
      if (ViewDesc.Texture2D.MipLevels == ~0u)
        view_descriptor.miplevelCount = desc_.MipLevels - ViewDesc.Texture2D.MostDetailedMip;
      else
        view_descriptor.miplevelCount = ViewDesc.Texture2D.MipLevels;
      view_descriptor.firstArraySlice = 0;
      view_descriptor.arraySize = 1;
      View = texture->createView(view_descriptor);
      ResourceMinLODClamp = ViewDesc.Texture2D.ResourceMinLODClamp;
      break;
    }
    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY: {
      view_descriptor.type = WMTTextureType2DArray;
      view_descriptor.firstMiplevel = ViewDesc.Texture2DArray.MostDetailedMip;
      if (ViewDesc.Texture2DArray.MipLevels == ~0u)
        view_descriptor.miplevelCount = desc_.MipLevels - ViewDesc.Texture2DArray.MostDetailedMip;
      else
        view_descriptor.miplevelCount = ViewDesc.Texture2DArray.MipLevels;
      view_descriptor.firstArraySlice = ViewDesc.Texture2DArray.FirstArraySlice;
      if (ViewDesc.Texture2DArray.ArraySize == ~0u)
        view_descriptor.arraySize = desc_.DepthOrArraySize - ViewDesc.Texture2DArray.FirstArraySlice;
      else
        view_descriptor.arraySize = ViewDesc.Texture2DArray.ArraySize;
      View = texture->createView(view_descriptor);
      ResourceMinLODClamp = ViewDesc.Texture2DArray.ResourceMinLODClamp;
      break;
    }
    case D3D12_SRV_DIMENSION_TEXTURECUBE: {
      view_descriptor.type = WMTTextureTypeCube; // FIXME: lowering to cube array
      view_descriptor.firstMiplevel = ViewDesc.TextureCube.MostDetailedMip;
      if (ViewDesc.TextureCube.MipLevels == ~0u)
        view_descriptor.miplevelCount = desc_.MipLevels - ViewDesc.TextureCube.MostDetailedMip;
      else
        view_descriptor.miplevelCount = ViewDesc.TextureCube.MipLevels;
      view_descriptor.firstArraySlice = 0;
      view_descriptor.arraySize = 6;
      View = texture->createView(view_descriptor);
      ResourceMinLODClamp = ViewDesc.TextureCube.ResourceMinLODClamp;
      break;
    }
    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY: {
      view_descriptor.type = WMTTextureTypeCubeArray;
      view_descriptor.firstMiplevel = ViewDesc.TextureCubeArray.MostDetailedMip;
      if (ViewDesc.TextureCubeArray.MipLevels == ~0u)
        view_descriptor.miplevelCount = desc_.MipLevels - ViewDesc.TextureCubeArray.MostDetailedMip;
      else
        view_descriptor.miplevelCount = ViewDesc.TextureCubeArray.MipLevels;
      if (ViewDesc.TextureCubeArray.NumCubes == ~0u)
        view_descriptor.arraySize = desc_.DepthOrArraySize - ViewDesc.TextureCubeArray.First2DArrayFace;
      else
        view_descriptor.arraySize = ViewDesc.TextureCubeArray.NumCubes * 6;
      View = texture->createView(view_descriptor);
      ResourceMinLODClamp = ViewDesc.TextureCubeArray.ResourceMinLODClamp;
      break;
    }
    case D3D12_SRV_DIMENSION_TEXTURE2DMS: {
      view_descriptor.type = WMTTextureType2DMultisample; // FIXME: lowering to 2d array
      view_descriptor.firstMiplevel = 0;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = 0;
      view_descriptor.arraySize = 1;
      View = texture->createView(view_descriptor);
      break;
    }
    case D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY: {
      view_descriptor.type = WMTTextureType2DMultisampleArray;
      view_descriptor.firstMiplevel = 0;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = ViewDesc.Texture2DMSArray.FirstArraySlice;
      if (ViewDesc.Texture2DMSArray.ArraySize == ~0u)
        view_descriptor.arraySize = desc_.DepthOrArraySize - ViewDesc.Texture2DMSArray.FirstArraySlice;
      else
        view_descriptor.arraySize = ViewDesc.Texture2DMSArray.ArraySize;
      View = texture->createView(view_descriptor);
      break;
    }
    case D3D12_SRV_DIMENSION_TEXTURE3D: {
      view_descriptor.type = WMTTextureType3D;
      view_descriptor.firstMiplevel = ViewDesc.Texture3D.MostDetailedMip;
      if (ViewDesc.Texture3D.MipLevels == ~0u)
        view_descriptor.miplevelCount = desc_.MipLevels - ViewDesc.Texture3D.MostDetailedMip;
      else
        view_descriptor.miplevelCount = ViewDesc.Texture3D.MipLevels;
      view_descriptor.firstArraySlice = 0;
      view_descriptor.arraySize = 1;
      View = texture->createView(view_descriptor);
      ResourceMinLODClamp = ViewDesc.Texture3D.ResourceMinLODClamp;
      break;
    }
    default:
      return E_INVALIDARG;
    }

    ResourceMinLODClamp = FLOAT((INT)ResourceMinLODClamp - view_descriptor.firstMiplevel);

    return Heap->AddShaderResourceView(Index, texture.ptr(), View, ResourceMinLODClamp);
  };

  virtual HRESULT STDMETHODCALLTYPE
  CreateUnorderedAccessView(
      ID3D12Resource *pCounter, const D3D12_UNORDERED_ACCESS_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE Descriptor
  ) {
    IMPLEMENT_ME
    return S_OK;
  };

  virtual HRESULT STDMETHODCALLTYPE
  CreateRenderTargetView(const D3D12_RENDER_TARGET_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE Descriptor) {
    HRESULT hr;
    D3D12_RENDER_TARGET_VIEW_DESC ViewDesc;
    if (!pDesc) {
      hr = ExtractEntireResourceViewDescription(desc_, &ViewDesc);
      if (FAILED(hr))
        return hr;
    } else {
      ViewDesc = *pDesc;
    }

    auto [Heap, Index] = GetRenderTargetHeap(device_, Descriptor);
    TextureViewKey View = texture->fullView;

    TextureViewDescriptor view_descriptor;
    MTL_DXGI_FORMAT_DESC metal_format;
    hr = MTLQueryDXGIFormat(device_->GetMTLDevice(), ViewDesc.Format, metal_format);
    if (FAILED(hr))
      return hr;

    view_descriptor.format = metal_format.PixelFormat;

    MTL_RENDER_TARGET_DESC RenderTargetDesc;
    RenderTargetDesc.DepthPlane = 0;
    RenderTargetDesc.RenderTargetArrayLength = 0;
    RenderTargetDesc.Flags = 0;

    switch (ViewDesc.ViewDimension) {
    case D3D12_RTV_DIMENSION_TEXTURE1D: {
      view_descriptor.type = WMTTextureType2D; // FIXME: lowering to 2d array
      view_descriptor.firstMiplevel = ViewDesc.Texture1D.MipSlice;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = 0;
      view_descriptor.arraySize = 1;
      View = texture->createView(view_descriptor);
      break;
    }
    case D3D12_RTV_DIMENSION_TEXTURE1DARRAY: {
      view_descriptor.type = WMTTextureType2DArray;
      view_descriptor.firstMiplevel = ViewDesc.Texture1DArray.MipSlice;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = ViewDesc.Texture1DArray.FirstArraySlice;
      view_descriptor.arraySize =
          ViewDesc.Texture1DArray.ArraySize == ~0u ? texture->arrayLength() : ViewDesc.Texture1DArray.ArraySize;
      RenderTargetDesc.RenderTargetArrayLength = view_descriptor.arraySize;
      View = texture->createView(view_descriptor);
      break;
    }
    case D3D12_RTV_DIMENSION_TEXTURE2D: {
      if (ViewDesc.Texture2D.PlaneSlice)
        IMPLEMENT_ME
      view_descriptor.type = WMTTextureType2D; // FIXME: lowering to 2d array
      view_descriptor.firstMiplevel = ViewDesc.Texture2D.MipSlice;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = 0;
      view_descriptor.arraySize = 1;
      View = texture->createView(view_descriptor);
      break;
    }
    case D3D12_RTV_DIMENSION_TEXTURE2DARRAY: {
      if (ViewDesc.Texture2DArray.PlaneSlice)
        IMPLEMENT_ME
      view_descriptor.type = WMTTextureType2DArray;
      view_descriptor.firstMiplevel = ViewDesc.Texture2DArray.MipSlice;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = ViewDesc.Texture2DArray.FirstArraySlice;
      view_descriptor.arraySize =
          ViewDesc.Texture2DArray.ArraySize == ~0u ? texture->arrayLength() : ViewDesc.Texture2DArray.ArraySize;
      RenderTargetDesc.RenderTargetArrayLength = view_descriptor.arraySize;
      View = texture->createView(view_descriptor);
      break;
    }
    case D3D12_RTV_DIMENSION_TEXTURE2DMS: {
      view_descriptor.type = WMTTextureType2DMultisample; // FIXME: lowering to 2d array
      view_descriptor.firstMiplevel = 0;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = 0;
      view_descriptor.arraySize = 1;
      View = texture->createView(view_descriptor);
      break;
    }
    case D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY: {
      view_descriptor.type = WMTTextureType2DMultisampleArray;
      view_descriptor.firstMiplevel = 0;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = ViewDesc.Texture2DMSArray.FirstArraySlice;
      view_descriptor.arraySize =
          ViewDesc.Texture2DMSArray.ArraySize == ~0u ? texture->arrayLength() : ViewDesc.Texture2DMSArray.ArraySize;
      RenderTargetDesc.RenderTargetArrayLength = view_descriptor.arraySize;
      View = texture->createView(view_descriptor);
      break;
    }
    case D3D12_RTV_DIMENSION_TEXTURE3D: {
      view_descriptor.type = WMTTextureType3D;
      view_descriptor.firstMiplevel = ViewDesc.Texture3D.MipSlice;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = 0;
      view_descriptor.arraySize = 1;
      auto ArraySize = std::max<UINT16>(desc_.DepthOrArraySize >> ViewDesc.Texture3D.MipSlice, 1);
      RenderTargetDesc.RenderTargetArrayLength = view_descriptor.arraySize;

      if (ViewDesc.Texture3D.WSize == 1) {
        RenderTargetDesc.DepthPlane = ViewDesc.Texture3D.FirstWSlice;
      } else if (ViewDesc.Texture3D.FirstWSlice == 0) {
        if (ArraySize != ViewDesc.Texture3D.WSize) {
          WARN("Created a subview of 3D texture.");
        }
        RenderTargetDesc.RenderTargetArrayLength = ArraySize;
      } else {
        ERR("Failed to create 3D RTV, FirstWSlice:", ViewDesc.Texture3D.FirstWSlice,
            " WSize:", ViewDesc.Texture3D.WSize, " MippedDepth:", ArraySize);
      }

      break;
    }
    default:
      return E_INVALIDARG;
    }

    RenderTargetDesc.Texture = texture.ptr();
    RenderTargetDesc.View = View;
    RenderTargetDesc.Width = std::max<uint32_t>(1u, texture->width() >> view_descriptor.firstMiplevel);
    RenderTargetDesc.Height = std::max<uint32_t>(1u, texture->height() >> view_descriptor.firstMiplevel);

    return Heap->AddRenderTarget(Index, &RenderTargetDesc);
  };

  virtual HRESULT STDMETHODCALLTYPE
  CreateDepthStencilView(const D3D12_DEPTH_STENCIL_VIEW_DESC *pDesc, D3D12_CPU_DESCRIPTOR_HANDLE Descriptor) {
    HRESULT hr;
    D3D12_DEPTH_STENCIL_VIEW_DESC ViewDesc;
    if (!pDesc) {
      hr = ExtractEntireResourceViewDescription(desc_, &ViewDesc);
      if (FAILED(hr))
        return hr;
    } else {
      ViewDesc = *pDesc;
    }

    auto [Heap, Index] = GetRenderTargetHeap(device_, Descriptor);
    TextureViewKey View = texture->fullView;

    TextureViewDescriptor view_descriptor;
    MTL_DXGI_FORMAT_DESC metal_format;
    hr = MTLQueryDXGIFormat(device_->GetMTLDevice(), ViewDesc.Format, metal_format);
    if (FAILED(hr))
      return hr;
    view_descriptor.format = metal_format.PixelFormat;

    MTL_RENDER_TARGET_DESC RenderTargetDesc;
    RenderTargetDesc.DepthPlane = 0;
    RenderTargetDesc.RenderTargetArrayLength = 0;
    RenderTargetDesc.Flags = ViewDesc.Flags;

    switch (ViewDesc.ViewDimension) {
    case D3D12_DSV_DIMENSION_TEXTURE1D: {
      view_descriptor.type = WMTTextureType2D; // FIXME: lowering to 2d array
      view_descriptor.firstMiplevel = ViewDesc.Texture1D.MipSlice;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = 0;
      view_descriptor.arraySize = 1;
      View = texture->createView(view_descriptor);
      break;
    }
    case D3D12_DSV_DIMENSION_TEXTURE1DARRAY: {
      view_descriptor.type = WMTTextureType2DArray;
      view_descriptor.firstMiplevel = ViewDesc.Texture1DArray.MipSlice;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = ViewDesc.Texture1DArray.FirstArraySlice;
      view_descriptor.arraySize =
          ViewDesc.Texture1DArray.ArraySize == ~0u ? texture->arrayLength() : ViewDesc.Texture1DArray.ArraySize;
      RenderTargetDesc.RenderTargetArrayLength = view_descriptor.arraySize;
      View = texture->createView(view_descriptor);
      break;
    }
    case D3D12_DSV_DIMENSION_TEXTURE2D: {
      view_descriptor.type = WMTTextureType2D; // FIXME: lowering to 2d array
      view_descriptor.firstMiplevel = ViewDesc.Texture2D.MipSlice;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = 0;
      view_descriptor.arraySize = 1;
      View = texture->createView(view_descriptor);
      break;
    }
    case D3D12_DSV_DIMENSION_TEXTURE2DARRAY: {
      view_descriptor.type = WMTTextureType2DArray;
      view_descriptor.firstMiplevel = ViewDesc.Texture2DArray.MipSlice;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = ViewDesc.Texture2DArray.FirstArraySlice;
      view_descriptor.arraySize =
          ViewDesc.Texture2DArray.ArraySize == ~0u ? texture->arrayLength() : ViewDesc.Texture2DArray.ArraySize;
      RenderTargetDesc.RenderTargetArrayLength = view_descriptor.arraySize;
      View = texture->createView(view_descriptor);
      break;
    }
    case D3D12_DSV_DIMENSION_TEXTURE2DMS: {
      view_descriptor.type = WMTTextureType2DMultisample; // FIXME: lowering to 2d array
      view_descriptor.firstMiplevel = 0;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = 0;
      view_descriptor.arraySize = 1;
      View = texture->createView(view_descriptor);
      break;
    }
    case D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY: {
      view_descriptor.type = WMTTextureType2DMultisampleArray;
      view_descriptor.firstMiplevel = 0;
      view_descriptor.miplevelCount = 1;
      view_descriptor.firstArraySlice = ViewDesc.Texture2DMSArray.FirstArraySlice;
      view_descriptor.arraySize =
          ViewDesc.Texture2DMSArray.ArraySize == ~0u ? texture->arrayLength() : ViewDesc.Texture2DMSArray.ArraySize;
      RenderTargetDesc.RenderTargetArrayLength = view_descriptor.arraySize;
      View = texture->createView(view_descriptor);
      break;
    }
    default:
      return E_INVALIDARG;
    }

    RenderTargetDesc.Texture = texture.ptr();
    RenderTargetDesc.View = View;
    RenderTargetDesc.Width = std::max<uint32_t>(1u, texture->width() >> view_descriptor.firstMiplevel);
    RenderTargetDesc.Height = std::max<uint32_t>(1u, texture->height() >> view_descriptor.firstMiplevel);

    return Heap->AddRenderTarget(Index, &RenderTargetDesc);
  };

  virtual void STDMETHODCALLTYPE GetResourceTiling(
      UINT *TotalTileCount, D3D12_PACKED_MIP_INFO *PackedMipInfo, D3D12_TILE_SHAPE *StandardTitleShape,
      UINT *SubresourceTilingCount, UINT FirstSubresourceTiling, D3D12_SUBRESOURCE_TILING *SubresourceTilings
  ) {
    IMPLEMENT_ME
  };
};

HRESULT
CreateCommittedTexture(
    MTLD3D12Device *pDevice, const D3D12_HEAP_PROPERTIES *pHeapProps, D3D12_HEAP_FLAGS HeapFlags,
    const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState, const D3D12_CLEAR_VALUE *OptimizedClearValue,
    REFIID riid, void **ppResource
) {
  InitReturnPtr(ppResource);
  auto texture = Com(new MTLD3D12Texture(pDevice));
  HRESULT hr = texture->Initialize(pHeapProps, HeapFlags, pDesc, InitialState, nullptr);
  if (FAILED(hr))
    return hr;
  if (!ppResource)
    return S_FALSE;
  return texture->QueryInterface(riid, ppResource);
}

HRESULT
CreatePlacedTexture(
    MTLD3D12Device *pDevice, MTLD3D12Heap *pHeap, const D3D12_RESOURCE_DESC *pDesc, D3D12_RESOURCE_STATES InitialState,
    const D3D12_CLEAR_VALUE *OptimizedClearValue, REFIID riid, void **ppResource
) {
  InitReturnPtr(ppResource);
  auto texture = Com(new MTLD3D12Texture(pDevice));
  D3D12_HEAP_DESC heap_desc = pHeap->GetDesc();

  if (heap_desc.Flags & D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS)
    return E_INVALIDARG;

  HRESULT hr = texture->Initialize(&heap_desc.Properties, heap_desc.Flags, pDesc, InitialState, pHeap);
  if (FAILED(hr))
    return hr;
  if (!ppResource)
    return S_FALSE;
  return texture->QueryInterface(riid, ppResource);
}

} // namespace dxmt