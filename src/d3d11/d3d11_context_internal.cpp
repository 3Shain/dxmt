
/**
This file is included by d3d11_context.cpp
and should be not used as a compilation unit

since it is for internal use only
(and I don't want to deal with several thousands line of code)
*/
#include "d3d11_private.h"
#include "d3d11_context_state.hpp"
#include "d3d11_device.hpp"
#include "d3d11_pipeline.hpp"
#include "dxmt_command_queue.hpp"
#include "mtld11_resource.hpp"
#include "util_flags.hpp"
#include "util_math.hpp"

namespace dxmt {

template <bool Tessellation>
inline MTL_BINDABLE_RESIDENCY_MASK GetResidencyMask(ShaderType type, bool read,
                                                    bool write) {
  switch (type) {
  case ShaderType::Vertex:
    if constexpr (Tessellation)
      return (read ? MTL_RESIDENCY_OBJECT_READ : MTL_RESIDENCY_NULL) |
             (write ? MTL_RESIDENCY_OBJECT_WRITE : MTL_RESIDENCY_NULL);
    else
      return (read ? MTL_RESIDENCY_VERTEX_READ : MTL_RESIDENCY_NULL) |
             (write ? MTL_RESIDENCY_VERTEX_WRITE : MTL_RESIDENCY_NULL);
  case ShaderType::Pixel:
    return (read ? MTL_RESIDENCY_FRAGMENT_READ : MTL_RESIDENCY_NULL) |
           (write ? MTL_RESIDENCY_FRAGMENT_WRITE : MTL_RESIDENCY_NULL);
  case ShaderType::Hull:
    return (read ? MTL_RESIDENCY_MESH_READ : MTL_RESIDENCY_NULL) |
           (write ? MTL_RESIDENCY_MESH_WRITE : MTL_RESIDENCY_NULL);
  case ShaderType::Domain:
    return (read ? MTL_RESIDENCY_VERTEX_READ : MTL_RESIDENCY_NULL) |
           (write ? MTL_RESIDENCY_VERTEX_WRITE : MTL_RESIDENCY_NULL);
  case ShaderType::Geometry:
    D3D11_ASSERT(0 && "TODO");
    break;
  case ShaderType::Compute:
    return (read ? MTL_RESIDENCY_READ : MTL_RESIDENCY_NULL) |
           (write ? MTL_RESIDENCY_WRITE : MTL_RESIDENCY_NULL);
  }
}

inline MTL::ResourceUsage
GetUsageFromResidencyMask(MTL_BINDABLE_RESIDENCY_MASK mask) {
  return ((mask & MTL_RESIDENCY_READ) ? MTL::ResourceUsageRead : 0) |
         ((mask & MTL_RESIDENCY_WRITE) ? MTL::ResourceUsageWrite : 0);
}

inline MTL::RenderStages
GetStagesFromResidencyMask(MTL_BINDABLE_RESIDENCY_MASK mask) {
  return ((mask & (MTL_RESIDENCY_FRAGMENT_READ | MTL_RESIDENCY_FRAGMENT_WRITE))
              ? MTL::RenderStageFragment
              : 0) |
         ((mask & (MTL_RESIDENCY_VERTEX_READ | MTL_RESIDENCY_VERTEX_WRITE))
              ? MTL::RenderStageVertex
              : 0) |
         ((mask & (MTL_RESIDENCY_OBJECT_READ | MTL_RESIDENCY_OBJECT_WRITE))
              ? MTL::RenderStageObject
              : 0)
              |
         ((mask & (MTL_RESIDENCY_MESH_READ | MTL_RESIDENCY_MESH_WRITE))
              ? MTL::RenderStageMesh
              : 0);
}

struct Subresource {
  DXGI_FORMAT Format;
  uint32_t MipLevel;
  uint32_t ArraySlice;
  uint32_t Width;
  uint32_t Height;
  uint32_t Depth;
};

class BlitObject {
public:
  ID3D11Resource *pResource;
  MTL_FORMAT_DESC FormatDescription;
  D3D11_RESOURCE_DIMENSION Dimension;
  union {
    D3D11_TEXTURE1D_DESC Texture1DDesc;
    D3D11_TEXTURE2D_DESC1 Texture2DDesc;
    D3D11_TEXTURE3D_DESC1 Texture3DDesc;
    D3D11_BUFFER_DESC BufferDesc;
  };

  BlitObject(IMTLDXGIAdatper *pAdapter, ID3D11Resource *pResource)
      : pResource(pResource) {
    pResource->GetType(&Dimension);
    switch (Dimension) {
    case D3D11_RESOURCE_DIMENSION_UNKNOWN:
      break;
    case D3D11_RESOURCE_DIMENSION_BUFFER:
      reinterpret_cast<ID3D11Buffer *>(pResource)->GetDesc(&BufferDesc);
      break;
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
      reinterpret_cast<ID3D11Texture1D *>(pResource)->GetDesc(&Texture1DDesc);
      pAdapter->QueryFormatDesc(Texture1DDesc.Format, &FormatDescription);
      break;
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
      reinterpret_cast<ID3D11Texture2D1 *>(pResource)->GetDesc1(&Texture2DDesc);
      pAdapter->QueryFormatDesc(Texture2DDesc.Format, &FormatDescription);
      break;
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
      reinterpret_cast<ID3D11Texture3D1 *>(pResource)->GetDesc1(&Texture3DDesc);
      pAdapter->QueryFormatDesc(Texture3DDesc.Format, &FormatDescription);
      break;
    }
  };
};

class TextureCopyCommand {
public:
  ID3D11Resource *pSrc;
  ID3D11Resource *pDst;
  uint32_t SrcSubresource;
  uint32_t DstSubresource;

  Subresource Src;
  Subresource Dst;
  MTL::Origin SrcOrigin;
  MTL::Size SrcSize;
  MTL::Origin DstOrigin;

  MTL_FORMAT_DESC SrcFormat;
  MTL_FORMAT_DESC DstFormat;

  bool Invalid = true;

  TextureCopyCommand(BlitObject &Dst_, UINT DstSubresource, UINT DstX,
                     UINT DstY, UINT DstZ, BlitObject &Src_,
                     UINT SrcSubresource, const D3D11_BOX *pSrcBox)
      : pSrc(Src_.pResource), pDst(Dst_.pResource), SrcSubresource(SrcSubresource),
        DstSubresource(DstSubresource), SrcFormat(Src_.FormatDescription),
        DstFormat(Dst_.FormatDescription) {
    if (Dst_.Dimension != Src_.Dimension)
      return;

    if(SrcFormat.PixelFormat == MTL::PixelFormatInvalid)
      return;
    if(DstFormat.PixelFormat == MTL::PixelFormatInvalid)
      return;

    switch (Dst_.Dimension) {
    default: {
      return;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
      D3D11_TEXTURE1D_DESC& dst_desc = Dst_.Texture1DDesc;
      D3D11_TEXTURE1D_DESC& src_desc = Src_.Texture1DDesc;

      Src.Format = src_desc.Format;
      Src.MipLevel = SrcSubresource % src_desc.MipLevels;
      Src.ArraySlice = SrcSubresource / src_desc.MipLevels;
      Src.Width = std::max(1u, src_desc.Width >> Src.MipLevel);
      Src.Height = 1;
      Src.Depth = 1;

      Dst.Format = dst_desc.Format;
      Dst.MipLevel = DstSubresource % dst_desc.MipLevels;
      Dst.ArraySlice = DstSubresource / dst_desc.MipLevels;
      Dst.Width = std::max(1u, dst_desc.Width >> Dst.MipLevel);
      Dst.Height = 1;
      Dst.Depth = 1;
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
      D3D11_TEXTURE2D_DESC1& dst_desc = Dst_.Texture2DDesc;
      D3D11_TEXTURE2D_DESC1& src_desc = Src_.Texture2DDesc;

      Src.Format = src_desc.Format;
      Src.MipLevel = SrcSubresource % src_desc.MipLevels;
      Src.ArraySlice = SrcSubresource / src_desc.MipLevels;
      Src.Width = std::max(1u, src_desc.Width >> Src.MipLevel);
      Src.Height = std::max(1u, src_desc.Height >> Src.MipLevel);
      Src.Depth = 1;

      Dst.Format = dst_desc.Format;
      Dst.MipLevel = DstSubresource % dst_desc.MipLevels;
      Dst.ArraySlice = DstSubresource / dst_desc.MipLevels;
      Dst.Width = std::max(1u, dst_desc.Width >> Dst.MipLevel);
      Dst.Height = std::max(1u, dst_desc.Height >> Dst.MipLevel);
      Dst.Depth = 1;
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
      D3D11_TEXTURE3D_DESC1& dst_desc = Dst_.Texture3DDesc;
      D3D11_TEXTURE3D_DESC1& src_desc = Src_.Texture3DDesc;

      Src.Format = src_desc.Format;
      Src.MipLevel = SrcSubresource;
      Src.ArraySlice = 0;
      Src.Width = std::max(1u, src_desc.Width >> Src.MipLevel);
      Src.Height = std::max(1u, src_desc.Height >> Src.MipLevel);
      Src.Depth = std::max(1u, src_desc.Depth >> Src.MipLevel);

      Dst.Format = dst_desc.Format;
      Dst.MipLevel = DstSubresource;
      Dst.ArraySlice = 0;
      Dst.Width = std::max(1u, dst_desc.Width >> Dst.MipLevel);
      Dst.Height = std::max(1u, dst_desc.Height >> Dst.MipLevel);
      Dst.Depth = std::max(1u, dst_desc.Depth >> Dst.MipLevel);

      break;
    }
    }

    D3D11_BOX SrcBox;

    if (pSrcBox) {
      if (pSrcBox->left >= pSrcBox->right)
        return;
      if (pSrcBox->top >= pSrcBox->bottom)
        return;
      if (pSrcBox->front >= pSrcBox->back)
        return;
      SrcBox = *pSrcBox;
    } else {
      SrcBox = {0, 0, 0, Src.Width, Src.Height, Src.Depth};
    }

    // Discard, if source box does't overlap with source size at all
    if (SrcBox.left >= Src.Width)
      return;
    if (SrcBox.top >= Src.Height)
      return;
    if (SrcBox.front >= Src.Depth)
      return;

    // Discard, if dest origin is not even within dest
    if (DstX >= Dst.Width)
      return;
    if (DstY >= Dst.Height)
      return;
    if (DstZ >= Dst.Depth)
      return;

    // Clip source box, if it's larger than source size
    // It is useful for copying between BC textures
    // since Metal takes virtual size
    // while D3D11 requires block size aligned (typically 4x)
    SrcBox.right = std::min(SrcBox.right, Src.Width);
    SrcBox.bottom = std::min(SrcBox.bottom, Src.Height);
    SrcBox.back = std::min(SrcBox.back, Src.Depth);

    if (DstFormat.IsCompressed == SrcFormat.IsCompressed) {
      // Discard, if the copy overflow
      if ((DstX + SrcBox.right - SrcBox.left) > Dst.Width)
        return;
      if ((DstY + SrcBox.bottom - SrcBox.top) > Dst.Height)
        return;
      if ((DstZ + SrcBox.back - SrcBox.front) > Dst.Depth)
        return;
    }

    DstOrigin = {DstX, DstY, DstZ};
    SrcOrigin = {SrcBox.left, SrcBox.top, SrcBox.front};
    SrcSize = {SrcBox.right - SrcBox.left, SrcBox.bottom - SrcBox.top,
               SrcBox.back - SrcBox.front};

    Invalid = false;
  }
};

class TextureUpdateCommand {
public:
  ID3D11Resource *pDst;
  Subresource Dst;
  MTL::Region DstRegion;
  uint32_t EffectiveBytesPerRow;
  uint32_t EffectiveRows;

  MTL_FORMAT_DESC DstFormat;

  bool Invalid = true;

  TextureUpdateCommand(BlitObject &Dst_, UINT DstSubresource,
                       const D3D11_BOX *pDstBox)
      : pDst(Dst_.pResource), DstFormat(Dst_.FormatDescription) {

    if(DstFormat.PixelFormat == MTL::PixelFormatInvalid)
      return;

    switch (Dst_.Dimension) {
    default: {
      return;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
      D3D11_TEXTURE1D_DESC &dst_desc = Dst_.Texture1DDesc;

      if(DstSubresource >= dst_desc.MipLevels * dst_desc.ArraySize)
        return;

      Dst.Format = dst_desc.Format;
      Dst.MipLevel = DstSubresource % dst_desc.MipLevels;
      Dst.ArraySlice = DstSubresource / dst_desc.MipLevels;
      Dst.Width = std::max(1u, dst_desc.Width >> Dst.MipLevel);
      Dst.Height = 1;
      Dst.Depth = 1;
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
      D3D11_TEXTURE2D_DESC1 dst_desc = Dst_.Texture2DDesc;

      if(DstSubresource >= dst_desc.MipLevels * dst_desc.ArraySize)
        return;

      Dst.Format = dst_desc.Format;
      Dst.MipLevel = DstSubresource % dst_desc.MipLevels;
      Dst.ArraySlice = DstSubresource / dst_desc.MipLevels;
      Dst.Width = std::max(1u, dst_desc.Width >> Dst.MipLevel);
      Dst.Height = std::max(1u, dst_desc.Height >> Dst.MipLevel);
      Dst.Depth = 1;
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
      D3D11_TEXTURE3D_DESC1 dst_desc = Dst_.Texture3DDesc;

      if(DstSubresource >= dst_desc.MipLevels)
        return;

      Dst.Format = dst_desc.Format;
      Dst.MipLevel = DstSubresource;
      Dst.ArraySlice = 0;
      Dst.Width = std::max(1u, dst_desc.Width >> Dst.MipLevel);
      Dst.Height = std::max(1u, dst_desc.Height >> Dst.MipLevel);
      Dst.Depth = std::max(1u, dst_desc.Depth >> Dst.MipLevel);
      break;
    }
    }

    D3D11_BOX DstBox;

    if (pDstBox) {
      if (pDstBox->left >= pDstBox->right)
        return;
      if (pDstBox->top >= pDstBox->bottom)
        return;
      if (pDstBox->front >= pDstBox->back)
        return;
      DstBox = *pDstBox;
    } else {
      DstBox = {0, 0, 0, Dst.Width, Dst.Height, Dst.Depth};
    }

    // Discard, if box does't overlap with the texure at all
    if (DstBox.left >= Dst.Width)
      return;
    if (DstBox.top >= Dst.Height)
      return;
    if (DstBox.front >= Dst.Depth)
      return;

    // It is useful for copying between BC textures
    // since Metal takes virtual size
    // while D3D11 requires block size aligned (typically 4x)
    DstBox.right = std::min(DstBox.right, Dst.Width);
    DstBox.bottom = std::min(DstBox.bottom, Dst.Height);
    DstBox.back = std::min(DstBox.back, Dst.Depth);

    DstRegion = {DstBox.left,
                 DstBox.top,
                 DstBox.front,
                 DstBox.right - DstBox.left,
                 DstBox.bottom - DstBox.top,
                 DstBox.back - DstBox.front};

    if (DstFormat.IsCompressed) {
      EffectiveBytesPerRow =
          (align(DstRegion.size.width, 4u) >> 2) * DstFormat.BytesPerTexel;
      EffectiveRows = align(DstRegion.size.height, 4u) >> 2;
    } else {
      EffectiveBytesPerRow = DstRegion.size.width * DstFormat.BytesPerTexel;
      EffectiveRows = DstRegion.size.height;
    }

    Invalid = false;
  }
};

class ContextInternal {

public:
  ContextInternal(IMTLD3D11Device *pDevice, D3D11ContextState &state,
                  CommandQueue &cmd_queue)
      : device(pDevice), state_(state), cmd_queue(cmd_queue) {
    pDevice->CreateRasterizerState2(
        &kDefaultRasterizerDesc,
        (ID3D11RasterizerState2 **)&default_rasterizer_state);
    pDevice->CreateBlendState1(&kDefaultBlendDesc,
                               (ID3D11BlendState1 **)&default_blend_state);
    pDevice->CreateDepthStencilState(
        &kDefaultDepthStencilDesc,
        (ID3D11DepthStencilState **)&default_depth_stencil_state);
    /* we don't need the extra reference as they are always valid */
    default_rasterizer_state->Release();
    default_blend_state->Release();
    default_depth_stencil_state->Release();
  }

  /**
  Valid transition:
  * -> Idle
  Idle -> RenderEncoderActive
  Idle -> ComputeEncoderActive
  Idle -> (Upload|Readback)BlitEncoderActive
  RenderEncoderActive <-> RenderPipelineReady
  ComputeEncoderActive <-> CoputePipelineReady
  */
  enum class CommandBufferState {
    Idle,
    RenderEncoderActive,
    RenderPipelineReady,
    TessellationRenderPipelineReady,
    ComputeEncoderActive,
    ComputePipelineReady,
    BlitEncoderActive,
    UpdateBlitEncoderActive,
    ReadbackBlitEncoderActive
  };

#pragma region ShaderCommon

  template <ShaderType Type, typename IShader>
  void SetShader(IShader *pShader, ID3D11ClassInstance *const *ppClassInstances,
                 UINT NumClassInstances) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    if (pShader) {
      if (auto expected = com_cast<IMTLD3D11Shader>(pShader)) {
        ShaderStage.Shader = std::move(expected);
        // Com<IMTLCompiledShader> _;
        // ShaderStage.Shader->GetCompiledShader(&_);
        ShaderStage.ConstantBuffers.set_dirty();
        ShaderStage.SRVs.set_dirty();
        ShaderStage.Samplers.set_dirty();
        if (Type == ShaderType::Compute) {
          state_.ComputeStageUAV.UAVs.set_dirty();
        } else {
          state_.OutputMerger.UAVs.set_dirty();
        }
      } else {
        D3D11_ASSERT(0 && "wtf");
      }
    } else {
      ShaderStage.Shader = nullptr;
    }

    if (NumClassInstances)
      ERR("Class instances not supported");

    if (Type == ShaderType::Compute) {
      InvalidateComputePipeline();
    } else {
      InvalidateRenderPipeline();
    }
  }

  template <ShaderType Type, typename IShader>
  void GetShader(IShader **ppShader, ID3D11ClassInstance **ppClassInstances,
                 UINT *pNumClassInstances) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    if (ppShader) {
      if (ShaderStage.Shader) {
        ShaderStage.Shader->QueryInterface(IID_PPV_ARGS(ppShader));
      } else {
        *ppShader = NULL;
      }
    }
    if (ppClassInstances) {
      *ppClassInstances = NULL;
    }
    if (pNumClassInstances) {
      *pNumClassInstances = 0;
    }
  }

  template <ShaderType Type>
  void SetConstantBuffer(UINT StartSlot, UINT NumBuffers,
                         ID3D11Buffer *const *ppConstantBuffers,
                         const UINT *pFirstConstant,
                         const UINT *pNumConstants) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    for (unsigned slot = StartSlot; slot < StartSlot + NumBuffers; slot++) {
      auto pConstantBuffer = ppConstantBuffers[slot - StartSlot];
      if (pConstantBuffer) {
        bool replaced = false;
        auto &entry =
            ShaderStage.ConstantBuffers.bind(slot, {pConstantBuffer}, replaced);
        if (!replaced) {
          if (pFirstConstant &&
              pFirstConstant[slot - StartSlot] != entry.FirstConstant) {
            ShaderStage.ConstantBuffers.set_dirty(slot);
            entry.FirstConstant = pFirstConstant[slot - StartSlot];
          }
          if (pNumConstants &&
              pNumConstants[slot - StartSlot] != entry.NumConstants) {
            ShaderStage.ConstantBuffers.set_dirty(slot);
            entry.NumConstants = pNumConstants[slot - StartSlot];
          }
          continue;
        }
        entry.FirstConstant =
            pFirstConstant ? pFirstConstant[slot - StartSlot] : 0;
        if (pNumConstants) {
          entry.NumConstants = pNumConstants[slot - StartSlot];
        } else {
          D3D11_BUFFER_DESC desc;
          pConstantBuffer->GetDesc(&desc);
          entry.NumConstants = desc.ByteWidth >> 4;
        }
        if (auto dynamic = com_cast<IMTLDynamicBindable>(pConstantBuffer)) {
          auto old = std::move(entry.Buffer); // FIXME: it looks so weird!
          // should migrate vertex buffer storage to BindingSet
          // once we have implemented BindingSet iteration
          dynamic->GetBindable(
              &entry.Buffer, [slot, &set = ShaderStage.ConstantBuffers](auto) {
                set.set_dirty(slot);
              });
        } else if (auto expected = com_cast<IMTLBindable>(pConstantBuffer)) {
          entry.Buffer = std::move(expected);
        } else {
          D3D11_ASSERT(0 && "wtf");
        }
      } else {
        // BIND NULL
        ShaderStage.ConstantBuffers.unbind(slot);
      }
    }
  }

  template <ShaderType Type>
  void GetConstantBuffer(UINT StartSlot, UINT NumBuffers,
                         ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant,
                         UINT *pNumConstants) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    for (auto i = 0u; i < NumBuffers; i++) {
      if (ShaderStage.ConstantBuffers.test_bound(StartSlot + i)) {
        auto &cb = ShaderStage.ConstantBuffers.at(StartSlot + i);
        if (ppConstantBuffers) {
          cb.Buffer->GetLogicalResourceOrView(
              IID_PPV_ARGS(&ppConstantBuffers[i]));
        }
        if (pFirstConstant) {
          pFirstConstant[i] = cb.FirstConstant;
        }
        if (pNumConstants) {
          pNumConstants[i] = cb.NumConstants;
        }
      } else {
        if (ppConstantBuffers) {
          ppConstantBuffers[i] = nullptr;
        }
      }
    }
  }

  template <ShaderType Type>
  void
  SetShaderResource(UINT StartSlot, UINT NumViews,
                    ID3D11ShaderResourceView *const *ppShaderResourceViews) {

    auto &ShaderStage = state_.ShaderStages[(UINT)Type];
    for (unsigned slot = StartSlot; slot < StartSlot + NumViews; slot++) {
      auto pView = ppShaderResourceViews[slot - StartSlot];
      if (pView) {
        bool replaced = false;
        auto &entry = ShaderStage.SRVs.bind(slot, {pView}, replaced);
        if (!replaced)
          continue;
        if (auto dynamic = com_cast<IMTLDynamicBindable>(pView)) {
          entry.SRV = nullptr;
          dynamic->GetBindable(&entry.SRV, [slot, &set = ShaderStage.SRVs](
                                               auto) { set.set_dirty(slot); });
        } else if (auto expected = com_cast<IMTLBindable>(pView)) {
          entry.SRV = std::move(expected);
        } else {
          D3D11_ASSERT(0 && "wtf");
        }
      } else {
        // BIND NULL
        ShaderStage.SRVs.unbind(slot);
      }
    }
  }

  template <ShaderType Type>
  void GetShaderResource(UINT StartSlot, UINT NumViews,
                         ID3D11ShaderResourceView **ppShaderResourceViews) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    if (!ppShaderResourceViews)
      return;
    for (auto i = 0u; i < NumViews; i++) {
      if (ShaderStage.SRVs.test_bound(StartSlot + i)) {
        ShaderStage.SRVs[StartSlot + i].SRV->GetLogicalResourceOrView(
            IID_PPV_ARGS(&ppShaderResourceViews[i]));
      } else {
        ppShaderResourceViews[i] = nullptr;
      }
    }
  }

  template <ShaderType Type>
  void SetSamplers(UINT StartSlot, UINT NumSamplers,
                   ID3D11SamplerState *const *ppSamplers) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];
    for (unsigned Slot = StartSlot; Slot < StartSlot + NumSamplers; Slot++) {
      auto pSampler = ppSamplers[Slot - StartSlot];
      if (pSampler) {
        bool replaced = false;
        auto &entry = ShaderStage.Samplers.bind(Slot, {pSampler}, replaced);
        if (!replaced)
          continue;
        if (auto expected = com_cast<IMTLD3D11SamplerState>(pSampler)) {
          entry.Sampler = expected.ptr();
        } else {
          D3D11_ASSERT(0 && "wtf");
        }
      } else {
        // BIND NULL
        ShaderStage.Samplers.unbind(Slot);
      }
    }
  }

  template <ShaderType Type>
  void GetSamplers(UINT StartSlot, UINT NumSamplers,
                   ID3D11SamplerState **ppSamplers) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];
    if (ppSamplers) {
      for (unsigned Slot = StartSlot; Slot < StartSlot + NumSamplers; Slot++) {
        if (ShaderStage.Samplers.test_bound(Slot)) {
          ShaderStage.Samplers[Slot].Sampler->QueryInterface(
              IID_PPV_ARGS(&ppSamplers[Slot - StartSlot]));
        } else {
          ppSamplers[Slot - StartSlot] = nullptr;
        }
      }
    }
  }

  template <ShaderType Type>
  void SetUnorderedAccessView(
      UINT StartSlot, UINT NumUAVs,
      ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
      const UINT *pUAVInitialCounts) {
    auto &binding_set = Type == ShaderType::Compute
                            ? state_.ComputeStageUAV.UAVs
                            : state_.OutputMerger.UAVs;

    // std::erase_if(state_.ComputeStageUAV.UAVs, [&](const auto &item) -> bool
    // {
    //   auto &[slot, bound_uav] = item;
    //   if (slot < StartSlot || slot >= (StartSlot + NumUAVs))
    //     return false;
    //   for (auto i = 0u; i < NumUAVs; i++) {
    //     if (auto uav = static_cast<IMTLD3D11UnorderedAccessView *>(
    //             ppUnorderedAccessViews[i])) {
    //       // FIXME! GetViewRange is not defined on IMTLBindable
    //       // if (bound_uav.View->GetViewRange().CheckOverlap(
    //       //         uav->GetViewRange())) {
    //       //   return true;
    //       // }
    //     }
    //   }
    //   return false;
    // });

    auto currentChunkId = cmd_queue.CurrentSeqId();

    for (unsigned slot = StartSlot; slot < StartSlot + NumUAVs; slot++) {
      auto pUAV = ppUnorderedAccessViews[slot - StartSlot];
      auto InitialCount =
          pUAVInitialCounts ? pUAVInitialCounts[slot - StartSlot] : ~0u;
      if (pUAV) {
        auto uav = com_cast<IMTLD3D11UnorderedAccessView>(pUAV);
        bool replaced = false;
        auto &entry = binding_set.bind(slot, {pUAV}, replaced);
        if (InitialCount != ~0u) {
          auto new_counter_handle = cmd_queue.counter_pool.AllocateCounter(
              currentChunkId, InitialCount);
          // it's possible that old_counter_handle == new_counter_handle 
          // if the uav doesn't support counter
          // thus it becomes essentially a no-op.
          auto old_counter_handle = uav->SwapCounter(new_counter_handle);
          if (old_counter_handle != DXMT_NO_COUNTER) {
            cmd_queue.counter_pool.DiscardCounter(currentChunkId,
                                                  old_counter_handle);
          }
        }
        if (!replaced) {
          continue;
        }
        if (auto expected = com_cast<IMTLBindable>(pUAV)) {
          entry.View = std::move(expected);
        } else {
          D3D11_ASSERT(0 && "wtf");
        }
        // FIXME: resolve srv hazard: unbind any cs srv that share the resource
        // std::erase_if(state_.ShaderStages[5].SRVs,
        //               [&](const auto &item) -> bool {
        //                 // auto &[slot, bound_srv] = item;
        //                 // if srv conflict with uav, return true
        //                 return false;
        //               });
      } else {
        binding_set.unbind(slot);
      }
    }
  }

#pragma endregion

#pragma region InputAssembler
  void SetVertexBuffers(UINT StartSlot, UINT NumBuffers,
                        ID3D11Buffer *const *ppVertexBuffers,
                        const UINT *pStrides, const UINT *pOffsets) {
    auto &VertexBuffers = state_.InputAssembler.VertexBuffers;
    for (unsigned slot = StartSlot; slot < StartSlot + NumBuffers; slot++) {
      auto pVertexBuffer = ppVertexBuffers[slot - StartSlot];
      if (pVertexBuffer) {
        bool replaced = false;
        auto &entry = VertexBuffers.bind(slot, {pVertexBuffer}, replaced);
        if (!replaced) {
          if (pStrides && pStrides[slot - StartSlot] != entry.Stride) {
            VertexBuffers.set_dirty(slot);
            entry.Stride = pStrides[slot - StartSlot];
          }
          if (pOffsets && pOffsets[slot - StartSlot] != entry.Offset) {
            VertexBuffers.set_dirty(slot);
            entry.Offset = pOffsets[slot - StartSlot];
          }
          continue;
        }
        if (auto dynamic = com_cast<IMTLDynamicBindable>(pVertexBuffer)) {
          entry.Buffer = nullptr;
          dynamic->GetBindable(&entry.Buffer, [&VertexBuffers, slot,
                                               &entry](MTL::Buffer *buffer) {
            VertexBuffers.set_dirty(slot);
            entry.BufferRaw = buffer;
          });
        } else if (auto expected = com_cast<IMTLBindable>(pVertexBuffer)) {
          entry.Buffer = std::move(expected);
        } else {
          D3D11_ASSERT(0 && "wtf");
        }
        if (pStrides) {
          entry.Stride = pStrides[slot - StartSlot];
        } else {
          ERR("SetVertexBuffers: stride is null");
          entry.Stride = 0;
        }
        if (pOffsets) {
          entry.Offset = pOffsets[slot - StartSlot];
        } else {
          ERR("SetVertexBuffers: offset is null");
          entry.Stride = 0;
        }
        entry.BufferRaw =
            entry.Buffer->UseBindable(this->cmd_queue.CurrentSeqId()).buffer();
      } else {
        VertexBuffers.unbind(slot);
      }
    }
  }

  void GetVertexBuffers(UINT StartSlot, UINT NumBuffers,
                        ID3D11Buffer **ppVertexBuffers, UINT *pStrides,
                        UINT *pOffsets) {
    auto &VertexBuffers = state_.InputAssembler.VertexBuffers;
    for (unsigned i = 0; i < NumBuffers; i++) {
      if (!VertexBuffers.test_bound(i + StartSlot)) {
        if (ppVertexBuffers != NULL)
          ppVertexBuffers[i] = nullptr;
        if (pStrides != NULL)
          pStrides[i] = 0;
        if (pOffsets != NULL)
          pOffsets[i] = 0;
        continue;
      }
      auto &VertexBuffer = state_.InputAssembler.VertexBuffers[i + StartSlot];
      if (ppVertexBuffers != NULL) {
        VertexBuffer.Buffer->GetLogicalResourceOrView(
            IID_PPV_ARGS(&ppVertexBuffers[i]));
      }
      if (pStrides != NULL)
        pStrides[i] = VertexBuffer.Stride;
      if (pOffsets != NULL)
        pOffsets[i] = VertexBuffer.Offset;
    }
  }
#pragma endregion

#pragma region CopyResource

  void CopyBuffer(ID3D11Buffer *pDstResource, uint32_t DstSubresource,
                  uint32_t DstX, uint32_t DstY, uint32_t DstZ,
                  ID3D11Buffer *pSrcResource, uint32_t SrcSubresource,
                  const D3D11_BOX *pSrcBox) {
    D3D11_BUFFER_DESC dst_desc;
    D3D11_BUFFER_DESC src_desc;
    pDstResource->GetDesc(&dst_desc);
    pSrcResource->GetDesc(&src_desc);
    D3D11_ASSERT(SrcSubresource == 0);
    D3D11_ASSERT(DstSubresource == 0);
    D3D11_BOX SrcBox;
    if (pSrcBox) {
      SrcBox = *pSrcBox;
    } else {
      SrcBox.left = 0;
      SrcBox.right = src_desc.ByteWidth;
    }
    if (SrcBox.right <= SrcBox.left)
      return;
    auto currentChunkId = cmd_queue.CurrentSeqId();
    if (auto staging_dst = com_cast<IMTLD3D11Staging>(pDstResource)) {
      if (auto staging_src = com_cast<IMTLD3D11Staging>(pSrcResource)) {
        D3D11_ASSERT(0 && "tod: copy between staging");
      } else if (auto src = com_cast<IMTLBindable>(pSrcResource)) {
        // copy from device to staging
        MTL_STAGING_RESOURCE dst_bind;
        uint32_t bytes_per_row, bytes_per_image;
        if (!staging_dst->UseCopyDestination(DstSubresource, currentChunkId,
                                             &dst_bind, &bytes_per_row,
                                             &bytes_per_image))
          return;
        EmitBlitCommand<true>([src_ = src->UseBindable(currentChunkId),
                               dst = Obj(dst_bind.Buffer), DstX, SrcBox](
                                  MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto src = src_.buffer();
          encoder->copyFromBuffer(src, SrcBox.left, dst, DstX,
                                  SrcBox.right - SrcBox.left);
        }, ContextInternal::CommandBufferState::ReadbackBlitEncoderActive);
        promote_flush = true;
      } else {
        D3D11_ASSERT(0 && "todo");
      }
    } else if (dst_desc.Usage == D3D11_USAGE_DEFAULT) {
      auto dst = com_cast<IMTLBindable>(pDstResource);
      D3D11_ASSERT(dst);
      if (auto staging_src = com_cast<IMTLD3D11Staging>(pSrcResource)) {
        // copy from staging to default
        MTL_STAGING_RESOURCE src_bind;
        uint32_t bytes_per_row, bytes_per_image;
        if (!staging_src->UseCopySource(SrcSubresource, currentChunkId,
                                        &src_bind, &bytes_per_row,
                                        &bytes_per_image))
          return;
        EmitBlitCommand<true>([dst_ = dst->UseBindable(currentChunkId),
                               src = Obj(src_bind.Buffer), DstX, SrcBox](
                                  MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto dst = dst_.buffer();
          // FIXME: offste should be calculated from SrcBox
          encoder->copyFromBuffer(src, SrcBox.left, dst, DstX,
                                  SrcBox.right - SrcBox.left);
        });
      } else if (auto src = com_cast<IMTLBindable>(pSrcResource)) {
        // on-device copy
        EmitBlitCommand<true>(
            [dst_ = dst->UseBindable(currentChunkId),
             src_ = src->UseBindable(currentChunkId), DstX,
             SrcBox](MTL::BlitCommandEncoder *encoder, auto &ctx) {
              auto src = src_.buffer();
              auto dst = dst_.buffer();
              encoder->copyFromBuffer(src, SrcBox.left, dst, DstX,
                                      SrcBox.right - SrcBox.left);
            });
      } else {
        D3D11_ASSERT(0 && "todo");
      }
    } else if (dst_desc.Usage == D3D11_USAGE_DYNAMIC) {
      auto dst_dynamic = com_cast<IMTLDynamicBindable>(pDstResource);
      D3D11_ASSERT(dst_dynamic);
      Com<IMTLBindable> dst;
      dst_dynamic->GetBindable(&dst, [](auto) {});
      if (auto staging_src = com_cast<IMTLD3D11Staging>(pSrcResource)) {
        // copy from staging to dynamic
        MTL_STAGING_RESOURCE src_bind;
        uint32_t bytes_per_row, bytes_per_image;
        if (!staging_src->UseCopySource(SrcSubresource, currentChunkId,
                                        &src_bind, &bytes_per_row,
                                        &bytes_per_image))
          return;
        EmitBlitCommand<true>([dst_ = dst->UseBindable(currentChunkId).buffer(),
                               src = Obj(src_bind.Buffer), DstX, SrcBox](
                                  MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto dst = dst_;
          // FIXME: offste should be calculated from SrcBox
          encoder->copyFromBuffer(src, SrcBox.left, dst, DstX,
                                  SrcBox.right - SrcBox.left);
        });
      } else if (auto src = com_cast<IMTLBindable>(pSrcResource)) {
        // device to dynamic
        EmitBlitCommand<true>(
            [dst_ = dst->UseBindable(currentChunkId).buffer(),
             src_ = src->UseBindable(currentChunkId), DstX,
             SrcBox](MTL::BlitCommandEncoder *encoder, auto &ctx) {
              auto src = src_.buffer();
              auto dst = dst_;
              encoder->copyFromBuffer(src, SrcBox.left, dst, DstX,
                                      SrcBox.right - SrcBox.left);
            });
      } else {
        D3D11_ASSERT(0 && "todo");
      }
    } else {
      D3D11_ASSERT(0 && "todo");
    }
  }

  void CopyTexture(TextureCopyCommand &&cmd) {
    if (cmd.Invalid)
      return;
    if (cmd.SrcFormat.IsCompressed != cmd.DstFormat.IsCompressed) {
      if (cmd.SrcFormat.IsCompressed) {
        return CopyTextureFromCompressed(std::move(cmd));
      } else {
        return CopyTextureToCompressed(std::move(cmd));
      }
    }
    return CopyTextureBitcast(std::move(cmd));
  }

  void CopyTextureBitcast(TextureCopyCommand &&cmd) {
    auto currentChunkId = cmd_queue.CurrentSeqId();
    if (auto staging_dst = com_cast<IMTLD3D11Staging>(cmd.pDst)) {
      if (auto staging_src = com_cast<IMTLD3D11Staging>(cmd.pSrc)) {
        D3D11_ASSERT(0 && "TODO: copy between staging");
      } else if (auto src = com_cast<IMTLBindable>(cmd.pSrc)) {
        // copy from device to staging
        MTL_STAGING_RESOURCE dst_bind;
        uint32_t bytes_per_row, bytes_per_image;
        if (!staging_dst->UseCopyDestination(cmd.DstSubresource, currentChunkId,
                                             &dst_bind, &bytes_per_row,
                                             &bytes_per_image))
          return;
        EmitBlitCommand<true>(
            [src_ = src->UseBindable(currentChunkId),
             dst = Obj(dst_bind.Buffer), bytes_per_row, bytes_per_image,
             cmd = std::move(cmd)](MTL::BlitCommandEncoder *encoder,
                                   auto &ctx) {
              auto src = src_.texture(&ctx);
              // auto offset = DstOrigin.z * bytes_per_image +
              //               DstOrigin.y * bytes_per_row +
              //               DstOrigin.x * bytes_per_texel;
              encoder->copyFromTexture(src, cmd.Src.ArraySlice,
                                       cmd.Src.MipLevel, cmd.SrcOrigin,
                                       cmd.SrcSize, dst.ptr(), 0 /* offset */,
                                       bytes_per_row, bytes_per_image);
            },
            ContextInternal::CommandBufferState::ReadbackBlitEncoderActive);
        promote_flush = true;
      } else {
        D3D11_ASSERT(0 && "TODO: copy from dynamic to staging");
      }
    } else if (auto dst = com_cast<IMTLBindable>(cmd.pDst)) {
      if (auto staging_src = com_cast<IMTLD3D11Staging>(cmd.pSrc)) {
        // copy from staging to default
        MTL_STAGING_RESOURCE src_bind;
        uint32_t bytes_per_row, bytes_per_image;
        if (!staging_src->UseCopySource(cmd.SrcSubresource, currentChunkId,
                                        &src_bind, &bytes_per_row,
                                        &bytes_per_image))
          return;

        EmitBlitCommand<true>([dst_ = dst->UseBindable(currentChunkId),
                               src = Obj(src_bind.Buffer), bytes_per_row,
                               bytes_per_image, cmd = std::move(cmd)](
                                  MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto dst = dst_.texture(&ctx);
          // auto offset = SrcBox.front * bytes_per_image +
          //               SrcBox.top * bytes_per_row +
          //               SrcBox.left * bytes_per_texel;
          encoder->copyFromBuffer(
              src, 0 /* offset */, bytes_per_row, bytes_per_image, cmd.SrcSize,
              dst, cmd.Dst.ArraySlice, cmd.Dst.MipLevel, cmd.DstOrigin);
        });
      } else if (auto src = com_cast<IMTLBindable>(cmd.pSrc)) {
        // on-device copy
        EmitBlitCommand<true>([dst_ = dst->UseBindable(currentChunkId),
                               src_ = src->UseBindable(currentChunkId),
                               cmd = std::move(cmd), currentChunkId](
                                  MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto src = src_.texture(&ctx);
          auto dst = dst_.texture(&ctx);
          auto src_format = src->pixelFormat();
          auto dst_format = dst->pixelFormat();
          if (Forget_sRGB(dst_format) != Forget_sRGB(src_format)) {
            // bitcast, using a temporary buffer
            size_t bytes_per_row, bytes_per_image, bytes_total;
            if (cmd.SrcFormat.IsCompressed) {
              bytes_per_row = (align(cmd.SrcSize.width, 4u) >> 2) *
                              cmd.SrcFormat.BytesPerTexel;
              bytes_per_image =
                  bytes_per_row * (align(cmd.SrcSize.height, 4u) >> 2);
            } else {
              bytes_per_row = cmd.SrcSize.width * cmd.SrcFormat.BytesPerTexel;
              bytes_per_image = bytes_per_row * cmd.SrcSize.height;
            }
            bytes_total = bytes_per_image * cmd.SrcSize.depth;
            auto [_, buffer, offset] =
                ctx.queue->AllocateTempBuffer(currentChunkId, bytes_total, 16);
            encoder->copyFromTexture(src, cmd.Src.ArraySlice, cmd.Src.MipLevel,
                                     cmd.SrcOrigin, cmd.SrcSize, buffer, offset,
                                     bytes_per_row, bytes_per_image);
            encoder->copyFromBuffer(
                buffer, offset, bytes_per_row, bytes_per_image, cmd.SrcSize,
                dst, cmd.Dst.ArraySlice, cmd.Dst.MipLevel, cmd.DstOrigin);
            return;
          }
          encoder->copyFromTexture(src, cmd.Src.ArraySlice, cmd.Src.MipLevel,
                                   cmd.SrcOrigin, cmd.SrcSize, dst,
                                   cmd.Dst.ArraySlice, cmd.Dst.MipLevel,
                                   cmd.DstOrigin);
        });
      } else {
        D3D11_ASSERT(0 && "TODO: copy from dynamic to device");
      }
    } else {
      D3D11_ASSERT(0 && "TODO: copy to dynamic?");
    }
  }

  void CopyTextureFromCompressed(TextureCopyCommand &&cmd) {
    auto currentChunkId = cmd_queue.CurrentSeqId();
    if (auto staging_dst = com_cast<IMTLD3D11Staging>(cmd.pDst)) {
      if (auto staging_src = com_cast<IMTLD3D11Staging>(cmd.pSrc)) {
        D3D11_ASSERT(0 && "TODO: copy between staging (from compressed)");
      } else if (auto src = com_cast<IMTLBindable>(cmd.pSrc)) {
        D3D11_ASSERT(0 && "TODO: copy from compressed device to staging");
      } else {
        D3D11_ASSERT(0 && "TODO: copy from compressed dynamic to staging");
      }
    } else if (auto dst = com_cast<IMTLBindable>(cmd.pDst)) {
      if (auto staging_src = com_cast<IMTLD3D11Staging>(cmd.pSrc)) {
        // copy from staging to default
        D3D11_ASSERT(0 && "TODO: copy from compressed staging to default");
      } else if (auto src = com_cast<IMTLBindable>(cmd.pSrc)) {
        // on-device copy
        EmitBlitCommand<true>([dst_ = dst->UseBindable(currentChunkId),
                               src_ = src->UseBindable(currentChunkId),
                               cmd = std::move(cmd), currentChunkId](
                                  MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto src = src_.texture(&ctx);
          auto dst = dst_.texture(&ctx);
          auto block_w = (align(cmd.SrcSize.width, 4u) >> 2);
          auto block_h = (align(cmd.SrcSize.height, 4u) >> 2);
          auto bytes_per_row = block_w * cmd.SrcFormat.BytesPerTexel;
          auto bytes_per_image = bytes_per_row * block_h;
          auto [_, buffer, offset] = ctx.queue->AllocateTempBuffer(
              currentChunkId, bytes_per_image * cmd.SrcSize.depth, 16);
          encoder->copyFromTexture(src, cmd.Src.ArraySlice, cmd.Src.MipLevel,
                                   cmd.SrcOrigin, cmd.SrcSize, buffer, offset,
                                   bytes_per_row, bytes_per_image);
          encoder->copyFromBuffer(
              buffer, offset, bytes_per_row, bytes_per_image,
              MTL::Size::Make(block_w, block_h, cmd.SrcSize.depth), dst,
              cmd.Dst.ArraySlice, cmd.Dst.MipLevel, cmd.DstOrigin);
          return;
        });
      } else {
        D3D11_ASSERT(0 && "TODO: copy from compressed dynamic to device");
      }
    } else {
      D3D11_ASSERT(0 && "TODO: copy from compressed ? to dynamic");
    }
  }

  void CopyTextureToCompressed(TextureCopyCommand &&cmd) {
    auto currentChunkId = cmd_queue.CurrentSeqId();
    if (auto staging_dst = com_cast<IMTLD3D11Staging>(cmd.pDst)) {
      if (auto staging_src = com_cast<IMTLD3D11Staging>(cmd.pSrc)) {
        D3D11_ASSERT(0 && "TODO: copy between staging (to compressed)");
      } else if (auto src = com_cast<IMTLBindable>(cmd.pSrc)) {
        D3D11_ASSERT(0 && "TODO: copy from device to compressed staging");
      } else {
        D3D11_ASSERT(0 && "TODO: copy from dynamic to compressed staging");
      }
    } else if (auto dst = com_cast<IMTLBindable>(cmd.pDst)) {
      if (auto staging_src = com_cast<IMTLD3D11Staging>(cmd.pSrc)) {
        // copy from staging to default
        D3D11_ASSERT(0 && "TODO: copy from staging to compressed default");
      } else if (auto src = com_cast<IMTLBindable>(cmd.pSrc)) {
        // on-device copy
        EmitBlitCommand<true>([dst_ = dst->UseBindable(currentChunkId),
                               src_ = src->UseBindable(currentChunkId),
                               cmd = std::move(cmd), currentChunkId](
                                  MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto src = src_.texture(&ctx);
          auto dst = dst_.texture(&ctx);
          auto bytes_per_row = cmd.SrcSize.width * cmd.SrcFormat.BytesPerTexel;
          auto bytes_per_image = cmd.SrcSize.height * bytes_per_row;
          auto [_, buffer, offset] = ctx.queue->AllocateTempBuffer(
              currentChunkId, bytes_per_image * cmd.SrcSize.depth, 16);
          encoder->copyFromTexture(src, cmd.Src.ArraySlice, cmd.Src.MipLevel,
                                   cmd.SrcOrigin, cmd.SrcSize, buffer, offset,
                                   bytes_per_row, bytes_per_image);
          auto clamped_src_width = std::min(
              cmd.SrcSize.width << 2,
              std::max<uint32_t>(dst->width() >> cmd.Dst.MipLevel, 1u) -
                  cmd.DstOrigin.x);
          auto clamped_src_height = std::min(
              cmd.SrcSize.height << 2,
              std::max<uint32_t>(dst->height() >> cmd.Dst.MipLevel, 1u) -
                  cmd.DstOrigin.y);
          encoder->copyFromBuffer(
              buffer, offset, bytes_per_row, bytes_per_image,
              MTL::Size::Make(clamped_src_width, clamped_src_height,
                              cmd.SrcSize.depth),
              dst, cmd.Dst.ArraySlice, cmd.Dst.MipLevel, cmd.DstOrigin);
          return;
        });
      } else {
        D3D11_ASSERT(0 && "TODO: copy from dynamic to compressed device");
      }
    } else {
      D3D11_ASSERT(0 && "TODO: copy to compressed dynamic");
    }
  }

  void UpdateTexture(TextureUpdateCommand &&cmd, const void *pSrcData,
                     UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags) {
    if (cmd.Invalid)
      return;

    if (auto bindable = com_cast<IMTLBindable>(cmd.pDst)) {
      while (!bindable->GetContentionState(cmd_queue.CoherentSeqId())) {
        auto dst = bindable->UseBindable(cmd_queue.CurrentSeqId());
        auto texture = dst.texture();
        if (!texture)
          break;
        texture->replaceRegion(cmd.DstRegion, cmd.Dst.MipLevel,
                               cmd.Dst.ArraySlice, pSrcData, SrcRowPitch,
                               SrcDepthPitch);
        return;
      }
      auto bytes_per_depth_slice = cmd.EffectiveRows * cmd.EffectiveBytesPerRow;
      auto [ptr, staging_buffer, offset] = cmd_queue.AllocateStagingBuffer(
          bytes_per_depth_slice * cmd.DstRegion.size.depth, 16);
      if (cmd.EffectiveBytesPerRow == SrcRowPitch) {
        for (unsigned depthSlice = 0; depthSlice < cmd.DstRegion.size.depth;
             depthSlice++) {
          char *dst = ((char *)ptr) + depthSlice * bytes_per_depth_slice;
          const char *src =
              ((const char *)pSrcData) + depthSlice * SrcDepthPitch;
          memcpy(dst, src, bytes_per_depth_slice);
        }
      } else {
        for (unsigned depthSlice = 0; depthSlice < cmd.DstRegion.size.depth;
             depthSlice++) {
          for (unsigned row = 0; row < cmd.EffectiveRows; row++) {
            char *dst = ((char *)ptr) + row * cmd.EffectiveBytesPerRow +
                        depthSlice * bytes_per_depth_slice;
            const char *src = ((const char *)pSrcData) + row * SrcRowPitch +
                              depthSlice * SrcDepthPitch;
            memcpy(dst, src, cmd.EffectiveBytesPerRow);
          }
        }
      }
      EmitBlitCommand<true>(
          [staging_buffer, offset,
           dst = bindable->UseBindable(cmd_queue.CurrentSeqId()),
           cmd = std::move(cmd),
           bytes_per_depth_slice](MTL::BlitCommandEncoder *enc, auto &ctx) {
            enc->copyFromBuffer(
                staging_buffer, offset, cmd.EffectiveBytesPerRow,
                bytes_per_depth_slice, cmd.DstRegion.size, dst.texture(&ctx),
                cmd.Dst.ArraySlice, cmd.Dst.MipLevel, cmd.DstRegion.origin);
          },
          ContextInternal::CommandBufferState::UpdateBlitEncoderActive);
    } else if (auto dynamic = com_cast<IMTLDynamicBindable>(cmd.pDst)) {
      D3D11_ASSERT(CopyFlags && "otherwise resource cannot be dynamic");
      D3D11_ASSERT(0 && "TODO: UpdateSubresource1: update dynamic texture");
    } else if (auto staging_dst = com_cast<IMTLD3D11Staging>(cmd.pDst)) {
      // staging: ...
      D3D11_ASSERT(0 && "TODO: UpdateSubresource1: update staging texture");
    } else {
      D3D11_ASSERT(0 && "TODO: UpdateSubresource1: unknown texture");
    }
  }

#pragma endregion

#pragma region CommandEncoder Maintain State

  bool promote_flush = false;

  /**
  Render pass can be invalidated by reasons:
  - render target changes (including depth stencil)
  All pass can be invalidated by reasons:
  - a encoder type switch
  - flush/present
  Return value indicates if a commit happens
  */
  bool InvalidateCurrentPass(bool defer_commit = false) {
    CommandChunk *chk = cmd_queue.CurrentChunk();
    switch (cmdbuf_state) {
    case CommandBufferState::Idle:
      break;
    case CommandBufferState::RenderEncoderActive:
    case CommandBufferState::RenderPipelineReady:
    case CommandBufferState::TessellationRenderPipelineReady: {
      chk->emit([](CommandChunk::context &ctx) {
        ctx.render_encoder->endEncoding();
        ctx.render_encoder = nullptr;
        ctx.dsv_planar_flags = 0;
      });
      occlusion_query_seq++;
      break;
    }
    case CommandBufferState::ComputeEncoderActive:
    case CommandBufferState::ComputePipelineReady:
      chk->emit([](CommandChunk::context &ctx) {
        ctx.compute_encoder->endEncoding();
        ctx.compute_encoder = nullptr;
      });
      break;
    case CommandBufferState::UpdateBlitEncoderActive:
    case CommandBufferState::ReadbackBlitEncoderActive:
    case CommandBufferState::BlitEncoderActive:
      chk->emit([](CommandChunk::context &ctx) {
        ctx.blit_encoder->endEncoding();
        ctx.blit_encoder = nullptr;
      });
      break;
    }

    cmdbuf_state = CommandBufferState::Idle;
    if (promote_flush && !defer_commit) {
      Commit();
      return true;
    }

    return false;
  }

  /**
  Render pipeline can be invalidate by reasons:
  - shader program changes
  - input layout changes (if using metal vertex descriptor)
  - blend state changes
  - tessellation state changes (not implemented yet)
  - (when layered rendering) input primitive topology changes
  -
  */
  void InvalidateRenderPipeline() {
    if (cmdbuf_state != CommandBufferState::RenderPipelineReady &&
        cmdbuf_state != CommandBufferState::TessellationRenderPipelineReady)
      return;
    cmdbuf_state = CommandBufferState::RenderEncoderActive;
  }

  /**
  Compute pipeline can be invalidate by reasons:
  - shader program changes
  TOOD: maybe we don't need this, since compute shader can be pre-compiled
  */
  void InvalidateComputePipeline() {
    if (cmdbuf_state != CommandBufferState::ComputePipelineReady)
      return;
    cmdbuf_state = CommandBufferState::ComputeEncoderActive;
  }

  void EncodeClearPass(ENCODER_CLEARPASS_INFO *clear_pass) {
    InvalidateCurrentPass();
    CommandChunk *chk = cmd_queue.CurrentChunk();
    chk->emit([clear_pass,
               _ = clear_pass->use_clearpass()](CommandChunk::context &ctx) {
      if (clear_pass->skipped) {
        return;
      }
      auto pool = transfer(NS::AutoreleasePool::alloc()->init());
      unsigned index = clear_pass->num_color_attachments;
      while (index--) {
        auto enc_descriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
        auto attachmentz = enc_descriptor->colorAttachments()->object(0);
        attachmentz->setClearColor(clear_pass->clear_colors[index]);
        attachmentz->setTexture(
            clear_pass->clear_color_attachments[index].texture(&ctx));
        attachmentz->setLoadAction(MTL::LoadActionClear);
        attachmentz->setStoreAction(MTL::StoreActionStore);
        auto enc = ctx.cmdbuf->renderCommandEncoder(enc_descriptor);
        enc->setLabel(NS::String::string("ClearPass", NS::ASCIIStringEncoding));
        enc->endEncoding();
      }
      if (clear_pass->depth_stencil_flags) {
        auto enc_descriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
        MTL::Texture *texture =
            clear_pass->clear_depth_stencil_attachment.texture(&ctx);
        uint32_t planar_flags = DepthStencilPlanarFlags(texture->pixelFormat());
        if (clear_pass->depth_stencil_flags & planar_flags &
            D3D11_CLEAR_DEPTH) {
          auto attachmentz = enc_descriptor->depthAttachment();
          attachmentz->setClearDepth(clear_pass->clear_depth);
          attachmentz->setTexture(texture);
          attachmentz->setLoadAction(MTL::LoadActionClear);
          attachmentz->setStoreAction(MTL::StoreActionStore);
        }
        if (clear_pass->depth_stencil_flags & planar_flags &
            D3D11_CLEAR_STENCIL) {
          auto attachmentz = enc_descriptor->stencilAttachment();
          attachmentz->setClearStencil(clear_pass->clear_stencil);
          attachmentz->setTexture(texture);
          attachmentz->setLoadAction(MTL::LoadActionClear);
          attachmentz->setStoreAction(MTL::StoreActionStore);
        }
        enc_descriptor->setRenderTargetHeight(texture->height());
        enc_descriptor->setRenderTargetWidth(texture->width());
        enc_descriptor->setDefaultRasterSampleCount(1);
        auto enc = ctx.cmdbuf->renderCommandEncoder(enc_descriptor);
        enc->setLabel(NS::String::string("ClearDepthPass", NS::ASCIIStringEncoding));
        enc->endEncoding();
      }

    });
  };

  void ClearRenderTargetView(IMTLD3D11RenderTargetView *pRenderTargetView,
                             const FLOAT ColorRGBA[4]) {
    CommandChunk *chk = cmd_queue.CurrentChunk();

    auto rtv_props = pRenderTargetView->GetRenderTargetProps();
    if (rtv_props->RenderTargetArrayLength > 0) {
      EmitComputeCommandChk<true>(
          [texture = pRenderTargetView->GetBinding(cmd_queue.CurrentSeqId()),
           clear_color =
               std::array<float, 4>({ColorRGBA[0], ColorRGBA[1], ColorRGBA[2],
                                     ColorRGBA[3]})](auto encoder, auto &ctx) {
            D3D11_ASSERT(texture.texture(&ctx)->textureType() ==
                         MTL::TextureType3D);
            ctx.queue->clear_cmd.ClearTexture3DFloat(
                encoder, texture.texture(&ctx), clear_color);
          });
      return;
    }

    auto target = pRenderTargetView->GetBinding(cmd_queue.CurrentSeqId());
    auto clear_color = MTL::ClearColor::Make(ColorRGBA[0], ColorRGBA[1],
                                             ColorRGBA[2], ColorRGBA[3]);

    auto previous_encoder = chk->get_last_encoder();
    // use `while` instead of `if`, for short circuiting
    while (previous_encoder->kind == EncoderKind::ClearPass) {
      auto previous_clearpass = (ENCODER_CLEARPASS_INFO *)previous_encoder;
      unsigned i = 0;
      for (; i < previous_clearpass->num_color_attachments; i++) {
        if (previous_clearpass->clear_color_attachments[i] == target) {
          previous_clearpass->clear_colors[i] = clear_color;
          return;
        }
      }
      if (i == 8) {
        break; // we have to create a new clearpass
      }
      previous_clearpass->num_color_attachments = i + 1;
      previous_clearpass->clear_color_attachments[i] = std::move(target);
      previous_clearpass->clear_colors[i] = clear_color;
      return;
    }

    auto clear_pass = chk->mark_clear_pass();
    clear_pass->num_color_attachments = 1;
    clear_pass->clear_color_attachments[0] = std::move(target);
    clear_pass->clear_colors[0] = clear_color;
    EncodeClearPass(clear_pass);
  }

  void ClearDepthStencilView(IMTLD3D11DepthStencilView *pDepthStencilView,
                             UINT ClearFlags, FLOAT Depth, UINT8 Stencil) {
    if (ClearFlags == 0)
      return;
    CommandChunk *chk = cmd_queue.CurrentChunk();
    auto target = pDepthStencilView->GetBinding(cmd_queue.CurrentSeqId());

    auto previous_encoder = chk->get_last_encoder();
    bool inherit_rtvs_from_previous_encoder = false;
    // use `while` instead of `if`, for short circuiting
    while (previous_encoder->kind == EncoderKind::ClearPass) {
      auto previous_clearpass = (ENCODER_CLEARPASS_INFO *)previous_encoder;
      // if there is already a depth stencil attachment
      if (previous_clearpass->clear_depth_stencil_attachment) {
        // if it's the same target
        if (previous_clearpass->clear_depth_stencil_attachment == target) {
          // override previous value
          previous_clearpass->depth_stencil_flags |= ClearFlags;
          if (ClearFlags & D3D11_CLEAR_DEPTH) {
            previous_clearpass->clear_depth = Depth;
          }
          if (ClearFlags & D3D11_CLEAR_STENCIL) {
            previous_clearpass->clear_stencil = Stencil;
          }
          return;
        }
        // otherwise we must create a new clearpass
        inherit_rtvs_from_previous_encoder = true;
        break;
      }
      // no depth stencil attachment, just set it
      previous_clearpass->clear_depth_stencil_attachment = std::move(target);
      previous_clearpass->depth_stencil_flags = ClearFlags;
      if (ClearFlags & D3D11_CLEAR_DEPTH) {
        previous_clearpass->clear_depth = Depth;
      }
      if (ClearFlags & D3D11_CLEAR_STENCIL) {
        previous_clearpass->clear_stencil = Stencil;
      }
      return;
    }

    auto clear_pass = chk->mark_clear_pass();
    clear_pass->clear_depth_stencil_attachment = std::move(target);
    clear_pass->depth_stencil_flags = ClearFlags;
    if (ClearFlags & D3D11_CLEAR_DEPTH) {
      clear_pass->clear_depth = Depth;
    }
    if (ClearFlags & D3D11_CLEAR_STENCIL) {
      clear_pass->clear_stencil = Stencil;
    }
    if (inherit_rtvs_from_previous_encoder) {
      auto previous_clearpass = (ENCODER_CLEARPASS_INFO *)previous_encoder;
      clear_pass->num_color_attachments =
          previous_clearpass->num_color_attachments;
      for (unsigned i = 0; i < clear_pass->num_color_attachments; i++) {
        clear_pass->clear_colors[i] = previous_clearpass->clear_colors[i];
        clear_pass->clear_color_attachments[i] =
            std::move(previous_clearpass->clear_color_attachments[i]);
        previous_clearpass->num_color_attachments = 0;
      }
    }
    EncodeClearPass(clear_pass);
  }

  void ResolveSubresource(
    IMTLBindable* pSrc,
    UINT SrcSlice,
    IMTLBindable* pDst,
    UINT DstLevel,
    UINT DstSlice
  ) {
    CommandChunk *chk = cmd_queue.CurrentChunk();

    chk->mark_pass(EncoderKind::Resolve);
    InvalidateCurrentPass();
    chk->emit([
      src = pSrc->UseBindable(cmd_queue.CurrentSeqId()),
      dst = pDst->UseBindable(cmd_queue.CurrentSeqId()),
      SrcSlice, DstSlice, DstLevel
    ](CommandChunk::context &ctx) {
      auto pool = transfer(NS::AutoreleasePool::alloc()->init());
      auto enc_descriptor = MTL::RenderPassDescriptor::renderPassDescriptor();

      auto attachment = enc_descriptor->colorAttachments()->object(0);
      
      attachment->setTexture(src.texture(&ctx));
      attachment->setResolveTexture(dst.texture(&ctx));
      attachment->setLoadAction(MTL::LoadActionLoad);
      attachment->setStoreAction(MTL::StoreActionMultisampleResolve);
      attachment->setSlice(SrcSlice);
      attachment->setResolveLevel(DstLevel);
      attachment->setResolveSlice(DstSlice);
      attachment->setResolveDepthPlane(0);

      auto enc = ctx.cmdbuf->renderCommandEncoder(enc_descriptor);
      enc->setLabel(NS::String::string("ResolvePass", NS::ASCIIStringEncoding));
      enc->endEncoding();
    });
  }

  /**
  Switch to render encoder and set all states (expect for pipeline state)
  */
  bool SwitchToRenderEncoder() {
    if (cmdbuf_state == CommandBufferState::RenderPipelineReady)
      return true;
    if (cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady)
      return true;
    if (cmdbuf_state == CommandBufferState::RenderEncoderActive)
      return true;
    InvalidateCurrentPass();

    // set dirty state
    state_.ShaderStages[0].ConstantBuffers.set_dirty();
    state_.ShaderStages[0].Samplers.set_dirty();
    state_.ShaderStages[0].SRVs.set_dirty();
    state_.ShaderStages[1].ConstantBuffers.set_dirty();
    state_.ShaderStages[1].Samplers.set_dirty();
    state_.ShaderStages[1].SRVs.set_dirty();
    state_.InputAssembler.VertexBuffers.set_dirty();
    dirty_state.set(DirtyState::BlendFactorAndStencilRef,
                    DirtyState::RasterizerState, DirtyState::DepthStencilState,
                    DirtyState::Viewport, DirtyState::Scissors,
                    DirtyState::IndexBuffer);

    // should assume: render target is properly set
    {
      /* Setup RenderCommandEncoder */
      CommandChunk *chk = cmd_queue.CurrentChunk();
      auto currentChunkId = cmd_queue.CurrentSeqId();

      struct RENDER_TARGET_STATE {
        BindingRef Texture;
        UINT RenderTargetIndex;
        UINT MipSlice;
        UINT ArrayIndex;
        UINT DepthPlane;
        MTL::PixelFormat PixelFormat = MTL::PixelFormatInvalid;
        MTL::LoadAction LoadAction{MTL::LoadActionLoad};
        MTL::ClearColor ClearColor{0, 0, 0, 0};
      };

      uint32_t effective_render_target = 0;
      auto rtvs =
          chk->reserve_vector<RENDER_TARGET_STATE>(state_.OutputMerger.NumRTVs);
      for (unsigned i = 0; i < state_.OutputMerger.NumRTVs; i++) {
        auto &rtv = state_.OutputMerger.RTVs[i];
        if (rtv) {
          auto props = rtv->GetRenderTargetProps();
          rtvs.push_back({rtv->GetBinding(currentChunkId), i, props->Level,
                          props->Slice, props->DepthPlane,
                          rtv->GetPixelFormat()});
          D3D11_ASSERT(rtv->GetPixelFormat() != MTL::PixelFormatInvalid);
          effective_render_target++;
        } else {
          rtvs.push_back(
              {BindingRef(std::nullopt), i, 0, 0, 0, MTL::PixelFormatInvalid});
        }
      }
      struct DEPTH_STENCIL_STATE {
        BindingRef Texture{};
        UINT MipSlice{};
        UINT ArrayIndex{};
        MTL::PixelFormat PixelFormat = MTL::PixelFormatInvalid;
        MTL::LoadAction DepthLoadAction{MTL::LoadActionLoad};
        MTL::LoadAction StencilLoadAction{MTL::LoadActionLoad};
        float ClearDepth{};
        uint8_t ClearStencil{};
      };
      // auto &dsv = state_.OutputMerger.DSV;
      DEPTH_STENCIL_STATE dsv_info;
      uint32_t uav_only_render_target_width = 0;
      uint32_t uav_only_render_target_height = 0;
      bool uav_only = false;
      uint32_t uav_only_sample_count = 0;
      if (state_.OutputMerger.DSV) {
        dsv_info.Texture = state_.OutputMerger.DSV->GetBinding(currentChunkId);
        dsv_info.PixelFormat = state_.OutputMerger.DSV->GetPixelFormat();
      } else if (effective_render_target == 0) {
        if (state_.OutputMerger.NumRTVs) {
          return false;
        }
        D3D11_ASSERT(state_.Rasterizer.NumViewports);
        IMTLD3D11RasterizerState *state =
            state_.Rasterizer.RasterizerState
                ? state_.Rasterizer.RasterizerState
                : default_rasterizer_state;
        auto &viewport = state_.Rasterizer.viewports[0];
        uav_only_render_target_width = viewport.Width;
        uav_only_render_target_height = viewport.Height;
        uav_only_sample_count = state->UAVOnlySampleCount();
        uav_only = true;
      }

      auto previous_encoder = chk->get_last_encoder();
      if (previous_encoder->kind == EncoderKind::ClearPass) {
        auto previous_clearpass = (ENCODER_CLEARPASS_INFO *)previous_encoder;
        uint32_t skip_clear_color_mask = 0;
        bool skip_clear_ds = 0;
        for (unsigned i = 0; i < previous_clearpass->num_color_attachments;
             i++) {
          for (auto &rtv : rtvs) {
            if (previous_clearpass->clear_color_attachments[i] == rtv.Texture) {
              // you think this is unnecessary? not the case in C++!
              previous_clearpass->clear_color_attachments[i] = {};
              skip_clear_color_mask |= (1 << i);
              rtv.LoadAction = MTL::LoadActionClear;
              rtv.ClearColor = previous_clearpass->clear_colors[i];
            }
          }
        }
        if (previous_clearpass->clear_depth_stencil_attachment &&
            dsv_info.Texture) {
          if (previous_clearpass->clear_depth_stencil_attachment ==
              dsv_info.Texture) {
            previous_clearpass->clear_depth_stencil_attachment = {};
            skip_clear_ds = 1;
            dsv_info.DepthLoadAction =
                previous_clearpass->depth_stencil_flags & D3D11_CLEAR_DEPTH
                    ? MTL::LoadActionClear
                    : MTL::LoadActionLoad;
            dsv_info.StencilLoadAction =
                previous_clearpass->depth_stencil_flags & D3D11_CLEAR_STENCIL
                    ? MTL::LoadActionClear
                    : MTL::LoadActionLoad;
            dsv_info.ClearDepth = previous_clearpass->clear_depth;
            dsv_info.ClearStencil = previous_clearpass->clear_stencil;
          }
        }
        if ((uint32_t)std::popcount(skip_clear_color_mask) ==
                previous_clearpass->num_color_attachments &&
            skip_clear_ds) {
          // the previous clearpass is fully skipped
          previous_clearpass->skipped = true;
        } else if (skip_clear_color_mask || skip_clear_ds) {
          // the previous clearpass is partially skipped, so we construct new
          // clearpass for remaining
          previous_clearpass->skipped = true;
          auto remain_clearpass = chk->mark_clear_pass();
          for (unsigned i = 0; i < previous_clearpass->num_color_attachments;
               i++) {
            if (skip_clear_color_mask & (1 << i))
              continue;
            auto j = remain_clearpass->num_color_attachments;
            remain_clearpass->num_color_attachments++;
            remain_clearpass->clear_color_attachments[j] =
                std::move(previous_clearpass->clear_color_attachments[i]);
            remain_clearpass->clear_colors[j] =
                std::move(previous_clearpass->clear_colors[i]);
          }
          if (!skip_clear_ds) {
            remain_clearpass->clear_depth_stencil_attachment =
                std::move(previous_clearpass->clear_depth_stencil_attachment);
            remain_clearpass->depth_stencil_flags =
                previous_clearpass->depth_stencil_flags;
            remain_clearpass->clear_depth = previous_clearpass->clear_depth;
            remain_clearpass->clear_stencil = previous_clearpass->clear_stencil;
          }
          EncodeClearPass(remain_clearpass);
        } else {
          // previous_clearpass kept untouched
        }
      };

      auto pass_info = chk->mark_render_pass();

      auto bump_offset = NextOcclusionQuerySeq() % kOcclusionSampleCount;

      chk->emit([rtvs = std::move(rtvs), dsv = std::move(dsv_info), bump_offset,
                 effective_render_target, uav_only,
                 uav_only_render_target_height, uav_only_render_target_width,
                 uav_only_sample_count, pass_info](CommandChunk::context &ctx) {
        auto pool = transfer(NS::AutoreleasePool::alloc()->init());
        auto renderPassDescriptor =
            MTL::RenderPassDescriptor::renderPassDescriptor();
        for (auto &rtv : rtvs) {
          if (rtv.PixelFormat == MTL::PixelFormatInvalid) {
            continue;
          }
          auto colorAttachment =
              renderPassDescriptor->colorAttachments()->object(
                  rtv.RenderTargetIndex);
          colorAttachment->setTexture(rtv.Texture.texture(&ctx));
          colorAttachment->setLevel(rtv.MipSlice);
          colorAttachment->setSlice(rtv.ArrayIndex);
          colorAttachment->setDepthPlane(rtv.DepthPlane);
          colorAttachment->setLoadAction(rtv.LoadAction);
          colorAttachment->setClearColor(rtv.ClearColor);
          colorAttachment->setStoreAction(MTL::StoreActionStore);
        };
        uint32_t dsv_planar_flags = 0;

        if (dsv.Texture) {
          dsv_planar_flags = DepthStencilPlanarFlags(dsv.PixelFormat);
          MTL::Texture *texture = dsv.Texture.texture(&ctx);
          // TODO: ...should know more about store behavior (e.g. DiscardView)
          if (dsv_planar_flags & 1) {
            auto depthAttachment = renderPassDescriptor->depthAttachment();
            depthAttachment->setTexture(texture);
            depthAttachment->setLevel(dsv.MipSlice);
            depthAttachment->setSlice(dsv.ArrayIndex);
            depthAttachment->setLoadAction(dsv.DepthLoadAction);
            depthAttachment->setClearDepth(dsv.ClearDepth);
            depthAttachment->setStoreAction(MTL::StoreActionStore);
          }

          if (dsv_planar_flags & 2) {
            auto stencilAttachment = renderPassDescriptor->stencilAttachment();
            stencilAttachment->setTexture(texture);
            stencilAttachment->setLevel(dsv.MipSlice);
            stencilAttachment->setSlice(dsv.ArrayIndex);
            stencilAttachment->setLoadAction(dsv.StencilLoadAction);
            stencilAttachment->setClearStencil(dsv.ClearStencil);
            stencilAttachment->setStoreAction(MTL::StoreActionStore);
          }
        }
        if (effective_render_target == 0) {
          if (uav_only) {
            renderPassDescriptor->setRenderTargetHeight(
                uav_only_render_target_height);
            renderPassDescriptor->setRenderTargetWidth(
                uav_only_render_target_width);
            renderPassDescriptor->setDefaultRasterSampleCount(
                uav_only_sample_count);
          } else {
            D3D11_ASSERT(dsv_planar_flags);
            auto dsv_tex = dsv.Texture.texture(&ctx);
            renderPassDescriptor->setRenderTargetHeight(dsv_tex->height());
            renderPassDescriptor->setRenderTargetWidth(dsv_tex->width());
          }
        }
        renderPassDescriptor->setVisibilityResultBuffer(
            ctx.chk->visibility_result_heap);
        ctx.render_encoder =
            ctx.cmdbuf->renderCommandEncoder(renderPassDescriptor);
        auto [h, _] = ctx.chk->inspect_gpu_heap();
        ctx.dsv_planar_flags = dsv_planar_flags;
        D3D11_ASSERT(ctx.render_encoder);
        ctx.render_encoder->setVertexBuffer(h, 0, 16);
        ctx.render_encoder->setVertexBuffer(h, 0, 29);
        ctx.render_encoder->setVertexBuffer(h, 0, 30);
        ctx.render_encoder->setFragmentBuffer(h, 0, 29);
        ctx.render_encoder->setFragmentBuffer(h, 0, 30);
        if (pass_info->tessellation_pass) {
          ctx.render_encoder->setMeshBuffer(h, 0, 29);
          ctx.render_encoder->setMeshBuffer(h, 0, 30);
          ctx.render_encoder->setObjectBuffer(h, 0, 16);
          ctx.render_encoder->setObjectBuffer(h, 0, 29);
          ctx.render_encoder->setObjectBuffer(h, 0, 30);
        }
        // TODO: need to check if there is any query in building
        ctx.render_encoder->setVisibilityResultMode(
            MTL::VisibilityResultModeCounting, bump_offset << 3);
      });
    }

    cmdbuf_state = CommandBufferState::RenderEncoderActive;
    return true;
  }

  /**
  Switch to blit encoder
  */
  void SwitchToBlitEncoder(CommandBufferState BlitKind) {
    if (cmdbuf_state == BlitKind)
      return;
    InvalidateCurrentPass();

    {
      /* Setup ComputeCommandEncoder */
      CommandChunk *chk = cmd_queue.CurrentChunk();

      chk->mark_pass(EncoderKind::Blit);

      chk->emit([](CommandChunk::context &ctx) {
        ctx.blit_encoder = ctx.cmdbuf->blitCommandEncoder();
      });
    }

    cmdbuf_state = BlitKind;
  }

  /**
  Switch to compute encoder and set all states (expect for pipeline state)
  */
  void SwitchToComputeEncoder() {
    if (cmdbuf_state == CommandBufferState::ComputePipelineReady)
      return;
    if (cmdbuf_state == CommandBufferState::ComputeEncoderActive)
      return;
    InvalidateCurrentPass();

    // set dirty state
    state_.ShaderStages[5].ConstantBuffers.set_dirty();
    state_.ShaderStages[5].Samplers.set_dirty();
    state_.ShaderStages[5].SRVs.set_dirty();

    {
      /* Setup ComputeCommandEncoder */
      CommandChunk *chk = cmd_queue.CurrentChunk();

      chk->mark_pass(EncoderKind::Compute);

      chk->emit([](CommandChunk::context &ctx) {
        ctx.compute_encoder = ctx.cmdbuf->computeCommandEncoder();
        auto [heap, offset] = ctx.chk->inspect_gpu_heap();
        ctx.compute_encoder->setBuffer(heap, 0, 29);
        ctx.compute_encoder->setBuffer(heap, 0, 30);
      });
    }

    cmdbuf_state = CommandBufferState::ComputeEncoderActive;
  }

  template<bool IndexedDraw>
  bool FinalizeNoRasterizationRenderPipeline(IMTLD3D11Shader *pVertexShader) {

    if (!SwitchToRenderEncoder()) {
      return false;
    }

    CommandChunk *chk = cmd_queue.CurrentChunk();

    Com<IMTLCompiledGraphicsPipeline> pipeline;
    Com<IMTLCompiledShader> vs;
    MTL_GRAPHICS_PIPELINE_DESC pipelineDesc;
    pipelineDesc.VertexShader = pVertexShader;
    pipelineDesc.PixelShader = nullptr;
    pipelineDesc.HullShader = nullptr;
    pipelineDesc.DomainShader = nullptr;
    pipelineDesc.InputLayout = state_.InputAssembler.InputLayout;
    pipelineDesc.NumColorAttachments = 0;
    memset(pipelineDesc.ColorAttachmentFormats, 0,
           sizeof(pipelineDesc.ColorAttachmentFormats));
    pipelineDesc.BlendState = default_blend_state;
    pipelineDesc.DepthStencilFormat = MTL::PixelFormatInvalid;
    pipelineDesc.RasterizationEnabled = false;
    pipelineDesc.SampleMask = D3D11_DEFAULT_SAMPLE_MASK;
    if constexpr (IndexedDraw) {
      pipelineDesc.IndexBufferFormat =
          state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT
              ? SM50_INDEX_BUFFER_FORMAT_UINT32
              : SM50_INDEX_BUFFER_FORMAT_UINT16;
    } else {
      pipelineDesc.IndexBufferFormat = SM50_INDEX_BUFFER_FORMAT_NONE;
    }

    device->CreateGraphicsPipeline(&pipelineDesc, &pipeline);

    chk->emit([pso = std::move(pipeline)](CommandChunk::context &ctx) {
      MTL_COMPILED_GRAPHICS_PIPELINE GraphicsPipeline {};
      pso->GetPipeline(&GraphicsPipeline); // may block
      D3D11_ASSERT(GraphicsPipeline.PipelineState);
      ctx.render_encoder->setRenderPipelineState(
          GraphicsPipeline.PipelineState);
    });

    cmdbuf_state = CommandBufferState::RenderPipelineReady;
    return true;
  };

  template<bool IndexedDraw>
  bool FinalizeTessellationRenderPipeline() {

    if (cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady)
      return true;
    if (!state_.ShaderStages[(UINT)ShaderType::Hull].Shader) {
      return false;
    }
    if (!state_.ShaderStages[(UINT)ShaderType::Domain].Shader) {
      return false;
    }
    if (state_.ShaderStages[(UINT)ShaderType::Geometry].Shader) {
      ERR("tessellation-geometry pipeline is not supported yet, skip drawcall");
      return false;
    }

    if (!SwitchToRenderEncoder()) {
      return false;
    }

    CommandChunk *chk = cmd_queue.CurrentChunk();

    auto previous_encoder = chk->get_last_encoder();
    if (previous_encoder->kind == EncoderKind::Render) {
      auto previous_clearpass = (ENCODER_RENDER_INFO *)previous_encoder;
      previous_clearpass->tessellation_pass = 1;
    }

    Com<IMTLCompiledTessellationPipeline> pipeline;
    Com<IMTLCompiledShader> vs, ps;

    MTL_GRAPHICS_PIPELINE_DESC pipelineDesc;
    pipelineDesc.VertexShader =
        state_.ShaderStages[(UINT)ShaderType::Vertex].Shader.ptr();
    pipelineDesc.PixelShader =
        state_.ShaderStages[(UINT)ShaderType::Pixel].Shader.ptr();
    pipelineDesc.HullShader =
        state_.ShaderStages[(UINT)ShaderType::Hull].Shader.ptr();
    pipelineDesc.DomainShader =
        state_.ShaderStages[(UINT)ShaderType::Domain].Shader.ptr();
    pipelineDesc.InputLayout = state_.InputAssembler.InputLayout;
    pipelineDesc.NumColorAttachments = state_.OutputMerger.NumRTVs;
    for (unsigned i = 0; i < ARRAYSIZE(state_.OutputMerger.RTVs); i++) {
      auto &rtv = state_.OutputMerger.RTVs[i];
      if (rtv && i < pipelineDesc.NumColorAttachments) {
        pipelineDesc.ColorAttachmentFormats[i] =
            state_.OutputMerger.RTVs[i]->GetPixelFormat();
      } else {
        pipelineDesc.ColorAttachmentFormats[i] = MTL::PixelFormatInvalid;
      }
    }
    pipelineDesc.BlendState = state_.OutputMerger.BlendState
                                  ? state_.OutputMerger.BlendState
                                  : default_blend_state;
    pipelineDesc.DepthStencilFormat =
        state_.OutputMerger.DSV ? state_.OutputMerger.DSV->GetPixelFormat()
                                : MTL::PixelFormatInvalid;
    pipelineDesc.RasterizationEnabled = true;
    pipelineDesc.SampleMask = state_.OutputMerger.SampleMask;
    if constexpr (IndexedDraw) {
      pipelineDesc.IndexBufferFormat =
          state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT
              ? SM50_INDEX_BUFFER_FORMAT_UINT32
              : SM50_INDEX_BUFFER_FORMAT_UINT16;
    } else {
      pipelineDesc.IndexBufferFormat = SM50_INDEX_BUFFER_FORMAT_NONE;
    }

    device->CreateTessellationPipeline(&pipelineDesc, &pipeline);

    chk->emit([pso = std::move(pipeline)](CommandChunk::context &ctx) {
      MTL_COMPILED_TESSELLATION_PIPELINE GraphicsPipeline{};
      pso->GetPipeline(&GraphicsPipeline); // may block
      D3D11_ASSERT(GraphicsPipeline.MeshPipelineState);
      D3D11_ASSERT(GraphicsPipeline.RasterizationPipelineState);
      ctx.tess_mesh_pso = GraphicsPipeline.MeshPipelineState;
      ctx.tess_raster_pso = GraphicsPipeline.RasterizationPipelineState;
      ctx.tess_num_output_control_point_element =
          GraphicsPipeline.NumControlPointOutputElement;
      ctx.tess_num_output_patch_constant_scalar =
          GraphicsPipeline.NumPatchConstantOutputScalar;
      ctx.tess_threads_per_patch =
          GraphicsPipeline.ThreadsPerPatch;
    });

    cmdbuf_state = CommandBufferState::TessellationRenderPipelineReady;
    return true;
  }

  /**
  Assume we have all things needed to build PSO
  If the current encoder is not a render encoder, switch to it.
  */
  template<bool IndexedDraw>
  bool FinalizeCurrentRenderPipeline() {
    if (state_.ShaderStages[(UINT)ShaderType::Hull].Shader) {
      return FinalizeTessellationRenderPipeline<IndexedDraw>();
    }
    if (cmdbuf_state == CommandBufferState::RenderPipelineReady)
      return true;
    if (state_.ShaderStages[(UINT)ShaderType::Geometry].Shader) {
      if (!state_.ShaderStages[(UINT)ShaderType::Pixel].Shader) {
        return FinalizeNoRasterizationRenderPipeline<IndexedDraw>(
            state_.ShaderStages[(UINT)ShaderType::Geometry].Shader.ptr());
      }
      // ERR("geometry shader is not supported yet, skip drawcall");
      return false;
    }

    if (!SwitchToRenderEncoder()) {
      return false;
    }

    CommandChunk *chk = cmd_queue.CurrentChunk();

    Com<IMTLCompiledGraphicsPipeline> pipeline;

    MTL_GRAPHICS_PIPELINE_DESC pipelineDesc;
    pipelineDesc.VertexShader = state_.ShaderStages[(UINT)ShaderType::Vertex]
            .Shader.ptr();
    pipelineDesc.PixelShader = state_.ShaderStages[(UINT)ShaderType::Pixel]
            .Shader.ptr();
    pipelineDesc.HullShader = nullptr;
    pipelineDesc.DomainShader = nullptr;
    pipelineDesc.InputLayout = state_.InputAssembler.InputLayout;
    pipelineDesc.NumColorAttachments = state_.OutputMerger.NumRTVs;
    for (unsigned i = 0; i < ARRAYSIZE(state_.OutputMerger.RTVs); i++) {
      auto &rtv = state_.OutputMerger.RTVs[i];
      if (rtv && i < pipelineDesc.NumColorAttachments) {
        pipelineDesc.ColorAttachmentFormats[i] =
            state_.OutputMerger.RTVs[i]->GetPixelFormat();
      } else {
        pipelineDesc.ColorAttachmentFormats[i] = MTL::PixelFormatInvalid;
      }
    }
    pipelineDesc.BlendState = state_.OutputMerger.BlendState
                                   ? state_.OutputMerger.BlendState
                                   : default_blend_state;
    pipelineDesc.DepthStencilFormat =
        state_.OutputMerger.DSV ? state_.OutputMerger.DSV->GetPixelFormat()
                                : MTL::PixelFormatInvalid;
    pipelineDesc.RasterizationEnabled = true;
    pipelineDesc.SampleMask = state_.OutputMerger.SampleMask;
    if constexpr (IndexedDraw) {
      pipelineDesc.IndexBufferFormat =
          state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT
              ? SM50_INDEX_BUFFER_FORMAT_UINT32
              : SM50_INDEX_BUFFER_FORMAT_UINT16;
    } else {
      pipelineDesc.IndexBufferFormat = SM50_INDEX_BUFFER_FORMAT_NONE;
    }

    device->CreateGraphicsPipeline(&pipelineDesc, &pipeline);

    chk->emit([pso = std::move(pipeline)](CommandChunk::context &ctx) {
      MTL_COMPILED_GRAPHICS_PIPELINE GraphicsPipeline {};
      pso->GetPipeline(&GraphicsPipeline); // may block
      D3D11_ASSERT(GraphicsPipeline.PipelineState);
      ctx.render_encoder->setRenderPipelineState(
          GraphicsPipeline.PipelineState);
    });

    cmdbuf_state = CommandBufferState::RenderPipelineReady;
    return true;
  }


  template<bool IndexedDraw>
  bool PreDraw() {
    if (!FinalizeCurrentRenderPipeline<IndexedDraw>()) {
      return false;
    }
    CommandChunk *chk = cmd_queue.CurrentChunk();
    UpdateVertexBuffer();
    UpdateSOTargets();
    if (dirty_state.any(DirtyState::DepthStencilState)) {
      IMTLD3D11DepthStencilState *state =
          state_.OutputMerger.DepthStencilState
              ? state_.OutputMerger.DepthStencilState
              : default_depth_stencil_state;
      chk->emit([state, stencil_ref = state_.OutputMerger.StencilRef](
                    CommandChunk::context &ctx) {
        auto encoder = ctx.render_encoder;
        encoder->setDepthStencilState(
            state->GetDepthStencilState(ctx.dsv_planar_flags));
        encoder->setStencilReferenceValue(stencil_ref);
      });
    }
    if (dirty_state.any(DirtyState::RasterizerState)) {
      IMTLD3D11RasterizerState *state =
          state_.Rasterizer.RasterizerState
              ? state_.Rasterizer.RasterizerState
              : default_rasterizer_state;
      chk->emit([state](CommandChunk::context &ctx) {
        auto &encoder = ctx.render_encoder;
        state->SetupRasterizerState(encoder);
      });
    }
    if (dirty_state.any(DirtyState::BlendFactorAndStencilRef)) {
      chk->emit([r = state_.OutputMerger.BlendFactor[0],
                 g = state_.OutputMerger.BlendFactor[1],
                 b = state_.OutputMerger.BlendFactor[2],
                 a = state_.OutputMerger.BlendFactor[3],
                 stencil_ref = state_.OutputMerger.StencilRef](
                    CommandChunk::context &ctx) {
        auto &encoder = ctx.render_encoder;
        encoder->setBlendColor(r, g, b, a);
        encoder->setStencilReferenceValue(stencil_ref);
      });
    }
    IMTLD3D11RasterizerState *current_rs =
        state_.Rasterizer.RasterizerState
            ? state_.Rasterizer.RasterizerState
            : default_rasterizer_state;
    bool allow_scissor =
        current_rs->IsScissorEnabled() &&
        state_.Rasterizer.NumViewports == state_.Rasterizer.NumScissorRects;
    // FIXME: multiple viewports with one scissor should be allowed?
    if (current_rs->IsScissorEnabled() && state_.Rasterizer.NumViewports > 1 &&
        state_.Rasterizer.NumScissorRects == 1) {
      ERR("FIXME: handle multiple viewports with single scissor rect.");
    }
    if (dirty_state.any(DirtyState::Viewport)) {
      auto viewports =
          chk->reserve_vector<MTL::Viewport>(state_.Rasterizer.NumViewports);
      for (unsigned i = 0; i < state_.Rasterizer.NumViewports; i++) {
        auto &d3dViewport = state_.Rasterizer.viewports[i];
        viewports.push_back({d3dViewport.TopLeftX, d3dViewport.TopLeftY,
                             d3dViewport.Width, d3dViewport.Height,
                             d3dViewport.MinDepth, d3dViewport.MaxDepth});
      }
      chk->emit([viewports = std::move(viewports)](CommandChunk::context &ctx) {
        auto &encoder = ctx.render_encoder;
        encoder->setViewports(viewports.data(), viewports.size());
      });
    }
    if (dirty_state.any(DirtyState::Scissors)) {
      auto scissors = chk->reserve_vector<MTL::ScissorRect>(
          allow_scissor ? state_.Rasterizer.NumScissorRects
                        : state_.Rasterizer.NumViewports);
      if (allow_scissor) {
        for (unsigned i = 0; i < state_.Rasterizer.NumScissorRects; i++) {
          auto &d3dRect = state_.Rasterizer.scissor_rects[i];
          scissors.push_back({(UINT)d3dRect.left, (UINT)d3dRect.top,
                              (UINT)d3dRect.right - d3dRect.left,
                              (UINT)d3dRect.bottom - d3dRect.top});
        }
      } else {
        for (unsigned i = 0; i < state_.Rasterizer.NumViewports; i++) {
          auto &d3dViewport = state_.Rasterizer.viewports[i];
          scissors.push_back(
              {(UINT)d3dViewport.TopLeftX, (UINT)d3dViewport.TopLeftY,
               (UINT)d3dViewport.Width, (UINT)d3dViewport.Height});
        }
      }
      chk->emit([scissors = std::move(scissors)](CommandChunk::context &ctx) {
        auto &encoder = ctx.render_encoder;
        encoder->setScissorRects(scissors.data(), scissors.size());
      });
    }
    if (dirty_state.any(DirtyState::IndexBuffer)) {
      // we should be able to retrieve it from chunk
      auto seq_id = cmd_queue.CurrentSeqId();
      if (state_.InputAssembler.IndexBuffer) {
        chk->emit([buffer = state_.InputAssembler.IndexBuffer->UseBindable(
                       seq_id)](CommandChunk::context &ctx) {
          ctx.current_index_buffer_ref = buffer.buffer();
        });
      } else {
        chk->emit([](CommandChunk::context &ctx) {
          ctx.current_index_buffer_ref = nullptr;
        });
      }
    }
    dirty_state.clrAll();
    if (cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady) {
      return UploadShaderStageResourceBinding<ShaderType::Vertex, true>() &&
             UploadShaderStageResourceBinding<ShaderType::Pixel, true>() &&
             UploadShaderStageResourceBinding<ShaderType::Hull, true>() &&
             UploadShaderStageResourceBinding<ShaderType::Domain, true>();
    }
    return UploadShaderStageResourceBinding<ShaderType::Vertex, false>() &&
           UploadShaderStageResourceBinding<ShaderType::Pixel, false>();
  }

  /**
  Assume we have all things needed to build PSO
  If the current encoder is not a compute encoder, switch to it.
  */
  bool FinalizeCurrentComputePipeline() {
    if (cmdbuf_state == CommandBufferState::ComputePipelineReady)
      return true;

    SwitchToComputeEncoder();

    Com<IMTLCompiledShader> shader;
    state_.ShaderStages[(UINT)ShaderType::Compute]
        .Shader //
        ->GetCompiledShader(&shader);
    if (!shader) {
      ERR("Shader not found?");
      return false;
    }
    MTL_COMPILED_SHADER shader_data;
    shader->GetShader(&shader_data);
    Com<IMTLCompiledComputePipeline> pipeline;
    device->CreateComputePipeline(shader.ptr(), &pipeline);

    const MTL_SHADER_REFLECTION *reflection =
        state_.ShaderStages[(UINT)ShaderType::Compute].Shader->GetReflection();

    CommandChunk *chk = cmd_queue.CurrentChunk();
    chk->emit([pso = std::move(pipeline),
               tg_size = MTL::Size::Make(reflection->ThreadgroupSize[0],
                                         reflection->ThreadgroupSize[1],
                                         reflection->ThreadgroupSize[2])](
                  CommandChunk::context &ctx) {
      MTL_COMPILED_COMPUTE_PIPELINE ComputePipeline;
      pso->GetPipeline(&ComputePipeline); // may block
      ctx.compute_encoder->setComputePipelineState(
          ComputePipeline.PipelineState);
      ctx.cs_threadgroup_size = tg_size;
    });

    cmdbuf_state = CommandBufferState::ComputePipelineReady;
    return true;
  }

  bool PreDispatch() {
    if (!FinalizeCurrentComputePipeline()) {
      return false;
    }
    UploadShaderStageResourceBinding<ShaderType::Compute, false>();
    return true;
  }

  template <ShaderType stage, bool Tessellation>
  bool UploadShaderStageResourceBinding() {
    auto &ShaderStage = state_.ShaderStages[(UINT)stage];
    if (!ShaderStage.Shader) {
      return true;
    }
    auto &UAVBindingSet = stage == ShaderType::Compute
                              ? state_.ComputeStageUAV.UAVs
                              : state_.OutputMerger.UAVs;

    const MTL_SHADER_REFLECTION *reflection =
        ShaderStage.Shader->GetReflection();

    bool dirty_cbuffer = ShaderStage.ConstantBuffers.any_dirty_masked(
        reflection->ConstantBufferSlotMask);
    bool dirty_sampler =
        ShaderStage.Samplers.any_dirty_masked(reflection->SamplerSlotMask);
    bool dirty_srv = ShaderStage.SRVs.any_dirty_masked(
        reflection->SRVSlotMaskHi, reflection->SRVSlotMaskLo);
    bool dirty_uav = UAVBindingSet.any_dirty_masked(reflection->UAVSlotMask);
    if (!dirty_cbuffer && !dirty_sampler && !dirty_srv && !dirty_uav)
      return true;

    auto currentChunkId = cmd_queue.CurrentSeqId();

    auto ConstantBufferCount = reflection->NumConstantBuffers;
    auto BindingCount = reflection->NumArguments;
    CommandChunk *chk = cmd_queue.CurrentChunk();
    auto encoderId = chk->current_encoder_id();

    auto useResource = [&](BindingRef &&res,
                           MTL_BINDABLE_RESIDENCY_MASK residencyMask) {
      chk->emit(
          [res = std::move(res), residencyMask](CommandChunk::context &ctx) {
            switch (stage) {
            case ShaderType::Vertex:
            case ShaderType::Pixel:
            case ShaderType::Hull:
            case ShaderType::Domain:
              ctx.render_encoder->useResource(
                  res.resource(&ctx), GetUsageFromResidencyMask(residencyMask),
                  GetStagesFromResidencyMask(residencyMask));
              break;
            case ShaderType::Compute:
              ctx.compute_encoder->useResource(
                  res.resource(&ctx), GetUsageFromResidencyMask(residencyMask));
              break;
            case ShaderType::Geometry:
              D3D11_ASSERT(0 && "Not implemented");
              break;
            }
          });
    };
    if (ConstantBufferCount && dirty_cbuffer) {

      auto [heap, offset] =
          chk->allocate_gpu_heap(ConstantBufferCount << 3, 16);
      uint64_t *write_to_it = chk->gpu_argument_heap_contents + (offset >> 3);

      for (unsigned i = 0; i < ConstantBufferCount; i++) {
        auto &arg = reflection->ConstantBuffers[i];
        auto slot = arg.SM50BindingSlot;
        MTL_BINDABLE_RESIDENCY_MASK newResidencyMask = MTL_RESIDENCY_NULL;
        SIMPLE_RESIDENCY_TRACKER *pTracker;
        switch (arg.Type) {
        case SM50BindingType::ConstantBuffer: {
          if (!ShaderStage.ConstantBuffers.test_bound(slot)) {
            ERR("expect constant buffer at slot ", slot, " but none is bound.");
            return false;
          }
          auto &cbuf = ShaderStage.ConstantBuffers[slot];
          auto argbuf = cbuf.Buffer->GetArgumentData(&pTracker);
          write_to_it[arg.StructurePtrOffset] =
              argbuf.buffer() + (cbuf.FirstConstant << 4);
          ShaderStage.ConstantBuffers.clear_dirty(slot);
          pTracker->CheckResidency(
              encoderId, GetResidencyMask<Tessellation>(stage, true, false),
              &newResidencyMask);
          if (newResidencyMask) {
            useResource(cbuf.Buffer->UseBindable(currentChunkId),
                        newResidencyMask);
          }
          break;
        }
        default:
          D3D11_ASSERT(0 && "unreachable");
        }
      }

      /* kConstantBufferTableBinding = 29 */
      chk->emit([offset](CommandChunk::context &ctx) {
        if constexpr (stage == ShaderType::Vertex) {
          if constexpr (Tessellation) {
            ctx.render_encoder->setObjectBufferOffset(offset, 29);
          } else {
            ctx.render_encoder->setVertexBufferOffset(offset, 29);
          }
        } else if constexpr (stage == ShaderType::Pixel) {
          ctx.render_encoder->setFragmentBufferOffset(offset, 29);
        } else if constexpr (stage == ShaderType::Compute) {
          ctx.compute_encoder->setBufferOffset(offset, 29);
        } else if constexpr (stage == ShaderType::Hull) {
          ctx.render_encoder->setMeshBufferOffset(offset, 29);
        } else if constexpr (stage == ShaderType::Domain) {
          ctx.render_encoder->setVertexBufferOffset(offset, 29);
        } else {
          D3D11_ASSERT(0 && "Not implemented");
        }
      });
    }

    if (BindingCount && (dirty_sampler || dirty_srv || dirty_uav)) {

      /* FIXME: we are over-allocating */
      auto [heap, offset] = chk->allocate_gpu_heap(BindingCount * 8 * 3, 16);
      uint64_t *write_to_it = chk->gpu_argument_heap_contents + (offset >> 3);

      for (unsigned i = 0; i < BindingCount; i++) {
        auto &arg = reflection->Arguments[i];
        auto slot = arg.SM50BindingSlot;
        MTL_BINDABLE_RESIDENCY_MASK newResidencyMask = MTL_RESIDENCY_NULL;
        SIMPLE_RESIDENCY_TRACKER *pTracker;
        switch (arg.Type) {
        case SM50BindingType::ConstantBuffer: {
          D3D11_ASSERT(0 && "unreachable");
        }
        case SM50BindingType::Sampler: {
          if (!ShaderStage.Samplers.test_bound(slot)) {
            ERR("expect sample at slot ", slot, " but none is bound.");
            return false;
          }
          auto &sampler = ShaderStage.Samplers[slot];
          write_to_it[arg.StructurePtrOffset] =
              sampler.Sampler->GetArgumentHandle();
          ShaderStage.Samplers.clear_dirty(slot);
          break;
        }
        case SM50BindingType::SRV: {
          if (!ShaderStage.SRVs.test_bound(slot)) {
            // TODO: debug only
            // ERR("expect shader resource at slot ", slot, " but none is
            // bound.");
            write_to_it[arg.StructurePtrOffset] = 0;
            // ? we are doing something dangerous
            // need to verify that 0 have defined behavior (e.g. no gpu crash)
            if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH) {
              write_to_it[arg.StructurePtrOffset + 1] = 0;
            }
            break;
          }
          auto &srv = ShaderStage.SRVs[slot];
          auto arg_data = srv.SRV->GetArgumentData(&pTracker);
          if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) {
            write_to_it[arg.StructurePtrOffset] = arg_data.buffer();
          }
          if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH) {
            write_to_it[arg.StructurePtrOffset + 1] = arg_data.width();
          }
          if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE) {
            if (arg_data.requiresContext()) {
              chk->emit([=, ref = srv.SRV](CommandChunk::context &ctx) {
                write_to_it[arg.StructurePtrOffset] = arg_data.resource(&ctx);
              });
            } else {
              write_to_it[arg.StructurePtrOffset] = arg_data.texture();
            }
          }
          if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
            D3D11_ASSERT(0 && "srv can not have counter associated");
          }
          ShaderStage.SRVs.clear_dirty(slot);
          pTracker->CheckResidency(
              encoderId, GetResidencyMask<Tessellation>(stage, true, false),
              &newResidencyMask);
          if (newResidencyMask) {
            useResource(srv.SRV->UseBindable(currentChunkId), newResidencyMask);
          }
          break;
        }
        case SM50BindingType::UAV: {
          if constexpr (stage == ShaderType::Vertex) {
            ERR("uav in vertex shader! need to workaround");
            return false;
          }
          // FIXME: currently only pixel shader use uav from OM
          // REFACTOR NEEDED
          // TODO: consider separately handle uav
          if (!UAVBindingSet.test_bound(arg.SM50BindingSlot)) {
            // ERR("expect uav at slot ", arg.SM50BindingSlot,
            //     " but none is bound.");
            write_to_it[arg.StructurePtrOffset] = 0;
            if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH) {
              write_to_it[arg.StructurePtrOffset + 1] = 0;
            }
            if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
              write_to_it[arg.StructurePtrOffset + 2] = 0;
            }
            break;
          }
          auto &uav = UAVBindingSet[arg.SM50BindingSlot];
          auto arg_data = uav.View->GetArgumentData(&pTracker);
          if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_BUFFER) {
            write_to_it[arg.StructurePtrOffset] = arg_data.buffer();
          }
          if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_ELEMENT_WIDTH) {
            write_to_it[arg.StructurePtrOffset + 1] = arg_data.width();
          }
          if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_TEXTURE) {
            if (arg_data.requiresContext()) {
              D3D11_ASSERT(0 && "todo");
            } else {
              write_to_it[arg.StructurePtrOffset] = arg_data.texture();
            }
          }
          if (arg.Flags & MTL_SM50_SHADER_ARGUMENT_UAV_COUNTER) {
            auto counter_handle = arg_data.counter_handle();
            if (counter_handle != DXMT_NO_COUNTER) {
              auto counter = cmd_queue.counter_pool.GetCounter(counter_handle);
              chk->emit([counter](CommandChunk::context &ctx) {
                switch (stage) {
                case ShaderType::Vertex:
                case ShaderType::Pixel:
                case ShaderType::Hull:
                case ShaderType::Domain:
                  ctx.render_encoder->useResource(counter.Buffer,
                                                  MTL::ResourceUsageRead |
                                                      MTL::ResourceUsageWrite);
                  break;
                case ShaderType::Compute:
                  ctx.compute_encoder->useResource(counter.Buffer,
                                                   MTL::ResourceUsageRead |
                                                       MTL::ResourceUsageWrite);
                  break;
                case ShaderType::Geometry:
                  D3D11_ASSERT(0 && "Not implemented");
                  break;
                }
              });
              write_to_it[arg.StructurePtrOffset + 2] =
                  counter.Buffer->gpuAddress() + counter.Offset;
            } else {
              ERR("use uninitialized counter!");
              write_to_it[arg.StructurePtrOffset + 2] = 0;
            };
          }
          UAVBindingSet.clear_dirty(slot);
          pTracker->CheckResidency(encoderId,
                                   GetResidencyMask<Tessellation>(
                                       stage,
                                       // FIXME: don't use literal constant...
                                       (arg.Flags >> 10) & 1,
                                       (arg.Flags >> 10) & 2),
                                   &newResidencyMask);
          if (newResidencyMask) {
            useResource(uav.View->UseBindable(currentChunkId),
                        newResidencyMask);
          }
          break;
        }
        }
      }

      /* kArgumentBufferBinding = 30 */
      chk->emit([offset](CommandChunk::context &ctx) {
        if constexpr (stage == ShaderType::Vertex) {
          if constexpr (Tessellation) {
            ctx.render_encoder->setObjectBufferOffset(offset, 30);
          } else {
            ctx.render_encoder->setVertexBufferOffset(offset, 30);
          }
        } else if constexpr (stage == ShaderType::Pixel) {
          ctx.render_encoder->setFragmentBufferOffset(offset, 30);
        } else if constexpr (stage == ShaderType::Compute) {
          ctx.compute_encoder->setBufferOffset(offset, 30);
        } else if constexpr (stage == ShaderType::Hull) {
          ctx.render_encoder->setMeshBufferOffset(offset, 30);
        } else if constexpr (stage == ShaderType::Domain) {
          ctx.render_encoder->setVertexBufferOffset(offset, 30);
        } else {
          D3D11_ASSERT(0 && "Not implemented");
        }
      });
    }
    return true;
  }

  template <typename Fn> void EmitRenderCommand(Fn &&fn) {
    D3D11_ASSERT(cmdbuf_state == CommandBufferState::RenderEncoderActive ||
                 cmdbuf_state == CommandBufferState::RenderPipelineReady ||
                 cmdbuf_state ==
                     CommandBufferState::TessellationRenderPipelineReady);
    CommandChunk *chk = cmd_queue.CurrentChunk();
    chk->emit([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) {
      std::invoke(fn, ctx.render_encoder.ptr());
    });
  };

  template <typename Fn> void EmitRenderCommandChk(Fn &&fn) {
    D3D11_ASSERT(cmdbuf_state == CommandBufferState::RenderEncoderActive ||
                 cmdbuf_state == CommandBufferState::RenderPipelineReady ||
                 cmdbuf_state ==
                     CommandBufferState::TessellationRenderPipelineReady);
    CommandChunk *chk = cmd_queue.CurrentChunk();
    chk->emit([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) {
      std::invoke(fn, ctx);
    });
  };

  template <bool Force = false, typename Fn> void EmitComputeCommand(Fn &&fn) {
    if (Force) {
      SwitchToComputeEncoder();
    }
    if (cmdbuf_state == CommandBufferState::ComputeEncoderActive ||
        cmdbuf_state == CommandBufferState::ComputePipelineReady) {
      CommandChunk *chk = cmd_queue.CurrentChunk();
      chk->emit([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) {
        std::invoke(fn, ctx.compute_encoder.ptr(), ctx.cs_threadgroup_size);
      });
    }
  };

  template <bool Force = false, typename Fn>
  void EmitComputeCommandChk(Fn &&fn) {
    if (Force) {
      SwitchToComputeEncoder();
    }
    if (cmdbuf_state == CommandBufferState::ComputeEncoderActive ||
        cmdbuf_state == CommandBufferState::ComputePipelineReady) {
      CommandChunk *chk = cmd_queue.CurrentChunk();
      chk->emit([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) {
        std::invoke(fn, ctx.compute_encoder.ptr(), ctx);
      });
    }
  };

  template <bool Force = false, typename Fn>
  void EmitBlitCommand(Fn &&fn, CommandBufferState BlitKind =
                                    CommandBufferState::BlitEncoderActive) {
    if (Force) {
      SwitchToBlitEncoder(BlitKind);
    }
    if (cmdbuf_state == BlitKind) {
      CommandChunk *chk = cmd_queue.CurrentChunk();
      chk->emit([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) {
        fn(ctx.blit_encoder.ptr(), ctx);
      });
    }
  };

  /**
  it's just about vertex buffer
  - index buffer and input topology are provided in draw commands
  */
  void UpdateVertexBuffer() {
    if (!state_.InputAssembler.InputLayout)
      return;

    uint32_t slot_mask = state_.InputAssembler.InputLayout->GetInputSlotMask();
    if (slot_mask == 0) // effectively empty input layout
      return;

    auto &VertexBuffers = state_.InputAssembler.VertexBuffers;
    if (!VertexBuffers.any_dirty_masked((uint64_t)slot_mask))
      return;

    struct VERTEX_BUFFER_ENTRY {
      uint64_t buffer_handle;
      uint32_t stride;
      uint32_t length;
    };

    uint32_t VERTEX_BUFFER_SLOTS = 32 - __builtin_clz(slot_mask);

    CommandChunk *chk = cmd_queue.CurrentChunk();
    auto currentChunkId = cmd_queue.CurrentSeqId();
    auto [heap, offset] = chk->allocate_gpu_heap(16 * VERTEX_BUFFER_SLOTS, 16);
    VERTEX_BUFFER_ENTRY *entries =
        (VERTEX_BUFFER_ENTRY *)(((char *)heap->contents()) + offset);
    for (unsigned index = 0; index < VERTEX_BUFFER_SLOTS; index++) {
      if (!VertexBuffers.test_bound(index)) {
        entries[index].buffer_handle = 0;
        entries[index].stride = 0;
        entries[index].length = 0;
        continue;
      }
      auto &state = VertexBuffers[index];
      MTL_BINDABLE_RESIDENCY_MASK newResidencyMask = MTL_RESIDENCY_NULL;
      SIMPLE_RESIDENCY_TRACKER *pTracker;
      auto handle = state.Buffer->GetArgumentData(&pTracker);
      entries[index].buffer_handle = handle.buffer() + state.Offset;
      entries[index].stride = state.Stride;
      entries[index].length =
          handle.width() > state.Offset ? handle.width() - state.Offset : 0;
      pTracker->CheckResidency(
          currentChunkId,
          cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady
              ? GetResidencyMask<true>(ShaderType::Vertex, true, false)
              : GetResidencyMask<false>(ShaderType::Vertex, true, false),
          &newResidencyMask);
      if (newResidencyMask) {
        chk->emit([res = state.Buffer->UseBindable(currentChunkId),
                   newResidencyMask](CommandChunk::context &ctx) {
          ctx.render_encoder->useResource(
              res.buffer(), GetUsageFromResidencyMask(newResidencyMask),
              GetStagesFromResidencyMask(newResidencyMask));
        });
      }
    };
    if (cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady) {
      chk->emit([offset](CommandChunk::context &ctx) {
        ctx.render_encoder->setObjectBufferOffset(offset, 16);
      });
    }
    if (cmdbuf_state == CommandBufferState::RenderPipelineReady) {
      chk->emit([offset](CommandChunk::context &ctx) {
        ctx.render_encoder->setVertexBufferOffset(offset, 16);
      });
    }
  }

  void UpdateSOTargets() {
    if (state_.ShaderStages[(UINT)ShaderType::Pixel].Shader) {
      return;
    }
    /**
    FIXME: support all slots
     */
    if (!state_.StreamOutput.Targets.test_dirty(0)) {
      return;
    }
    if (state_.StreamOutput.Targets.test_bound(0)) {
      auto &so_slot0 = state_.StreamOutput.Targets[0];
      if (so_slot0.Offset == 0xFFFFFFFF) {
        ERR("UpdateSOTargets: appending is not supported, expect problem");
        EmitRenderCommand(
            [buffer = so_slot0.Buffer->UseBindable(cmd_queue.CurrentSeqId())](
                MTL::RenderCommandEncoder *encoder) {
              encoder->setVertexBuffer(buffer.buffer(), 0, 20);
            });
      } else {
        EmitRenderCommand(
            [buffer = so_slot0.Buffer->UseBindable(cmd_queue.CurrentSeqId()),
             offset = so_slot0.Offset](MTL::RenderCommandEncoder *encoder) {
              encoder->setVertexBuffer(buffer.buffer(), offset, 20);
            });
      }
    } else {
      EmitRenderCommand([](MTL::RenderCommandEncoder *encoder) {
        encoder->setVertexBuffer(nullptr, 0, 20);
      });
    }
    state_.StreamOutput.Targets.clear_dirty(0);
    if (state_.StreamOutput.Targets.any_dirty()) {
      ERR("UpdateSOTargets: non-zero slot is marked dirty but not bound, "
          "expect problem");
    }
  }

  void Commit() {
    promote_flush = false;
    D3D11_ASSERT(cmdbuf_state == CommandBufferState::Idle);
    /** FIXME: it might be unnecessary? */
    CommandChunk *chk = cmd_queue.CurrentChunk();
    chk->emit([device = Com<IMTLD3D11Device, false>(device)](
                  CommandChunk::context &ctx) {});
    cmd_queue.CommitCurrentChunk(occlusion_query_seq_chunk_start,
                                 ++occlusion_query_seq);
    occlusion_query_seq_chunk_start = occlusion_query_seq;
  };

private:
  uint64_t occlusion_query_seq = 0;
  uint64_t occlusion_query_seq_chunk_start = 0;

public:
  uint64_t NextOcclusionQuerySeq() {
    if (cmdbuf_state == CommandBufferState::RenderEncoderActive ||
        cmdbuf_state == CommandBufferState::RenderPipelineReady ||
        cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady) {
      uint64_t bump_offset = (++occlusion_query_seq) % kOcclusionSampleCount;
      CommandChunk *chk = cmd_queue.CurrentChunk();
      chk->emit([bump_offset](CommandChunk::context &ctx) {
        D3D11_ASSERT(ctx.render_encoder);
        ctx.render_encoder->setVisibilityResultMode(
            MTL::VisibilityResultModeCounting, bump_offset << 3);
      });
    }
    return occlusion_query_seq;
  };

#pragma endregion

#pragma region Default State

  IMTLD3D11RasterizerState* default_rasterizer_state;
  IMTLD3D11DepthStencilState* default_depth_stencil_state;
  IMTLD3D11BlendState* default_blend_state;

#pragma endregion

private:
  IMTLD3D11Device *device;
  D3D11ContextState &state_;
  CommandBufferState cmdbuf_state;
  CommandQueue &cmd_queue;

public:
  enum class DirtyState {
    DepthStencilState,
    RasterizerState,
    BlendFactorAndStencilRef,
    IndexBuffer,
    Viewport,
    Scissors,
  };

  Flags<DirtyState> dirty_state;
};

} // namespace dxmt