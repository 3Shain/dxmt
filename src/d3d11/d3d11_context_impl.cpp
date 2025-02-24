
/**
This file is included by d3d11_context_{imm|def}.cpp
and should be not used as a compilation unit

since it is for internal use only
(and I don't want to deal with several thousands line of code)
*/
#include "d3d11_annotation.hpp"
#include "d3d11_context.hpp"
#include "d3d11_device_child.hpp"
#include "d3d11_enumerable.hpp"
#include "d3d11_interfaces.hpp"
#include "d3d11_private.h"
#include "d3d11_context_state.hpp"
#include "d3d11_device.hpp"
#include "d3d11_pipeline.hpp"
#include "d3d11_query.hpp"
#include "dxmt_buffer.hpp"
#include "dxmt_context.hpp"
#include "dxmt_format.hpp"
#include "dxmt_staging.hpp"
#include "mtld11_resource.hpp"
#include "util_flags.hpp"
#include "util_math.hpp"

namespace dxmt {

template<typename Object> Rc<Object> forward_rc(Rc<Object>& obj);

inline bool
to_metal_primitive_type(D3D11_PRIMITIVE_TOPOLOGY topo, MTL::PrimitiveType& primitive, uint32_t& control_point_num) {
  control_point_num = 0;
  switch (topo) {
  case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
    primitive = MTL::PrimitiveTypePoint;
    break;
  case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
    primitive = MTL::PrimitiveTypeLine;
    break;
  case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
    primitive = MTL::PrimitiveTypeLineStrip;
    break;
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
    primitive = MTL::PrimitiveTypeTriangle;
    break;
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
    primitive = MTL::PrimitiveTypeTriangleStrip;
    break;
  case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
    WARN("adjacency topology is not implemented.");
    return false;
  case D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST:
    primitive = MTL::PrimitiveTypePoint;
    control_point_num = topo - 32;
    break;
  default:
    return false;
  }
  return true;
}

inline MTL::PrimitiveTopologyClass
to_metal_primitive_topology(D3D11_PRIMITIVE_TOPOLOGY topo) {
  switch (topo) {
  case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
    return MTL::PrimitiveTopologyClassPoint;
  case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
  case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
  case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
    return MTL::PrimitiveTopologyClassLine;
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
    return MTL::PrimitiveTopologyClassTriangle;
  case D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST:
  case D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST:
    // Metal tessellation only support triangle as output primitive
    return MTL::PrimitiveTopologyClassTriangle;
  case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED:
    D3D11_ASSERT(0 && "Invalid topology");
  }
  DXMT_UNREACHABLE
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
  MTL_DXGI_FORMAT_DESC FormatDescription;
  D3D11_RESOURCE_DIMENSION Dimension;
  union {
    D3D11_TEXTURE1D_DESC Texture1DDesc;
    D3D11_TEXTURE2D_DESC1 Texture2DDesc;
    D3D11_TEXTURE3D_DESC1 Texture3DDesc;
    D3D11_BUFFER_DESC BufferDesc;
  };

  BlitObject(MTLD3D11Device *pDevice, ID3D11Resource *pResource) : pResource(pResource) {
    pResource->GetType(&Dimension);
    switch (Dimension) {
    case D3D11_RESOURCE_DIMENSION_UNKNOWN:
      break;
    case D3D11_RESOURCE_DIMENSION_BUFFER:
      reinterpret_cast<ID3D11Buffer *>(pResource)->GetDesc(&BufferDesc);
      break;
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
      reinterpret_cast<ID3D11Texture1D *>(pResource)->GetDesc(&Texture1DDesc);
      MTLQueryDXGIFormat(pDevice->GetMTLDevice(), Texture1DDesc.Format, FormatDescription);
      break;
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
      reinterpret_cast<ID3D11Texture2D1 *>(pResource)->GetDesc1(&Texture2DDesc);
      MTLQueryDXGIFormat(pDevice->GetMTLDevice(), Texture2DDesc.Format, FormatDescription);
      break;
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
      reinterpret_cast<ID3D11Texture3D1 *>(pResource)->GetDesc1(&Texture3DDesc);
      MTLQueryDXGIFormat(pDevice->GetMTLDevice(), Texture3DDesc.Format, FormatDescription);
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

  MTL_DXGI_FORMAT_DESC SrcFormat;
  MTL_DXGI_FORMAT_DESC DstFormat;

  bool Invalid = true;

  TextureCopyCommand(
      BlitObject &Dst_, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, BlitObject &Src_, UINT SrcSubresource,
      const D3D11_BOX *pSrcBox
  ) :
      pSrc(Src_.pResource),
      pDst(Dst_.pResource),
      SrcSubresource(SrcSubresource),
      DstSubresource(DstSubresource),
      SrcFormat(Src_.FormatDescription),
      DstFormat(Dst_.FormatDescription) {
    if (Dst_.Dimension != Src_.Dimension)
      return;

    if (SrcFormat.PixelFormat == MTL::PixelFormatInvalid)
      return;
    if (DstFormat.PixelFormat == MTL::PixelFormatInvalid)
      return;

    switch (Dst_.Dimension) {
    default: {
      return;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
      D3D11_TEXTURE1D_DESC &dst_desc = Dst_.Texture1DDesc;
      D3D11_TEXTURE1D_DESC &src_desc = Src_.Texture1DDesc;

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
      D3D11_TEXTURE2D_DESC1 &dst_desc = Dst_.Texture2DDesc;
      D3D11_TEXTURE2D_DESC1 &src_desc = Src_.Texture2DDesc;

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
      D3D11_TEXTURE3D_DESC1 &dst_desc = Dst_.Texture3DDesc;
      D3D11_TEXTURE3D_DESC1 &src_desc = Src_.Texture3DDesc;

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

    if ((DstFormat.Flag & MTL_DXGI_FORMAT_BC) == (SrcFormat.Flag & MTL_DXGI_FORMAT_BC)) {
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
    SrcSize = {SrcBox.right - SrcBox.left, SrcBox.bottom - SrcBox.top, SrcBox.back - SrcBox.front};

    Invalid = false;
  }
};

class TextureUpdateCommand {
public:
  ID3D11Resource *pDst;
  UINT DstSubresource;
  Subresource Dst;
  MTL::Region DstRegion;
  uint32_t EffectiveBytesPerRow;
  uint32_t EffectiveRows;

  MTL_DXGI_FORMAT_DESC DstFormat;

  bool Invalid = true;

  TextureUpdateCommand(BlitObject &Dst_, UINT DstSubresource, const D3D11_BOX *pDstBox) :
      pDst(Dst_.pResource),
      DstSubresource(DstSubresource),
      DstFormat(Dst_.FormatDescription) {

    if (DstFormat.PixelFormat == MTL::PixelFormatInvalid)
      return;

    switch (Dst_.Dimension) {
    default: {
      return;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
      D3D11_TEXTURE1D_DESC &dst_desc = Dst_.Texture1DDesc;

      if (DstSubresource >= dst_desc.MipLevels * dst_desc.ArraySize)
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

      if (DstSubresource >= dst_desc.MipLevels * dst_desc.ArraySize)
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

      if (DstSubresource >= dst_desc.MipLevels)
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

    DstRegion = {
        DstBox.left,
        DstBox.top,
        DstBox.front,
        DstBox.right - DstBox.left,
        DstBox.bottom - DstBox.top,
        DstBox.back - DstBox.front
    };

    if (DstFormat.Flag & MTL_DXGI_FORMAT_BC) {
      EffectiveBytesPerRow = (align(DstRegion.size.width, 4u) >> 2) * DstFormat.BytesPerTexel;
      EffectiveRows = align(DstRegion.size.height, 4u) >> 2;
    } else {
      EffectiveBytesPerRow = DstRegion.size.width * DstFormat.BytesPerTexel;
      EffectiveRows = DstRegion.size.height;
    }

    Invalid = false;
  }
};

struct DXMT_DRAW_ARGUMENTS {
  uint32_t IndexCount;
  uint32_t StartIndex;
  uint32_t InstanceCount;
  uint32_t StartInstance;
  uint32_t BaseVertex;
};

template <typename ContextInternalState>
class MTLD3D11ContextExt;

template <typename ContextInternalState> class MTLD3D11DeviceContextImplBase : public MTLD3D11DeviceContextBase {
  template<typename ContextInternalState_>
  friend class MTLD3D11ContextExt;
public:
  HRESULT
  QueryInterface(REFIID riid, void **ppvObject) override {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) || riid == __uuidof(ID3D11DeviceContext) ||
        riid == __uuidof(ID3D11DeviceContext1) || riid == __uuidof(ID3D11DeviceContext2) ||
        riid == __uuidof(ID3D11DeviceContext3) || riid == __uuidof(ID3D11DeviceContext4) ||
        riid == __uuidof(IMTLD3D11DeviceContext)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (riid == __uuidof(IMTLD3D11ContextExt)) {
      *ppvObject = ref(&ext_);
      return S_OK;
    }

    if (riid == __uuidof(ID3DUserDefinedAnnotation)) {
      *ppvObject = ref(&annotation_);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11DeviceContext4), riid)) {
      WARN("D3D11DeviceContext: Unknown interface query ", str::format(riid));
    }
    return E_NOINTERFACE;
  }

  void
  SetResourceMinLOD(ID3D11Resource *pResource, FLOAT MinLOD) override {
    if (auto expected = com_cast<IMTLMinLODClampable>(pResource)) {
      expected->SetMinLOD(MinLOD);
    }
  }

  FLOAT
  GetResourceMinLOD(ID3D11Resource *pResource) override {
    if (auto expected = com_cast<IMTLMinLODClampable>(pResource)) {
      return expected->GetMinLOD();
    }
    return 0.0f;
  }

#pragma region Resource Manipulation

  void
  ClearRenderTargetView(ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4]) override {
    ClearRenderTargetView(static_cast<IMTLD3D11RenderTargetView *>(pRenderTargetView), ColorRGBA);
  }

  void
  ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView *pUnorderedAccessView, const UINT Values[4]) override {
    ClearUnorderedAccessViewUint(static_cast<D3D11UnorderedAccessView *>(pUnorderedAccessView), Values);
  }

  void
  ClearUnorderedAccessViewUint(D3D11UnorderedAccessView *pUAV, const UINT Values[4]) {
    InvalidateCurrentPass(true);
    SwitchToComputeEncoder();
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
    pUAV->GetDesc(&desc);
    if (desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
      EmitOP([=, buffer = pUAV->buffer(), viewId = pUAV->viewId(), slice = pUAV->bufferSlice(),
            value = std::array<uint32_t, 4>({Values[0], Values[1], Values[2], Values[3]}),
            is_raw = desc.Buffer.Flags & D3D11_BUFFER_UAV_FLAG_RAW,
            format = desc.Format](ArgumentEncodingContext &enc) {
        if (is_raw) {
          enc.encodeComputeCommand([&, buffer = enc.access(
                                           buffer, slice.byteOffset, slice.byteLength, DXMT_ENCODER_RESOURCE_ACESS_WRITE
                                       )](ComputeCommandContext &ctx) {
            ctx.cmd.ClearBufferUint(ctx.encoder, buffer, slice.byteOffset, slice.byteLength >> 2, value);
          });
        } else {
          if (format == DXGI_FORMAT_UNKNOWN) {
            enc.encodeComputeCommand(
                [&, buffer = enc.access(buffer, slice.byteOffset, slice.byteLength, DXMT_ENCODER_RESOURCE_ACESS_WRITE)](
                    ComputeCommandContext &ctx
                ) { ctx.cmd.ClearBufferUint(ctx.encoder, buffer, slice.byteOffset, slice.byteLength >> 2, value); }
            );
          } else {
            enc.encodeComputeCommand([&, texture = enc.access(buffer, viewId, DXMT_ENCODER_RESOURCE_ACESS_WRITE)](
                                         ComputeCommandContext &ctx
                                     ) { ctx.cmd.ClearTextureBufferUint(ctx.encoder, texture, value); });
          }
        }
      });
    else
      EmitOP([=, texture = pUAV->texture(), viewId = pUAV->viewId(),
            value = std::array<uint32_t, 4>({Values[0], Values[1], Values[2], Values[3]}),
            dimension = desc.ViewDimension](ArgumentEncodingContext &enc) mutable {
        auto view_format = texture->view(viewId)->pixelFormat();
        auto uint_format = MTLGetUnsignedIntegerFormat(view_format);
        /* RG11B10Float is a special case */
        if (view_format == MTL::PixelFormatRG11B10Float) {
          uint_format = MTL::PixelFormatR32Uint;
          value[0] = ((value[0] & 0x7FF) << 0) | ((value[1] & 0x7FF) << 11) | ((value[2] & 0x3FF) << 22);
        }

        if (uint_format == MTL::PixelFormatInvalid) {
          WARN("ClearUnorderedAccessViewUint: unhandled format ", view_format);
          return;
        }
        auto viewChecked = texture->checkViewUseFormat(viewId, uint_format);
        enc.encodeComputeCommand(
            [&, dimension, texture = enc.access(texture, viewChecked, DXMT_ENCODER_RESOURCE_ACESS_WRITE)](auto &ctx) {
              switch (dimension) {
              default:
                break;
              case D3D11_UAV_DIMENSION_TEXTURE1D:
              case D3D11_UAV_DIMENSION_TEXTURE2D:
                ctx.cmd.ClearTexture2DUint(ctx.encoder, texture, value);
                break;
              case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
              case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
                ctx.cmd.ClearTexture2DArrayUint(ctx.encoder, texture, value);
                break;
              case D3D11_UAV_DIMENSION_TEXTURE3D:
                ctx.cmd.ClearTexture3DUint(ctx.encoder, texture, value);
                break;
              }
            }
        );
      });
    InvalidateCurrentPass();
  }

  void
  ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView *pUnorderedAccessView, const FLOAT Values[4]) override {
    ClearUnorderedAccessViewFloat(static_cast<D3D11UnorderedAccessView *>(pUnorderedAccessView), Values);
  }

  void
  ClearUnorderedAccessViewFloat(D3D11UnorderedAccessView *pUAV, const FLOAT Values[4])  {
    InvalidateCurrentPass(true);
    SwitchToComputeEncoder();
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
    pUAV->GetDesc(&desc);
    if (desc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
      EmitOP([=, buffer = pUAV->buffer(), viewId = pUAV->viewId(), slice = pUAV->bufferSlice(),
            value = std::array<float, 4>({Values[0], Values[1], Values[2], Values[3]}),
            is_raw = desc.Buffer.Flags & D3D11_BUFFER_UAV_FLAG_RAW,
            format = desc.Format](ArgumentEncodingContext &enc) {
        if (is_raw) {
          enc.encodeComputeCommand([&, buffer = enc.access(
                                           buffer, slice.byteOffset, slice.byteLength, DXMT_ENCODER_RESOURCE_ACESS_WRITE
                                       )](ComputeCommandContext &ctx) {
            ctx.cmd.ClearBufferFloat(ctx.encoder, buffer, slice.byteOffset, slice.byteLength >> 2, value);
          });
        } else {
          if (format == DXGI_FORMAT_UNKNOWN) {
            enc.encodeComputeCommand(
                [&, buffer = enc.access(buffer, slice.byteOffset, slice.byteLength, DXMT_ENCODER_RESOURCE_ACESS_WRITE)](
                    ComputeCommandContext &ctx
                ) { ctx.cmd.ClearBufferFloat(ctx.encoder, buffer, slice.byteOffset, slice.byteLength >> 2, value); }
            );
          } else {
            enc.encodeComputeCommand([&, texture = enc.access(buffer, viewId, DXMT_ENCODER_RESOURCE_ACESS_WRITE)](
                                         ComputeCommandContext &ctx
                                     ) { ctx.cmd.ClearTextureBufferFloat(ctx.encoder, texture, value); });
          }
        }
      });
    else
      EmitOP([=, texture = pUAV->texture(), viewId = pUAV->viewId(), dimension = desc.ViewDimension,
            value = std::array<float, 4>({Values[0], Values[1], Values[2], Values[3]})](ArgumentEncodingContext &enc) {
        switch (dimension) {
        default:
          break;
        case D3D11_UAV_DIMENSION_TEXTURE1D:
          UNIMPLEMENTED("tex1d clear");
          break;
        case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
          UNIMPLEMENTED("tex1darr clear");
          break;
        case D3D11_UAV_DIMENSION_TEXTURE2D:
          enc.encodeComputeCommand([&, texture =
                                           enc.access(texture, viewId, DXMT_ENCODER_RESOURCE_ACESS_WRITE)](auto &ctx) {
            ctx.cmd.ClearTexture2DFloat(ctx.encoder, texture, value);
          });
          break;
        case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
          enc.encodeComputeCommand([&, texture =
                                           enc.access(texture, viewId, DXMT_ENCODER_RESOURCE_ACESS_WRITE)](auto &ctx) {
            ctx.cmd.ClearTexture2DArrayFloat(ctx.encoder, texture, value);
          });
          break;
        case D3D11_UAV_DIMENSION_TEXTURE3D:
          enc.encodeComputeCommand([&, texture =
                                           enc.access(texture, viewId, DXMT_ENCODER_RESOURCE_ACESS_WRITE)](auto &ctx) {
            ctx.cmd.ClearTexture3DFloat(ctx.encoder, texture, value);
          });
          break;
        }
      });
    InvalidateCurrentPass();
  }

  void
  ClearDepthStencilView(ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
      override {
    ClearDepthStencilView(static_cast<IMTLD3D11DepthStencilView*>(pDepthStencilView), ClearFlags, Depth, Stencil);
  }

  void
  ClearView(ID3D11View *pView, const FLOAT Color[4], const D3D11_RECT *pRect, UINT NumRects) override {
    if (NumRects && !pRect)
      return;

    while (auto expected = com_cast<ID3D11RenderTargetView>(pView)) {
      auto rtv = static_cast<IMTLD3D11RenderTargetView *>(expected.ptr());
      if (NumRects > 1)
        break;
      if (NumRects) {
        if (pRect[0].top != 0 || pRect[0].left != 0) {
          break;
        }
        uint32_t rect_width = pRect[0].right - pRect[0].left;
        uint32_t rect_height = pRect[0].bottom - pRect[0].top;
        if (rect_width != rtv->GetAttachmentDesc().Width)
          break;
        if (rect_height != rtv->GetAttachmentDesc().Height)
          break;
      }
      return ClearRenderTargetView(rtv, Color);
    }

    IMPLEMENT_ME
  }

  void
  GenerateMips(ID3D11ShaderResourceView *pShaderResourceView) override {
    if (auto srv = static_cast<D3D11ShaderResourceView *>(pShaderResourceView)) {
      D3D11_SHADER_RESOURCE_VIEW_DESC desc;
      pShaderResourceView->GetDesc(&desc);
      if (desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER || desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX) {
        return;
      }
      SwitchToBlitEncoder(CommandBufferState::BlitEncoderActive);
      EmitOP([tex = srv->texture(), viewId = srv->viewId()](ArgumentEncodingContext &enc) {
        auto texture = enc.access(tex, viewId, DXMT_ENCODER_RESOURCE_ACESS_READ | DXMT_ENCODER_RESOURCE_ACESS_WRITE);
        if (texture->mipmapLevelCount() > 1) {
          enc.encodeBlitCommand([texture](BlitCommandContext &ctx) { ctx.encoder->generateMipmaps(texture); });
        }
      });
    }
  }

  void
  ResolveSubresource(
      ID3D11Resource *pDstResource, UINT DstSubresource, ID3D11Resource *pSrcResource, UINT SrcSubresource,
      DXGI_FORMAT Format
  ) override {
    D3D11_RESOURCE_DIMENSION dimension;
    pDstResource->GetType(&dimension);
    if (dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
      return;
    pSrcResource->GetType(&dimension);
    if (dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
      return;
    D3D11_TEXTURE2D_DESC desc;
    uint32_t width, height;
    ((ID3D11Texture2D *)pSrcResource)->GetDesc(&desc);
    if (desc.ArraySize <= DstSubresource) {
      return;
    }
    width = desc.Width;
    height = desc.Height;
    ((ID3D11Texture2D *)pDstResource)->GetDesc(&desc);
    if (desc.SampleDesc.Count > 1) {
      ERR("ResolveSubresource: Destination is not valid resolve target");
      return;
    }
    uint32_t dst_level = DstSubresource % desc.MipLevels;
    uint32_t dst_slice = DstSubresource / desc.MipLevels;
    if (width != std::max(1u, desc.Width >> dst_level) || height != std::max(1u, desc.Height >> dst_level)) {
      ERR("ResolveSubresource: Size doesn't match");
      return;
    }
    ResolveSubresource(
        static_cast<D3D11ResourceCommon *>(pSrcResource), SrcSubresource,
        static_cast<D3D11ResourceCommon *>(pDstResource), dst_level, dst_slice
    );
  }

  void
  CopyResource(ID3D11Resource *pDstResource, ID3D11Resource *pSrcResource) override {
    BlitObject Dst(device, pDstResource);
    BlitObject Src(device, pSrcResource);

    switch (Dst.Dimension) {
    case D3D11_RESOURCE_DIMENSION_UNKNOWN:
      break;
    case D3D11_RESOURCE_DIMENSION_BUFFER: {
      if (Src.Dimension != D3D11_RESOURCE_DIMENSION_BUFFER)
        return;
      CopyBuffer((ID3D11Buffer *)pDstResource, 0, 0, 0, 0, (ID3D11Buffer *)pSrcResource, 0, nullptr);
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
      D3D11_TEXTURE1D_DESC desc;
      ((ID3D11Texture1D *)pSrcResource)->GetDesc(&desc);
      for (auto sub : EnumerateSubresources(desc)) {
        CopyTexture(TextureCopyCommand(Dst, sub.SubresourceId, 0, 0, 0, Src, sub.SubresourceId, nullptr));
      }
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
      D3D11_TEXTURE2D_DESC1 desc;
      ((ID3D11Texture2D1 *)pSrcResource)->GetDesc1(&desc);
      for (auto sub : EnumerateSubresources(desc)) {
        CopyTexture(TextureCopyCommand(Dst, sub.SubresourceId, 0, 0, 0, Src, sub.SubresourceId, nullptr));
      }
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
      D3D11_TEXTURE3D_DESC1 desc;
      ((ID3D11Texture3D1 *)pSrcResource)->GetDesc1(&desc);
      for (auto sub : EnumerateSubresources(desc)) {
        CopyTexture(TextureCopyCommand(Dst, sub.SubresourceId, 0, 0, 0, Src, sub.SubresourceId, nullptr));
      }
      break;
    }
    }
  }

  void
  CopySubresourceRegion(
      ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *pSrcResource,
      UINT SrcSubresource, const D3D11_BOX *pSrcBox
  ) override {
    CopySubresourceRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox, 0);
  }

  void
  CopySubresourceRegion1(
      ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource *pSrcResource,
      UINT SrcSubresource, const D3D11_BOX *pSrcBox, UINT CopyFlags
  ) override {
    if (!pDstResource)
      return;
    if (!pSrcResource)
      return;

    BlitObject Dst(device, pDstResource);
    BlitObject Src(device, pSrcResource);

    switch (Dst.Dimension) {
    case D3D11_RESOURCE_DIMENSION_UNKNOWN:
      break;
    case D3D11_RESOURCE_DIMENSION_BUFFER: {
      if (Src.Dimension != D3D11_RESOURCE_DIMENSION_BUFFER)
        return;
      CopyBuffer(
          (ID3D11Buffer *)pDstResource, DstSubresource, DstX, DstY, DstZ, (ID3D11Buffer *)pSrcResource, SrcSubresource,
          pSrcBox
      );
      break;
    }
    default:
      CopyTexture(TextureCopyCommand(Dst, DstSubresource, DstX, DstY, DstZ, Src, SrcSubresource, pSrcBox));
      break;
    }
  }

  void
  CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView *pSrcView)
      override {
    if (auto dst_bind = reinterpret_cast<D3D11ResourceCommon *>(pDstBuffer)) {
      if (auto uav = static_cast<D3D11UnorderedAccessView *>(pSrcView)) {
        SwitchToBlitEncoder(CommandBufferState::BlitEncoderActive);
        EmitOP([=, dst = dst_bind->buffer(), counter = uav->counter()](ArgumentEncodingContext &enc) {
          auto dst_buffer = enc.access(dst, DstAlignedByteOffset, 4, DXMT_ENCODER_RESOURCE_ACESS_WRITE);
          auto counter_buffer = enc.access(counter, 0, 4, DXMT_ENCODER_RESOURCE_ACESS_READ);
          enc.encodeBlitCommand([=](BlitCommandContext &ctx) {
            ctx.encoder->copyFromBuffer(counter_buffer, 0, dst_buffer, DstAlignedByteOffset, 4);
          });
        });
      }
    }
  }

  void
  UpdateSubresource(
      ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData,
      UINT SrcRowPitch, UINT SrcDepthPitch
  ) override {
    UpdateSubresource1(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch, 0);
  }

  void
  UpdateSubresource1(
      ID3D11Resource *pDstResource, UINT DstSubresource, const D3D11_BOX *pDstBox, const void *pSrcData,
      UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags
  ) override {
    if (!pDstResource)
      return;

    BlitObject Dst(device, pDstResource);

    if (Dst.Dimension == D3D11_RESOURCE_DIMENSION_BUFFER) {
      D3D11_BUFFER_DESC desc;
      ((ID3D11Buffer *)pDstResource)->GetDesc(&desc);
      uint32_t copy_offset = 0;
      uint32_t copy_len = desc.ByteWidth;
      if (pDstBox) {
        if (pDstBox->right <= pDstBox->left) {
          return;
        }
        copy_offset = pDstBox->left;
        copy_len = pDstBox->right - copy_offset;
      }
      UINT buffer_len = 0;
      UINT unused_bind_flag = 0;
      if (auto dynamic = GetDynamicBuffer(pDstResource, &buffer_len, &unused_bind_flag)) {
        D3D11_MAPPED_SUBRESOURCE mapped;
        if ((copy_len == buffer_len && copy_offset == 0) || (CopyFlags & D3D11_COPY_DISCARD)) {
          Map(pDstResource, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
          std::memcpy(reinterpret_cast<char *>(mapped.pData) + copy_offset, pSrcData, copy_len);
          Unmap(pDstResource, 0);
          return;
        }
        if (CopyFlags & D3D11_COPY_NO_OVERWRITE) {
          Map(pDstResource, 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped);
          std::memcpy(reinterpret_cast<char *>(mapped.pData) + copy_offset, pSrcData, copy_len);
          Unmap(pDstResource, 0);
          return;
        }
      }
      if (auto bindable = reinterpret_cast<D3D11ResourceCommon *>(pDstResource)) {
        // if (auto _ = UseImmediate(bindable.ptr())) {
        //   auto buffer = _.buffer();
        //   memcpy(((char *)buffer->contents()) + copy_offset, pSrcData, copy_len);
        //   buffer->didModifyRange(NS::Range::Make(copy_offset, copy_len));
        //   return;
        // }
        auto [ptr, staging_buffer, offset] = AllocateStagingBuffer(copy_len, 16);
        memcpy(ptr, pSrcData, copy_len);
        SwitchToBlitEncoder(CommandBufferState::UpdateBlitEncoderActive);
        EmitOP([staging_buffer, offset, dst = bindable->buffer(), copy_offset, copy_len](ArgumentEncodingContext &enc) {
          auto dst_buffer = enc.access(dst, copy_offset, copy_len, DXMT_ENCODER_RESOURCE_ACESS_WRITE);
          enc.encodeBlitCommand([&, dst_buffer](BlitCommandContext &ctx) {
            ctx.encoder->copyFromBuffer(staging_buffer, offset, dst_buffer, copy_offset, copy_len);
          });
        });
      } else {
        UNIMPLEMENTED("UpdateSubresource1: TODO: staging?");
      }
      return;
    }

    UpdateTexture(TextureUpdateCommand(Dst, DstSubresource, pDstBox), pSrcData, SrcRowPitch, SrcDepthPitch, CopyFlags);
  }

  void
  DiscardResource(ID3D11Resource *pResource) override {
    /*
    All the Discard* API is not implemented and that's probably fine (as it's
    more like a hint of optimization, and Metal manages resources on its own)
    FIXME: for render targets we can use this information: LoadActionDontCare
    FIXME: A Map with D3D11_MAP_WRITE type could become D3D11_MAP_WRITE_DISCARD?
    */
    ERR_ONCE("Not implemented");
  }

  void
  DiscardView(ID3D11View *pResourceView) override {
    DiscardView1(pResourceView, 0, 0);
  }

  void
  DiscardView1(ID3D11View *pResourceView, const D3D11_RECT *pRects, UINT NumRects) override {
    ERR_ONCE("Not implemented");
  }
#pragma endregion

#pragma region DrawCall

  void
  Draw(UINT VertexCount, UINT StartVertexLocation) override {
    MTL::PrimitiveType Primitive;
    uint32_t ControlPointCount;
    if(!to_metal_primitive_type(state_.InputAssembler.Topology, Primitive, ControlPointCount))
      return;
    if (!PreDraw<false>())
      return;
    if (ControlPointCount) {
      return TessellationDraw(ControlPointCount, VertexCount, 1, StartVertexLocation, 0);
    }
    EmitOP([Primitive, StartVertexLocation, VertexCount](ArgumentEncodingContext& enc) {
      enc.bumpVisibilityResultOffset();
      enc.encodeRenderCommand([&](RenderCommandContext& ctx) {
        ctx.encoder->drawPrimitives(Primitive, StartVertexLocation, VertexCount);
      });
    });
  }

  void
  DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) override {
    MTL::PrimitiveType Primitive;
    uint32_t ControlPointCount;
    if(!to_metal_primitive_type(state_.InputAssembler.Topology, Primitive, ControlPointCount))
      return;
    if (!PreDraw<true>())
      return;
    if (ControlPointCount) {
      return TessellationDrawIndexed(ControlPointCount, IndexCount, StartIndexLocation, BaseVertexLocation, 1, 0);
    };
    auto IndexType =
        state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT ? MTL::IndexTypeUInt32 : MTL::IndexTypeUInt16;
    auto IndexBufferOffset =
        state_.InputAssembler.IndexBufferOffset +
        StartIndexLocation * (state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT ? 4 : 2);
    EmitOP([IndexType, IndexBufferOffset, Primitive, IndexCount, BaseVertexLocation](ArgumentEncodingContext &enc) {
      enc.bumpVisibilityResultOffset();
      enc.encodeRenderCommand([&, index_buffer = Obj(enc.currentIndexBuffer())](RenderCommandContext &ctx) {
        assert(index_buffer);
        ctx.encoder->drawIndexedPrimitives(
            Primitive, IndexCount, IndexType, index_buffer, IndexBufferOffset, 1, BaseVertexLocation, 0
        );
      });
    });
  }

  void
  DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
      override {
    MTL::PrimitiveType Primitive;
    uint32_t ControlPointCount;
    if (!to_metal_primitive_type(state_.InputAssembler.Topology, Primitive, ControlPointCount))
      return;
    if (!PreDraw<false>())
      return;
    if (ControlPointCount) {
      return TessellationDraw(
          ControlPointCount, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation
      );
    }
    EmitOP([Primitive, StartVertexLocation, VertexCountPerInstance, InstanceCount,
          StartInstanceLocation](ArgumentEncodingContext &enc) {
      enc.bumpVisibilityResultOffset();
      enc.encodeRenderCommand([&](RenderCommandContext &ctx) {
        ctx.encoder->drawPrimitives(
            Primitive, StartVertexLocation, VertexCountPerInstance, InstanceCount, StartInstanceLocation
        );
      });
    });
  }

  void
  DrawIndexedInstanced(
      UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation,
      UINT StartInstanceLocation
  ) override {
    MTL::PrimitiveType Primitive;
    uint32_t ControlPointCount;
    if(!to_metal_primitive_type(state_.InputAssembler.Topology, Primitive, ControlPointCount))
      return;
    if (!PreDraw<true>())
      return;
    if (ControlPointCount) {
      return TessellationDrawIndexed(
          ControlPointCount, IndexCountPerInstance, StartIndexLocation, BaseVertexLocation, InstanceCount,
          StartInstanceLocation
      );
      return;
    }
    auto IndexType =
        state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT ? MTL::IndexTypeUInt32 : MTL::IndexTypeUInt16;
    auto IndexBufferOffset =
        state_.InputAssembler.IndexBufferOffset +
        StartIndexLocation * (state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT ? 4 : 2);
    EmitOP([IndexType, IndexBufferOffset, Primitive, InstanceCount, BaseVertexLocation, StartInstanceLocation,
          IndexCountPerInstance](ArgumentEncodingContext &enc) {
      enc.bumpVisibilityResultOffset();
      enc.encodeRenderCommand([&, index_buffer = Obj(enc.currentIndexBuffer())](RenderCommandContext &ctx) {
        assert(index_buffer);
        ctx.encoder->drawIndexedPrimitives(
            Primitive, IndexCountPerInstance, IndexType, index_buffer, IndexBufferOffset, InstanceCount,
            BaseVertexLocation, StartInstanceLocation
        );
      });
    });
  }

  void
  TessellationDraw(
      UINT NumControlPoint, UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation,
      UINT StartInstanceLocation
  ) {
    assert(NumControlPoint);

    EmitOP([=](ArgumentEncodingContext &enc) {
      auto offset = enc.allocate_gpu_heap(4 * 5, 4);
      DXMT_DRAW_ARGUMENTS *draw_arugment = enc.get_gpu_heap_pointer<DXMT_DRAW_ARGUMENTS>(offset);
      draw_arugment->BaseVertex = StartVertexLocation;
      draw_arugment->IndexCount = VertexCountPerInstance;
      draw_arugment->StartIndex = 0;
      draw_arugment->InstanceCount = InstanceCount;
      draw_arugment->StartInstance = StartInstanceLocation;
      auto PatchCountPerInstance = VertexCountPerInstance / NumControlPoint;
      auto PatchPerGroup = 32 / enc.tess_threads_per_patch;
      auto ThreadsPerPatch = enc.tess_threads_per_patch;
      auto PatchPerMeshInstance = (PatchCountPerInstance - 1) / PatchPerGroup + 1;
      auto [cp_buffer, cp_offset] = enc.allocateTempBuffer(
          enc.tess_num_output_control_point_element * 16 * VertexCountPerInstance * InstanceCount, 16
      );
      auto [pc_buffer, pc_offset] = enc.allocateTempBuffer(
          enc.tess_num_output_patch_constant_scalar * 4 * PatchCountPerInstance * InstanceCount, 4
      );
      auto [tess_factor_buffer, tess_factor_offset] =
          enc.allocateTempBuffer(6 * 2 * PatchCountPerInstance * InstanceCount, 4);

      enc.encodePreTessCommand([=](RenderCommandContext &ctx) {
        auto &encoder = ctx.encoder;
        encoder->setObjectBufferOffset(offset, 21);
        encoder->setMeshBuffer(cp_buffer, cp_offset, 20);
        encoder->setMeshBuffer(pc_buffer, pc_offset, 21);
        encoder->setMeshBuffer(tess_factor_buffer, tess_factor_offset, 22);
        encoder->drawMeshThreadgroups(
            MTL::Size(PatchPerMeshInstance, InstanceCount, 1), MTL::Size(ThreadsPerPatch, PatchPerGroup, 1),
            MTL::Size(ThreadsPerPatch, PatchPerGroup, 1)
        );
      });
      enc.bumpVisibilityResultOffset();
      enc.encodeRenderCommand([=](RenderCommandContext &ctx) {
        auto &encoder = ctx.encoder;
        encoder->setVertexBuffer(cp_buffer, cp_offset, 20);
        encoder->setVertexBuffer(pc_buffer, pc_offset, 21);
        encoder->setVertexBuffer(tess_factor_buffer, tess_factor_offset, 22);
        encoder->setTessellationFactorBuffer(tess_factor_buffer, tess_factor_offset, 0);
        encoder->drawPatches(
            0, 0,
            PatchCountPerInstance * InstanceCount, //
            nullptr, 0, 1, 0
        );
      });
    });
  }

  void
  TessellationDrawIndexed(
      UINT NumControlPoint, UINT IndexCountPerInstance, UINT StartIndexLocation, INT BaseVertexLocation,
      UINT InstanceCount, UINT BaseInstance
  ) {
    assert(NumControlPoint);
    auto IndexBufferOffset =
        state_.InputAssembler.IndexBufferOffset +
        StartIndexLocation * (state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT ? 4 : 2);

    EmitOP([=](ArgumentEncodingContext &enc) {
      auto offset = enc.allocate_gpu_heap(4 * 5, 4);
      DXMT_DRAW_ARGUMENTS *draw_arugment = enc.get_gpu_heap_pointer<DXMT_DRAW_ARGUMENTS>(offset);
      draw_arugment->BaseVertex = BaseVertexLocation;
      draw_arugment->IndexCount = IndexCountPerInstance;
      draw_arugment->StartIndex = 0; // already provided offset
      draw_arugment->InstanceCount = InstanceCount;
      draw_arugment->StartInstance = BaseInstance;
      auto PatchCountPerInstance = IndexCountPerInstance / NumControlPoint;
      auto PatchPerGroup = 32 / enc.tess_threads_per_patch;
      auto ThreadsPerPatch = enc.tess_threads_per_patch;
      auto PatchPerMeshInstance = (PatchCountPerInstance - 1) / PatchPerGroup + 1;
      auto [cp_buffer, cp_offset] = enc.allocateTempBuffer(
          enc.tess_num_output_control_point_element * 16 * IndexCountPerInstance * InstanceCount, 16
      );
      auto [pc_buffer, pc_offset] = enc.allocateTempBuffer(
          enc.tess_num_output_patch_constant_scalar * 4 * PatchCountPerInstance * InstanceCount, 4
      );
      auto [tess_factor_buffer, tess_factor_offset] =
          enc.allocateTempBuffer(6 * 2 * PatchCountPerInstance * InstanceCount, 4);
      enc.encodePreTessCommand([=, index = Obj(enc.currentIndexBuffer())](RenderCommandContext &ctx) {
        auto &encoder = ctx.encoder;
        encoder->setObjectBuffer(index, IndexBufferOffset, 20);
        encoder->setObjectBufferOffset(offset, 21);
        encoder->setMeshBuffer(cp_buffer, cp_offset, 20);
        encoder->setMeshBuffer(pc_buffer, pc_offset, 21);
        encoder->setMeshBuffer(tess_factor_buffer, tess_factor_offset, 22);
        encoder->drawMeshThreadgroups(
          MTL::Size(PatchPerMeshInstance, InstanceCount, 1),
          MTL::Size(ThreadsPerPatch, PatchPerGroup, 1),
          MTL::Size(ThreadsPerPatch, PatchPerGroup, 1)
        );
      });
      enc.bumpVisibilityResultOffset();
      enc.encodeRenderCommand([=](RenderCommandContext &ctx) {
        auto &encoder = ctx.encoder;
        encoder->setVertexBuffer(cp_buffer, cp_offset, 20);
        encoder->setVertexBuffer(pc_buffer, pc_offset, 21);
        encoder->setVertexBuffer(tess_factor_buffer, tess_factor_offset, 22);
        encoder->setTessellationFactorBuffer(tess_factor_buffer, tess_factor_offset, 0 /* TODO: */);
        encoder->drawPatches(
            0, 0,
            PatchCountPerInstance * InstanceCount, //
            nullptr, 0, 1, 0
        );
      });
    });
  }

  void
  DrawIndexedInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs) override {
    MTL::PrimitiveType Primitive;
    uint32_t ControlPointCount;
    if(!to_metal_primitive_type(state_.InputAssembler.Topology, Primitive, ControlPointCount))
      return;
    if (!PreDraw<true>())
      return;
    if (ControlPointCount) {
      EmitST([](ArgumentEncodingContext &enc) {
        enc.setCompatibilityFlag(FeatureCompatibility::UnsupportedIndirectTessellationDraw);
      });
      return;
    }
    auto IndexType =
        state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT ? MTL::IndexTypeUInt32 : MTL::IndexTypeUInt16;
    auto IndexBufferOffset = state_.InputAssembler.IndexBufferOffset;
    if (auto bindable = reinterpret_cast<D3D11ResourceCommon *>(pBufferForArgs)) {
      EmitOP([IndexType, IndexBufferOffset, Primitive, ArgBuffer = bindable->buffer(),
            AlignedByteOffsetForArgs](ArgumentEncodingContext &enc) {
        auto buffer = enc.access(ArgBuffer, AlignedByteOffsetForArgs, 20, DXMT_ENCODER_RESOURCE_ACESS_READ);
        enc.bumpVisibilityResultOffset();
        enc.encodeRenderCommand([&, buffer, index_buffer = Obj(enc.currentIndexBuffer())](RenderCommandContext &ctx) {
          ctx.encoder->drawIndexedPrimitives(
              Primitive, IndexType, index_buffer, IndexBufferOffset, buffer, AlignedByteOffsetForArgs
          );
        });
      });
    }
  }

  void
  DrawInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs) override {
    MTL::PrimitiveType Primitive;
    uint32_t ControlPointCount;
    if(!to_metal_primitive_type(state_.InputAssembler.Topology, Primitive, ControlPointCount))
      return;
    if (!PreDraw<true>())
      return;
    if (ControlPointCount) {
      EmitST([](ArgumentEncodingContext &enc) {
        enc.setCompatibilityFlag(FeatureCompatibility::UnsupportedIndirectTessellationDraw);
      });
      return;
    }
    if (auto bindable = reinterpret_cast<D3D11ResourceCommon *>(pBufferForArgs)) {
      EmitOP([Primitive, ArgBuffer = bindable->buffer(), AlignedByteOffsetForArgs](ArgumentEncodingContext &enc) {
        auto buffer = enc.access(ArgBuffer, AlignedByteOffsetForArgs, 20, DXMT_ENCODER_RESOURCE_ACESS_READ);
        enc.bumpVisibilityResultOffset();
        enc.encodeRenderCommand([&, buffer](RenderCommandContext &ctx) {
          ctx.encoder->drawPrimitives(Primitive, buffer, AlignedByteOffsetForArgs);
        });
      });
    }
  }

  void
  DrawAuto() override {
    EmitST([](ArgumentEncodingContext &enc) { enc.setCompatibilityFlag(FeatureCompatibility::UnsupportedDrawAuto); });
  }

  void
  Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) override {
    if (!PreDispatch())
      return;
    EmitOP([ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ](ArgumentEncodingContext &enc) {
      enc.encodeComputeCommand([&](ComputeCommandContext &ctx) {
        ctx.encoder->dispatchThreadgroups(
            MTL::Size::Make(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ), ctx.threadgroup_size
        );
      });
    });
  }

  void
  DispatchIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs) override {
    if (!PreDispatch())
      return;
    if (auto bindable = reinterpret_cast<D3D11ResourceCommon *>(pBufferForArgs)) {
      EmitOP([AlignedByteOffsetForArgs, ArgBuffer = bindable->buffer()](ArgumentEncodingContext &enc) {
        auto buffer = enc.access(ArgBuffer, AlignedByteOffsetForArgs, 12, DXMT_ENCODER_RESOURCE_ACESS_READ);
        enc.encodeComputeCommand([&, buffer = Obj(buffer)](ComputeCommandContext &ctx) {
          ctx.encoder->dispatchThreadgroups(buffer, AlignedByteOffsetForArgs, ctx.threadgroup_size);
        });
      });
    }
  }
#pragma endregion

#pragma region State API

  void
  GetPredication(ID3D11Predicate **ppPredicate, BOOL *pPredicateValue) override {

    if (ppPredicate) {
      *ppPredicate = state_.predicate.ref();
    }
    if (pPredicateValue) {
      *pPredicateValue = state_.predicate_value;
    }

    ERR_ONCE("Stub");
  }

  void
  SetPredication(ID3D11Predicate *pPredicate, BOOL PredicateValue) override {

    state_.predicate = pPredicate;
    state_.predicate_value = PredicateValue;

    EmitST([](ArgumentEncodingContext &enc) { enc.setCompatibilityFlag(FeatureCompatibility::UnsupportedPredication); });
  }

  //-----------------------------------------------------------------------------
  // State Machine
  //-----------------------------------------------------------------------------

  void
  SwapDeviceContextState(ID3DDeviceContextState *pState, ID3DDeviceContextState **ppPreviousState) override {
    IMPLEMENT_ME
  }

  void
  ClearState() override {
    ResetEncodingContextState();
    ResetD3D11ContextState();
  }

#pragma region InputAssembler

  void
  IASetInputLayout(ID3D11InputLayout *pInputLayout) override {
    if (auto expected = com_cast<IMTLD3D11InputLayout>(pInputLayout)) {
      state_.InputAssembler.InputLayout = std::move(expected);
    } else {
      state_.InputAssembler.InputLayout = nullptr;
    }
    InvalidateRenderPipeline();
  }
  void
  IAGetInputLayout(ID3D11InputLayout **ppInputLayout) override {
    if (ppInputLayout) {
      if (state_.InputAssembler.InputLayout) {
        state_.InputAssembler.InputLayout->QueryInterface(IID_PPV_ARGS(ppInputLayout));
      } else {
        *ppInputLayout = nullptr;
      }
    }
  }

  void
  IASetVertexBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets
  ) override {
    SetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
  }

  void
  IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppVertexBuffers, UINT *pStrides, UINT *pOffsets)
      override {
    GetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
  }

  void
  IASetIndexBuffer(ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format, UINT Offset) override {
    if (auto expected = reinterpret_cast<D3D11ResourceCommon *>(pIndexBuffer)) {
      state_.InputAssembler.IndexBuffer = expected;
      EmitST([buffer = state_.InputAssembler.IndexBuffer->buffer()](ArgumentEncodingContext &enc) mutable {
        enc.bindIndexBuffer(forward_rc(buffer));
      });
    } else {
      state_.InputAssembler.IndexBuffer = nullptr;
      EmitST([](ArgumentEncodingContext &enc) { enc.bindIndexBuffer({}); });
    }
    state_.InputAssembler.IndexBufferFormat = Format;
    state_.InputAssembler.IndexBufferOffset = Offset;
  }
  void
  IAGetIndexBuffer(ID3D11Buffer **pIndexBuffer, DXGI_FORMAT *Format, UINT *Offset) override {
    if (pIndexBuffer) {
      if (state_.InputAssembler.IndexBuffer) {
        state_.InputAssembler.IndexBuffer->QueryInterface(IID_PPV_ARGS(pIndexBuffer));
      } else {
        *pIndexBuffer = nullptr;
      }
    }
    if (Format != NULL)
      *Format = state_.InputAssembler.IndexBufferFormat;
    if (Offset != NULL)
      *Offset = state_.InputAssembler.IndexBufferOffset;
  }
  void
  IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) override {
    state_.InputAssembler.Topology = Topology;
  }
  void
  IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY *pTopology) override {
    if (pTopology) {
      *pTopology = state_.InputAssembler.Topology;
    }
  }
#pragma endregion

#pragma region VertexShader

  void
  VSSetShader(ID3D11VertexShader *pVertexShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
      override {
    SetShader<PipelineStage::Vertex, ID3D11VertexShader>(pVertexShader, ppClassInstances, NumClassInstances);
  }

  void
  VSGetShader(ID3D11VertexShader **ppVertexShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
      override {
    GetShader<PipelineStage::Vertex, ID3D11VertexShader>(ppVertexShader, ppClassInstances, pNumClassInstances);
  }

  void
  VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    SetShaderResource<PipelineStage::Vertex>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override {
    GetShaderResource<PipelineStage::Vertex>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override {
    SetSamplers<PipelineStage::Vertex>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override {
    GetSamplers<PipelineStage::Vertex>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override {
    VSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void
  VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override {
    VSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void
  VSSetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant,
      const UINT *pNumConstants
  ) override {
    SetConstantBuffer<PipelineStage::Vertex>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  VSGetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants
  ) override {
    GetConstantBuffer<PipelineStage::Vertex>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

#pragma endregion

#pragma region PixelShader

  void
  PSSetShader(ID3D11PixelShader *pPixelShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
      override {
    SetShader<PipelineStage::Pixel, ID3D11PixelShader>(pPixelShader, ppClassInstances, NumClassInstances);
  }

  void
  PSGetShader(ID3D11PixelShader **ppPixelShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
      override {
    GetShader<PipelineStage::Pixel, ID3D11PixelShader>(ppPixelShader, ppClassInstances, pNumClassInstances);
  }

  void
  PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    SetShaderResource<PipelineStage::Pixel>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override {
    GetShaderResource<PipelineStage::Pixel>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override {
    SetSamplers<PipelineStage::Pixel>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override {

    GetSamplers<PipelineStage::Pixel>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override {
    PSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, 0, 0);
  }

  void
  PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override {
    PSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, 0, 0);
  }

  void
  PSSetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant,
      const UINT *pNumConstants
  ) override {
    SetConstantBuffer<PipelineStage::Pixel>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  PSGetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants
  ) override {
    GetConstantBuffer<PipelineStage::Pixel>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

#pragma endregion

#pragma region GeometryShader

  void
  GSSetShader(ID3D11GeometryShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
      override {
    SetShader<PipelineStage::Geometry>(pShader, ppClassInstances, NumClassInstances);
  }

  void
  GSGetShader(ID3D11GeometryShader **ppGeometryShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
      override {
    GetShader<PipelineStage::Geometry>(ppGeometryShader, ppClassInstances, pNumClassInstances);
  }

  void
  GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override {
    GSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void
  GSSetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant,
      const UINT *pNumConstants
  ) override {
    SetConstantBuffer<PipelineStage::Geometry>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override {
    GSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void
  GSGetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants
  ) override {
    GetConstantBuffer<PipelineStage::Geometry>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    SetShaderResource<PipelineStage::Geometry>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override {
    GetShaderResource<PipelineStage::Geometry>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override {
    SetSamplers<PipelineStage::Geometry>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override {
    GetSamplers<PipelineStage::Geometry>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  SOSetTargets(UINT NumBuffers, ID3D11Buffer *const *ppSOTargets, const UINT *pOffsets) override {
    if (NumBuffers == 0) {
      NumBuffers = 4; // see msdn description of SOSetTargets
    }
    for (unsigned slot = 0; slot < NumBuffers; slot++) {
      auto pBuffer = ppSOTargets ? ppSOTargets[slot] : nullptr;
      if (pBuffer) {
        bool replaced = false;
        auto &entry = state_.StreamOutput.Targets.bind(slot, {pBuffer}, replaced);
        if (!replaced) {
          auto offset = pOffsets ? pOffsets[slot] : 0;
          if (offset != entry.Offset) {
            state_.StreamOutput.Targets.set_dirty(slot);
            entry.Offset = offset;
          }
          continue;
        }
        entry.Buffer = reinterpret_cast<D3D11ResourceCommon *>(pBuffer);
        entry.Offset = pOffsets ? pOffsets[slot] : 0;
      } else {
        state_.StreamOutput.Targets.unbind(slot);
      }
    }
  }

  void
  SOGetTargets(UINT NumBuffers, ID3D11Buffer **ppSOTargets) override {
    if (!ppSOTargets)
      return;
    for (unsigned i = 0; i < std::min(4u, NumBuffers); i++) {
      if (state_.StreamOutput.Targets.test_bound(i)) {
        state_.StreamOutput.Targets[i].Buffer->QueryInterface(IID_PPV_ARGS(&ppSOTargets[i]));
      } else {
        ppSOTargets[i] = nullptr;
      }
    }
  }

#pragma endregion

#pragma region HullShader

  void
  HSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override {
    GetShaderResource<PipelineStage::Hull>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  HSGetShader(ID3D11HullShader **ppHullShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
      override {
    GetShader<PipelineStage::Hull>(ppHullShader, ppClassInstances, pNumClassInstances);
  }

  void
  HSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override {
    GetSamplers<PipelineStage::Hull>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override {
    HSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void
  HSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    SetShaderResource<PipelineStage::Hull>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  HSSetShader(ID3D11HullShader *pHullShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
      override {
    SetShader<PipelineStage::Hull>(pHullShader, ppClassInstances, NumClassInstances);
  }

  void
  HSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override {
    SetSamplers<PipelineStage::Hull>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override {
    HSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void
  HSSetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant,
      const UINT *pNumConstants
  ) override {
    SetConstantBuffer<PipelineStage::Hull>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  HSGetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants
  ) override {
    GetConstantBuffer<PipelineStage::Hull>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

#pragma endregion

#pragma region DomainShader

  void
  DSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    SetShaderResource<PipelineStage::Domain>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  DSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override {
    GetShaderResource<PipelineStage::Domain>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  DSSetShader(ID3D11DomainShader *pDomainShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
      override {
    SetShader<PipelineStage::Domain, ID3D11DomainShader>(pDomainShader, ppClassInstances, NumClassInstances);
  }

  void
  DSGetShader(ID3D11DomainShader **ppDomainShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
      override {
    GetShader<PipelineStage::Domain, ID3D11DomainShader>(ppDomainShader, ppClassInstances, pNumClassInstances);
  }

  void
  DSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override {
    GetSamplers<PipelineStage::Domain>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  DSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override {
    SetSamplers<PipelineStage::Domain>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override {
    return DSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void
  DSSetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant,
      const UINT *pNumConstants
  ) override {
    SetConstantBuffer<PipelineStage::Domain>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override {
    UINT *pFirstConstant = 0, *pNumConstants = 0;
    return DSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  DSGetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants
  ) override {
    GetConstantBuffer<PipelineStage::Domain>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

#pragma endregion

#pragma region ComputeShader

  void
  CSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override {
    GetShaderResource<PipelineStage::Compute>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  CSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    SetShaderResource<PipelineStage::Compute>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  CSSetUnorderedAccessViews(
      UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
      const UINT *pUAVInitialCounts
  ) override {
    SetUnorderedAccessView<PipelineStage::Compute>(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
  }

  void
  CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews) override {
    for (auto i = 0u; i < NumUAVs; i++) {
      if (state_.ComputeStageUAV.UAVs.test_bound(StartSlot + i)) {
        state_.ComputeStageUAV.UAVs[StartSlot + i].View->QueryInterface(
            IID_PPV_ARGS(&ppUnorderedAccessViews[i])
        );
      } else {
        ppUnorderedAccessViews[i] = nullptr;
      }
    }
  }

  void
  CSSetShader(ID3D11ComputeShader *pComputeShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
      override {
    SetShader<PipelineStage::Compute>(pComputeShader, ppClassInstances, NumClassInstances);
  }

  void
  CSGetShader(ID3D11ComputeShader **ppComputeShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
      override {
    GetShader<PipelineStage::Compute>(ppComputeShader, ppClassInstances, pNumClassInstances);
  }

  void
  CSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override {
    SetSamplers<PipelineStage::Compute>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  CSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override {
    GetSamplers<PipelineStage::Compute>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) override {
    CSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void
  CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override {
    CSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void
  CSSetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant,
      const UINT *pNumConstants
  ) override {
    SetConstantBuffer<PipelineStage::Compute>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  CSGetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants
  ) override {
    GetConstantBuffer<PipelineStage::Compute>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

#pragma endregion

#pragma region OutputMerger

  void
  OMSetRenderTargets(
      UINT NumViews, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView
  ) override {
    OMSetRenderTargetsAndUnorderedAccessViews(
        NumViews, ppRenderTargetViews, pDepthStencilView, 0, D3D11_KEEP_UNORDERED_ACCESS_VIEWS, NULL, NULL
    );
  }

  void
  OMGetRenderTargets(
      UINT NumViews, ID3D11RenderTargetView **ppRenderTargetViews, ID3D11DepthStencilView **ppDepthStencilView
  ) override {
    OMGetRenderTargetsAndUnorderedAccessViews(NumViews, ppRenderTargetViews, ppDepthStencilView, 0, 0, NULL);
  }

  bool
  ValidateSetRenderTargets(
      UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView
  ) {
    MTL_RENDER_PASS_ATTACHMENT_DESC *ref = nullptr;

    // FIXME: static_cast? but I don't really want a com_cast
    auto dsv = static_cast<IMTLD3D11DepthStencilView *>(pDepthStencilView);
    UINT render_target_array_length = 0;

    if (dsv) {
      ref = &dsv->GetAttachmentDesc();
      render_target_array_length = ref->RenderTargetArrayLength;
    }

    for (unsigned i = 0; i < NumRTVs; i++) {
      auto rtv = static_cast<IMTLD3D11RenderTargetView *>(ppRenderTargetViews[i]);
      if (rtv) {
        // TODO: render target type and size should be checked as well
        if (ref) {
          auto &props = rtv->GetAttachmentDesc();
          if (props.SampleCount != ref->SampleCount)
            return false;
          if (props.RenderTargetArrayLength != ref->RenderTargetArrayLength) {
            // array length can be different only if either is 0 or 1
            if (std::max(props.RenderTargetArrayLength, ref->RenderTargetArrayLength) != 1)
              return false;
          }
          // render_target_array_length will be 1 only if all render targets are array
          render_target_array_length = std::min(render_target_array_length, props.RenderTargetArrayLength);
        } else {
          ref = &rtv->GetAttachmentDesc();
          render_target_array_length = ref->RenderTargetArrayLength;
        }
      }
    }

    if (ref) {
      state_.OutputMerger.SampleCount = ref->SampleCount;
      state_.OutputMerger.ArrayLength = render_target_array_length;
    } else {
      state_.OutputMerger.SampleCount = 1;
      state_.OutputMerger.ArrayLength = 0;
    }

    return true;
  };

  void
  OMSetRenderTargetsAndUnorderedAccessViews(
      UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews, ID3D11DepthStencilView *pDepthStencilView,
      UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
      const UINT *pUAVInitialCounts
  ) override {

    bool should_invalidate_pass = false;

    if (NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL) {
      if (!ValidateSetRenderTargets(NumRTVs, ppRenderTargetViews, pDepthStencilView)) {
        WARN("OMSetRenderTargets: invalid render targets");
        return;
      }
      auto &BoundRTVs = state_.OutputMerger.RTVs;
      constexpr unsigned RTVSlotCount = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
      for (unsigned rtv_index = 0; rtv_index < RTVSlotCount; rtv_index++) {
        if (rtv_index < NumRTVs && ppRenderTargetViews[rtv_index]) {
          auto rtv = static_cast<IMTLD3D11RenderTargetView *>(ppRenderTargetViews[rtv_index]);
          if (BoundRTVs[rtv_index].ptr() == rtv)
            continue;
          BoundRTVs[rtv_index] = rtv;
          should_invalidate_pass = true;
        } else {
          if (BoundRTVs[rtv_index]) {
            should_invalidate_pass = true;
          }
          BoundRTVs[rtv_index] = nullptr;
        }
      }
      state_.OutputMerger.NumRTVs = NumRTVs;

      if (auto dsv = static_cast<IMTLD3D11DepthStencilView *>(pDepthStencilView)) {
        if (state_.OutputMerger.DSV.ptr() != dsv) {
          state_.OutputMerger.DSV = dsv;
          should_invalidate_pass = true;
        }
      } else {
        if (state_.OutputMerger.DSV) {
          should_invalidate_pass = true;
        }
        state_.OutputMerger.DSV = nullptr;
      }
    }

    if (NumUAVs != D3D11_KEEP_UNORDERED_ACCESS_VIEWS) {
      SetUnorderedAccessView<PipelineStage::Pixel>(UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
    }

    if (should_invalidate_pass) {
      InvalidateCurrentPass();
    }
  }

  void
  OMGetRenderTargetsAndUnorderedAccessViews(
      UINT NumRTVs, ID3D11RenderTargetView **ppRenderTargetViews, ID3D11DepthStencilView **ppDepthStencilView,
      UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews
  ) override {
    if (ppRenderTargetViews) {
      for (unsigned i = 0; i < NumRTVs; i++) {
        if (i < state_.OutputMerger.NumRTVs && state_.OutputMerger.RTVs[i]) {
          state_.OutputMerger.RTVs[i]->QueryInterface(IID_PPV_ARGS(&ppRenderTargetViews[i]));
        } else {
          ppRenderTargetViews[i] = nullptr;
        }
      }
    }
    if (ppDepthStencilView) {
      if (state_.OutputMerger.DSV) {
        state_.OutputMerger.DSV->QueryInterface(IID_PPV_ARGS(ppDepthStencilView));
      } else {
        *ppDepthStencilView = nullptr;
      }
    }
    if (ppUnorderedAccessViews) {
      for (unsigned i = 0; i < NumUAVs; i++) {
        if (state_.OutputMerger.UAVs.test_bound(i + UAVStartSlot)) {
          state_.OutputMerger.UAVs[i + UAVStartSlot].View->QueryInterface(IID_PPV_ARGS(&ppUnorderedAccessViews[i]));
        } else {
          ppUnorderedAccessViews[i] = nullptr;
        }
      }
    }
  }

  void
  OMSetBlendState(ID3D11BlendState *pBlendState, const FLOAT BlendFactor[4], UINT SampleMask) override {
    bool should_invalidate_pipeline = false;
    if (auto expected = com_cast<IMTLD3D11BlendState>(pBlendState)) {
      if (expected.ptr() != state_.OutputMerger.BlendState) {
        state_.OutputMerger.BlendState = expected.ptr();
        should_invalidate_pipeline = true;
      }
    } else {
      if (state_.OutputMerger.BlendState) {
        state_.OutputMerger.BlendState = nullptr;
        should_invalidate_pipeline = true;
      }
    }
    if (BlendFactor) {
      memcpy(state_.OutputMerger.BlendFactor, BlendFactor, sizeof(float[4]));
    } else {
      state_.OutputMerger.BlendFactor[0] = 1.0f;
      state_.OutputMerger.BlendFactor[1] = 1.0f;
      state_.OutputMerger.BlendFactor[2] = 1.0f;
      state_.OutputMerger.BlendFactor[3] = 1.0f;
    }
    if (state_.OutputMerger.SampleMask != SampleMask) {
      state_.OutputMerger.SampleMask = SampleMask;
      should_invalidate_pipeline = true;
    }
    if (should_invalidate_pipeline) {
      InvalidateRenderPipeline();
    }
    dirty_state.set(DirtyState::BlendFactorAndStencilRef);
  }
  void
  OMGetBlendState(ID3D11BlendState **ppBlendState, FLOAT BlendFactor[4], UINT *pSampleMask) override {
    if (ppBlendState) {
      if (state_.OutputMerger.BlendState) {
        state_.OutputMerger.BlendState->QueryInterface(IID_PPV_ARGS(ppBlendState));
      } else {
        *ppBlendState = nullptr;
      }
    }
    if (BlendFactor) {
      memcpy(BlendFactor, state_.OutputMerger.BlendFactor, sizeof(float[4]));
    }
    if (pSampleMask) {
      *pSampleMask = state_.OutputMerger.SampleMask;
    }
  }

  void
  OMSetDepthStencilState(ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef) override {
    if (auto expected = com_cast<IMTLD3D11DepthStencilState>(pDepthStencilState)) {
      state_.OutputMerger.DepthStencilState = expected.ptr();
    } else {
      state_.OutputMerger.DepthStencilState = nullptr;
    }
    state_.OutputMerger.StencilRef = StencilRef;
    dirty_state.set(DirtyState::DepthStencilState);
  }

  void
  OMGetDepthStencilState(ID3D11DepthStencilState **ppDepthStencilState, UINT *pStencilRef) override {
    if (ppDepthStencilState) {
      if (state_.OutputMerger.DepthStencilState) {
        state_.OutputMerger.DepthStencilState->QueryInterface(IID_PPV_ARGS(ppDepthStencilState));
      } else {
        *ppDepthStencilState = nullptr;
      }
    }
    if (pStencilRef) {
      *pStencilRef = state_.OutputMerger.StencilRef;
    }
  }

#pragma endregion

#pragma region Rasterizer

  void
  RSSetState(ID3D11RasterizerState *pRasterizerState) override {
    if (pRasterizerState) {
      if (auto expected = com_cast<IMTLD3D11RasterizerState>(pRasterizerState)) {
        auto current_rs =
            state_.Rasterizer.RasterizerState ? state_.Rasterizer.RasterizerState : default_rasterizer_state;
        if (current_rs == expected.ptr()) {
          return;
        }
        if (current_rs->IsScissorEnabled() != expected->IsScissorEnabled()) {
          dirty_state.set(DirtyState::Scissors);
        }
        state_.Rasterizer.RasterizerState = expected.ptr();
        dirty_state.set(DirtyState::RasterizerState);
      } else {
        ERR("RSSetState: invalid ID3D11RasterizerState object.");
      }
    } else {
      if (state_.Rasterizer.RasterizerState) {
        if (state_.Rasterizer.RasterizerState->IsScissorEnabled() != default_rasterizer_state->IsScissorEnabled()) {
          dirty_state.set(DirtyState::Scissors);
        }
      }
      state_.Rasterizer.RasterizerState = nullptr;
      dirty_state.set(DirtyState::RasterizerState);
    }
  }

  void
  RSGetState(ID3D11RasterizerState **ppRasterizerState) override {
    if (ppRasterizerState) {
      if (state_.Rasterizer.RasterizerState) {
        state_.Rasterizer.RasterizerState->QueryInterface(IID_PPV_ARGS(ppRasterizerState));
      } else {
        *ppRasterizerState = NULL;
      }
    }
  }

  void
  RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT *pViewports) override {
    if (NumViewports > 16)
      return;
    if (state_.Rasterizer.NumViewports == NumViewports &&
        memcmp(state_.Rasterizer.viewports, pViewports, NumViewports * sizeof(D3D11_VIEWPORT)) == 0) {
      return;
    }
    state_.Rasterizer.NumViewports = NumViewports;
    for (auto i = 0u; i < NumViewports; i++) {
      state_.Rasterizer.viewports[i] = pViewports[i];
    }
    dirty_state.set(DirtyState::Viewport);
    IMTLD3D11RasterizerState *current_rs =
        state_.Rasterizer.RasterizerState ? state_.Rasterizer.RasterizerState : default_rasterizer_state;
    if (!current_rs->IsScissorEnabled()) {
      dirty_state.set(DirtyState::Scissors);
    }
  }

  void
  RSGetViewports(UINT *pNumViewports, D3D11_VIEWPORT *pViewports) override {
    if (pNumViewports) {
      *pNumViewports = state_.Rasterizer.NumViewports;
    }
    if (pViewports) {
      for (auto i = 0u; i < state_.Rasterizer.NumViewports; i++) {
        pViewports[i] = state_.Rasterizer.viewports[i];
      }
    }
  }

  void
  RSSetScissorRects(UINT NumRects, const D3D11_RECT *pRects) override {
    if (NumRects > 16)
      return;
    if (state_.Rasterizer.NumScissorRects == NumRects &&
        memcmp(state_.Rasterizer.scissor_rects, pRects, NumRects * sizeof(D3D11_RECT)) == 0) {
      return;
    }
    state_.Rasterizer.NumScissorRects = NumRects;
    for (unsigned i = 0; i < NumRects; i++) {
      state_.Rasterizer.scissor_rects[i] = pRects[i];
    }
    IMTLD3D11RasterizerState *current_rs =
        state_.Rasterizer.RasterizerState ? state_.Rasterizer.RasterizerState : default_rasterizer_state;
    if (current_rs->IsScissorEnabled()) {
      dirty_state.set(DirtyState::Scissors);
      // otherwise no need to update scissor because it's either already dirty
      // or duplicates viewports
    }
  }

  void
  RSGetScissorRects(UINT *pNumRects, D3D11_RECT *pRects) override {
    if (pNumRects) {
      *pNumRects = state_.Rasterizer.NumScissorRects;
    }
    if (pRects) {
      for (auto i = 0u; i < state_.Rasterizer.NumScissorRects; i++) {
        pRects[i] = state_.Rasterizer.scissor_rects[i];
      }
    }
  }
#pragma endregion

#pragma region D3D11DeviceContext2

  HRESULT STDMETHODCALLTYPE UpdateTileMappings(
      ID3D11Resource *resource, UINT region_count, const D3D11_TILED_RESOURCE_COORDINATE *region_start_coordinates,
      const D3D11_TILE_REGION_SIZE *region_sizes, ID3D11Buffer *pool, UINT range_count, const UINT *range_flags,
      const UINT *pool_start_offsets, const UINT *range_tile_counts, UINT flags
  ) override {
    IMPLEMENT_ME
  };

  HRESULT STDMETHODCALLTYPE CopyTileMappings(
      ID3D11Resource *dst_resource, const D3D11_TILED_RESOURCE_COORDINATE *dst_start_coordinate,
      ID3D11Resource *src_resource, const D3D11_TILED_RESOURCE_COORDINATE *src_start_coordinate,
      const D3D11_TILE_REGION_SIZE *region_size, UINT flags
  ) override {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE CopyTiles(
      ID3D11Resource *resource, const D3D11_TILED_RESOURCE_COORDINATE *start_coordinate,
      const D3D11_TILE_REGION_SIZE *size, ID3D11Buffer *buffer, UINT64 start_offset, UINT flags
  ) override {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE UpdateTiles(
      ID3D11Resource *dst_resource, const D3D11_TILED_RESOURCE_COORDINATE *dst_start_coordinate,
      const D3D11_TILE_REGION_SIZE *dst_region_size, const void *src_data, UINT flags
  ) override {
    IMPLEMENT_ME
  };

  HRESULT STDMETHODCALLTYPE ResizeTilePool(ID3D11Buffer *pool, UINT64 size) override { IMPLEMENT_ME };

  void STDMETHODCALLTYPE
  TiledResourceBarrier(ID3D11DeviceChild *before_barrier, ID3D11DeviceChild *after_barrier) override {
    IMPLEMENT_ME
  };

  WINBOOL STDMETHODCALLTYPE IsAnnotationEnabled() override { IMPLEMENT_ME };

  void STDMETHODCALLTYPE SetMarkerInt(const WCHAR *label, int data) override { IMPLEMENT_ME };

  void STDMETHODCALLTYPE BeginEventInt(const WCHAR *label, int data) override { IMPLEMENT_ME };

  void STDMETHODCALLTYPE EndEvent() override { IMPLEMENT_ME };

#pragma endregion

#pragma region Misc

  UINT
  GetContextFlags() override {
    return 0;
  }

#pragma endregion

#pragma region 11.3

  void STDMETHODCALLTYPE
  Flush1(D3D11_CONTEXT_TYPE type, HANDLE event) override {
    IMPLEMENT_ME;
  }

  void STDMETHODCALLTYPE
  SetHardwareProtectionState(WINBOOL enable) override {
    WARN("SetHardwareProtectionState: stub");
  }

  void STDMETHODCALLTYPE
  GetHardwareProtectionState(WINBOOL *enable) override {
    *enable = false;
    WARN("GetHardwareProtectionState: stub");
  }

  virtual HRESULT STDMETHODCALLTYPE
  Signal(ID3D11Fence *fence, UINT64 value) override {
    IMPLEMENT_ME
  }

  virtual HRESULT STDMETHODCALLTYPE
  Wait(ID3D11Fence *fence, UINT64 value) override {
    IMPLEMENT_ME
  }

#pragma endregion

#pragma region Internal

  template <CommandWithContext<ArgumentEncodingContext> cmd> void EmitST(cmd &&fn);
  template <CommandWithContext<ArgumentEncodingContext> cmd> void EmitOP(cmd &&fn);

  template <typename T> moveonly_list<T> AllocateCommandData(size_t n = 1);

  std::tuple<void *, MTL::Buffer *, uint64_t> AllocateStagingBuffer(size_t size, size_t alignment);
  void UseCopyDestination(Rc<StagingResource> &);
  void UseCopySource(Rc<StagingResource> &);

  template <PipelineStage stage, bool Tessellation>
  bool
  UploadShaderStageResourceBinding() {
    auto &ShaderStage = state_.ShaderStages[stage];
    if (!ShaderStage.Shader) {
      return true;
    }
    auto &UAVBindingSet = stage == PipelineStage::Compute ? state_.ComputeStageUAV.UAVs : state_.OutputMerger.UAVs;

    const MTL_SHADER_REFLECTION *reflection = &ShaderStage.Shader->GetManagedShader()->reflection();

    bool dirty_cbuffer = ShaderStage.ConstantBuffers.any_dirty_masked(reflection->ConstantBufferSlotMask);
    bool dirty_sampler = ShaderStage.Samplers.any_dirty_masked(reflection->SamplerSlotMask);
    bool dirty_srv = ShaderStage.SRVs.any_dirty_masked(reflection->SRVSlotMaskHi, reflection->SRVSlotMaskLo);
    bool dirty_uav = UAVBindingSet.any_dirty_masked(reflection->UAVSlotMask);
    if (!dirty_cbuffer && !dirty_sampler && !dirty_srv && !dirty_uav)
      return true;

    if (reflection->NumConstantBuffers && dirty_cbuffer) {
      EmitST([reflection](ArgumentEncodingContext &enc) { enc.encodeConstantBuffers<stage, Tessellation>(reflection); });
      ShaderStage.ConstantBuffers.clear_dirty();
    }

    if (reflection->NumArguments && (dirty_sampler || dirty_srv || dirty_uav)) {
      EmitST([reflection](ArgumentEncodingContext &enc) { enc.encodeShaderResources<stage, Tessellation>(reflection); });
      ShaderStage.Samplers.clear_dirty();
      ShaderStage.SRVs.clear_dirty();
      if (stage == PipelineStage::Pixel || stage == PipelineStage::Compute) {
        UAVBindingSet.clear_dirty();
      }
    }
    return true;
  }

  void
  UpdateVertexBuffer() {
    if (!state_.InputAssembler.InputLayout)
      return;

    uint32_t slot_mask = state_.InputAssembler.InputLayout->GetManagedInputLayout()->input_slot_mask();
    if (slot_mask == 0) // effectively empty input layout
      return;

    auto &VertexBuffers = state_.InputAssembler.VertexBuffers;
    if (!VertexBuffers.any_dirty_masked((uint64_t)slot_mask))
      return;

    if (cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady) {
      EmitST([slot_mask](ArgumentEncodingContext &enc) { enc.encodeVertexBuffers<true>(slot_mask); });
    }
    if (cmdbuf_state == CommandBufferState::RenderPipelineReady) {
      EmitST([slot_mask](ArgumentEncodingContext &enc) { enc.encodeVertexBuffers<false>(slot_mask); });
    }

    VertexBuffers.clear_dirty_mask(slot_mask);
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

  template <PipelineStage stage, typename IShader>
  void
  SetShader(IShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) {
    auto &ShaderStage = state_.ShaderStages[stage];

    if (pShader) {
      if (auto expected = com_cast<IMTLD3D11Shader>(pShader)) {
        if (ShaderStage.Shader.ptr() == expected.ptr()) {
          return;
        }
        ShaderStage.Shader = std::move(expected);
        ShaderStage.ConstantBuffers.set_dirty();
        ShaderStage.SRVs.set_dirty();
        ShaderStage.Samplers.set_dirty();
        if constexpr (stage == PipelineStage::Vertex) {
          state_.InputAssembler.VertexBuffers.set_dirty();
        }
        if constexpr (stage == PipelineStage::Compute) {
          state_.ComputeStageUAV.UAVs.set_dirty();
        } else {
          state_.OutputMerger.UAVs.set_dirty();
        }
      } else {
        D3D11_ASSERT(0 && "unexpected shader object");
      }
    } else {
      ShaderStage.Shader = nullptr;
    }

    if (NumClassInstances)
      ERR("Class instances not supported");

    if constexpr (stage == PipelineStage::Compute) {
      InvalidateComputePipeline();
    } else {
      InvalidateRenderPipeline();
    }
  }

  template <PipelineStage Type, typename IShader>
  void
  GetShader(IShader **ppShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) {
    auto &ShaderStage = state_.ShaderStages[Type];

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

  template <PipelineStage Stage>
  void
  SetConstantBuffer(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant,
      const UINT *pNumConstants
  ) {
    auto &ShaderStage = state_.ShaderStages[Stage];

    for (unsigned slot = StartSlot; slot < StartSlot + NumBuffers; slot++) {
      auto pConstantBuffer = ppConstantBuffers[slot - StartSlot];
      if (pConstantBuffer) {
        bool replaced = false;
        auto &entry = ShaderStage.ConstantBuffers.bind(slot, {pConstantBuffer}, replaced);
        if (!replaced) {
          if (pFirstConstant &&
              pFirstConstant[slot - StartSlot] != entry.FirstConstant) {
            ShaderStage.ConstantBuffers.set_dirty(slot);
            entry.FirstConstant = pFirstConstant[slot - StartSlot];
            EmitST([=, offset = entry.FirstConstant
                                << 4](ArgumentEncodingContext &enc) mutable {
              enc.bindConstantBufferOffset<Stage>(slot, offset);
            });
          }
          if (pNumConstants && pNumConstants[slot - StartSlot] != entry.NumConstants) {
            ShaderStage.ConstantBuffers.set_dirty(slot);
            entry.NumConstants = pNumConstants[slot - StartSlot];
          }
          continue;
        }
        entry.FirstConstant = pFirstConstant ? pFirstConstant[slot - StartSlot] : 0;
        if (pNumConstants) {
          entry.NumConstants = pNumConstants[slot - StartSlot];
        } else {
          D3D11_BUFFER_DESC desc;
          pConstantBuffer->GetDesc(&desc);
          entry.NumConstants = desc.ByteWidth >> 4;
        }
        entry.Buffer = reinterpret_cast<D3D11ResourceCommon *>(pConstantBuffer);
        EmitST([=, buffer = entry.Buffer->buffer(), offset = entry.FirstConstant << 4](ArgumentEncodingContext &enc
             ) mutable { enc.bindConstantBuffer<Stage>(slot, offset, forward_rc(buffer)); });
      } else {
        // BIND NULL
        if (ShaderStage.ConstantBuffers.unbind(slot)) {
          EmitST([=](ArgumentEncodingContext &enc) { enc.bindConstantBuffer<Stage>(slot, 0, {}); });
        }
      }
    }
  }

  template <PipelineStage Stage>
  void
  GetConstantBuffer(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants
  ) {
    auto &ShaderStage = state_.ShaderStages[Stage];

    for (auto i = 0u; i < NumBuffers; i++) {
      if (ShaderStage.ConstantBuffers.test_bound(StartSlot + i)) {
        auto &cb = ShaderStage.ConstantBuffers.at(StartSlot + i);
        if (ppConstantBuffers) {
          cb.Buffer->QueryInterface(IID_PPV_ARGS(&ppConstantBuffers[i]));
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

  template <PipelineStage Stage>
  void
  SetShaderResource(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) {

    auto &ShaderStage = state_.ShaderStages[Stage];
    for (unsigned slot = StartSlot; slot < StartSlot + NumViews; slot++) {
      auto pView = static_cast<D3D11ShaderResourceView *>(ppShaderResourceViews[slot - StartSlot]);
      if (pView) {
        bool replaced = false;
        auto &entry = ShaderStage.SRVs.bind(slot, {pView}, replaced);
        if (!replaced)
          continue;
        entry.SRV = pView;
        if (auto buffer = entry.SRV->buffer()) {
          EmitST([=, buffer = std::move(buffer), viewId = pView->viewId(),
                slice = pView->bufferSlice()](ArgumentEncodingContext &enc) mutable {
            enc.bindBuffer<Stage>(slot, forward_rc(buffer), viewId, slice);
          });
        } else {
          EmitST([=, texture = pView->texture(), viewId = pView->viewId()](ArgumentEncodingContext &enc) mutable {
            enc.bindTexture<Stage>(slot, forward_rc(texture), viewId);
          });
        }
      } else {
        // BIND NULL
        if (ShaderStage.SRVs.unbind(slot)) {
          EmitST([=](ArgumentEncodingContext& enc) {
            enc.bindBuffer<Stage>(slot, {}, 0, {});
          });
        }
      }
    }
  }

  template <PipelineStage Stage>
  void
  GetShaderResource(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) {
    auto &ShaderStage = state_.ShaderStages[Stage];

    if (!ppShaderResourceViews)
      return;
    for (auto i = 0u; i < NumViews; i++) {
      if (ShaderStage.SRVs.test_bound(StartSlot + i)) {
        ShaderStage.SRVs[StartSlot + i].SRV->QueryInterface(IID_PPV_ARGS(&ppShaderResourceViews[i]));
      } else {
        ppShaderResourceViews[i] = nullptr;
      }
    }
  }

  template <PipelineStage Stage>
  void
  SetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) {
    auto &ShaderStage = state_.ShaderStages[Stage];
    for (unsigned Slot = StartSlot; Slot < StartSlot + NumSamplers; Slot++) {
      auto pSampler = ppSamplers[Slot - StartSlot];
      if (pSampler) {
        bool replaced = false;
        auto &entry = ShaderStage.Samplers.bind(Slot, {pSampler}, replaced);
        if (!replaced)
          continue;
        if (auto expected = com_cast<IMTLD3D11SamplerState>(pSampler)) {
          entry.Sampler = expected.ptr();
          EmitST([=, sampler = entry.Sampler](ArgumentEncodingContext &enc) {
            enc.bindSampler<Stage>(Slot, sampler->GetSamplerState(), sampler->GetLODBias());
          });
        } else {
          D3D11_ASSERT(0 && "wtf");
        }
      } else {
        // BIND NULL
        if (ShaderStage.Samplers.unbind(Slot)) {
          EmitST([=](ArgumentEncodingContext& enc) {
            enc.bindSampler<Stage>(Slot, nullptr, 0);
          });
        }
      }
    }
  }

  template <PipelineStage Stage>
  void
  GetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) {
    auto &ShaderStage = state_.ShaderStages[Stage];
    if (ppSamplers) {
      for (unsigned Slot = StartSlot; Slot < StartSlot + NumSamplers; Slot++) {
        if (ShaderStage.Samplers.test_bound(Slot)) {
          ShaderStage.Samplers[Slot].Sampler->QueryInterface(IID_PPV_ARGS(&ppSamplers[Slot - StartSlot]));
        } else {
          ppSamplers[Slot - StartSlot] = nullptr;
        }
      }
    }
  }

  void
  UpdateUAVCounter(D3D11UnorderedAccessView *uav, uint32_t value) {
    EmitST([counter = uav->counter(), value](ArgumentEncodingContext &enc) {
      if (!counter.ptr())
        return;
      auto new_counter = counter->allocate(BufferAllocationFlag::GpuManaged);
      *reinterpret_cast<uint32_t *>(new_counter->mappedMemory) = value;
      new_counter->buffer()->didModifyRange({0, 4});
      auto old = counter->rename(std::move(new_counter));
      // TODO: reused discarded buffer
    });
  }

  template <PipelineStage Stage>
  void
  SetUnorderedAccessView(
      UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
      const UINT *pUAVInitialCounts
  ) {
    auto &binding_set = Stage == PipelineStage::Compute ? state_.ComputeStageUAV.UAVs : state_.OutputMerger.UAVs;

    // std::erase_if(state_.ComputeStageUAV.UAVs, [&](const auto &item) -> bool
    // {
    //   auto &[slot, bound_uav] = item;
    //   if (slot < StartSlot || slot >= (StartSlot + NumUAVs))
    //     return false;
    //   for (auto i = 0u; i < NumUAVs; i++) {
    //     if (auto uav = static_cast<MTLD3D11UnorderedAccessView *>(
    //             ppUnorderedAccessViews[i])) {
    //       // if (bound_uav.View->GetViewRange().CheckOverlap(
    //       //         uav->GetViewRange())) {
    //       //   return true;
    //       // }
    //     }
    //   }
    //   return false;
    // });

    for (unsigned slot = StartSlot; slot < StartSlot + NumUAVs; slot++) {
      auto pUAV = static_cast<D3D11UnorderedAccessView*>(ppUnorderedAccessViews[slot - StartSlot]);
      auto InitialCount = pUAVInitialCounts ? pUAVInitialCounts[slot - StartSlot] : ~0u;
      if (pUAV) {
        bool replaced = false;
        auto &entry = binding_set.bind(slot, {pUAV}, replaced);
        if (InitialCount != ~0u) {
          UpdateUAVCounter(pUAV, InitialCount);
        }
        if (!replaced) {
          continue;
        }
        entry.View = pUAV;
        if (auto buffer = pUAV->buffer()) {
          EmitST([=, buffer = std::move(buffer), viewId = pUAV->viewId(), counter = pUAV->counter(),
                slice = pUAV->bufferSlice()](ArgumentEncodingContext &enc) mutable {
            enc.bindOutputBuffer<Stage>(slot, forward_rc(buffer), viewId, forward_rc(counter), slice);
          });
        } else {
          EmitST([=, texture = pUAV->texture(), viewId = pUAV->viewId()](ArgumentEncodingContext &enc) mutable {
            enc.bindOutputTexture<Stage>(slot, forward_rc(texture), viewId);
          });
        }
        // FIXME: resolve srv hazard: unbind any cs srv that share the resource
        // std::erase_if(state_.ShaderStages[5].SRVs,
        //               [&](const auto &item) -> bool {
        //                 // auto &[slot, bound_srv] = item;
        //                 // if srv conflict with uav, return true
        //                 return false;
        //               });
      } else {
        if (binding_set.unbind(slot)) {
          EmitST([=](ArgumentEncodingContext &enc) { enc.bindOutputBuffer<Stage>(slot, {}, 0, {}, {}); });
        }
      }
    }
  }

#pragma endregion

#pragma region InputAssembler
  void
  SetVertexBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppVertexBuffers, const UINT *pStrides, const UINT *pOffsets
  ) {
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
          EmitST([=, offset = entry.Offset,
                stride = entry.Stride](ArgumentEncodingContext &enc) mutable {
            enc.bindVertexBufferOffset(slot, offset, stride);
          });
          continue;
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
        entry.Buffer = reinterpret_cast<D3D11ResourceCommon *>(pVertexBuffer);
          EmitST([=, buffer = entry.Buffer->buffer(), offset = entry.Offset,
                stride = entry.Stride](ArgumentEncodingContext &enc) mutable {
            enc.bindVertexBuffer(slot, offset, stride, forward_rc(buffer));
        });
      } else {
        if (VertexBuffers.unbind(slot)) {
          EmitST([=](ArgumentEncodingContext& enc) {
            enc.bindVertexBuffer(slot, 0, 0, {});
          });
        }
      }
    }
  }

  void
  GetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppVertexBuffers, UINT *pStrides, UINT *pOffsets) {
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
        VertexBuffer.Buffer->QueryInterface(IID_PPV_ARGS(&ppVertexBuffers[i]));
      }
      if (pStrides != NULL)
        pStrides[i] = VertexBuffer.Stride;
      if (pOffsets != NULL)
        pOffsets[i] = VertexBuffer.Offset;
    }
  }
#pragma endregion

#pragma region CopyResource

  void
  CopyBuffer(
      ID3D11Buffer *pDstResource, uint32_t DstSubresource, uint32_t DstX, uint32_t DstY, uint32_t DstZ,
      ID3D11Buffer *pSrcResource, uint32_t SrcSubresource, const D3D11_BOX *pSrcBox
  ) {
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
    if (auto staging_dst = GetStagingResource(pDstResource, DstSubresource)) {
      if (auto staging_src = GetStagingResource(pSrcResource, SrcSubresource)) {
        UNIMPLEMENTED("copy buffer between staging");
      } else if (auto src = reinterpret_cast<D3D11ResourceCommon *>(pSrcResource)) {
        // copy from device to staging
        UseCopyDestination(staging_dst);
        SwitchToBlitEncoder(CommandBufferState::ReadbackBlitEncoderActive);
        EmitOP([src_ = src->buffer(), dst = std::move(staging_dst), DstX, SrcBox](ArgumentEncodingContext &enc) {
          auto src = enc.access(src_, SrcBox.left, SrcBox.right - SrcBox.left, DXMT_ENCODER_RESOURCE_ACESS_READ);
          enc.encodeBlitCommand([=, &SrcBox, dst = dst->current](BlitCommandContext &ctx) {
            ctx.encoder->copyFromBuffer(src, SrcBox.left, dst, DstX, SrcBox.right - SrcBox.left);
          });
        });
        promote_flush = true;
      } else {
        UNIMPLEMENTED("todo");
      }
    } else if (auto dst = reinterpret_cast<D3D11ResourceCommon *>(pDstResource)) {
      if (auto staging_src = GetStagingResource(pSrcResource, SrcSubresource)) {
        UseCopySource(staging_src);
        SwitchToBlitEncoder(CommandBufferState::UpdateBlitEncoderActive);
        EmitOP([dst_ = dst->buffer(), src = std::move(staging_src), DstX, SrcBox](ArgumentEncodingContext &enc) {
          auto dst = enc.access(dst_, DstX, SrcBox.right - SrcBox.left, DXMT_ENCODER_RESOURCE_ACESS_WRITE);
          enc.encodeBlitCommand([=, &SrcBox, src = src->current](BlitCommandContext &ctx) {
            ctx.encoder->copyFromBuffer(src, SrcBox.left, dst, DstX, SrcBox.right - SrcBox.left);
          });
        });
      } else if (auto src = reinterpret_cast<D3D11ResourceCommon *>(pSrcResource)) {
        // on-device copy
        SwitchToBlitEncoder(CommandBufferState::BlitEncoderActive);
        EmitOP([dst_ = dst->buffer(), src_ = src->buffer(), DstX,
                               SrcBox](ArgumentEncodingContext& enc) {
          auto src = enc.access(src_, SrcBox.left, SrcBox.right - SrcBox.left, DXMT_ENCODER_RESOURCE_ACESS_READ);
          auto dst = enc.access(dst_, DstX, SrcBox.right - SrcBox.left, DXMT_ENCODER_RESOURCE_ACESS_WRITE);
          enc.encodeBlitCommand([&, src, dst](BlitCommandContext &ctx) {
            ctx.encoder->copyFromBuffer(src, SrcBox.left, dst, DstX, SrcBox.right - SrcBox.left);
          });
        });
      } else {
        UNIMPLEMENTED("todo");
      }
    } else {
      UNIMPLEMENTED("todo");
    }
  }

  void
  CopyTexture(TextureCopyCommand &&cmd) {
    if (cmd.Invalid)
      return;
    if ((cmd.SrcFormat.Flag & MTL_DXGI_FORMAT_BC) != (cmd.DstFormat.Flag & MTL_DXGI_FORMAT_BC)) {
      if (cmd.SrcFormat.Flag & MTL_DXGI_FORMAT_BC) {
        return CopyTextureFromCompressed(std::move(cmd));
      } else {
        return CopyTextureToCompressed(std::move(cmd));
      }
    }
    return CopyTextureBitcast(std::move(cmd));
  }

  void
  CopyTextureBitcast(TextureCopyCommand &&cmd) {
    if (auto staging_dst = GetStagingResource(cmd.pDst, cmd.DstSubresource)) {
      if (auto staging_src = GetStagingResource(cmd.pSrc, cmd.SrcSubresource)) {
        UNIMPLEMENTED("copy between staging");
      } else if (auto src = GetTexture(cmd.pSrc)) {
        UseCopyDestination(staging_dst);
        SwitchToBlitEncoder(CommandBufferState::ReadbackBlitEncoderActive);
        EmitOP([src_ = std::move(src), dst = std::move(staging_dst),
              cmd = std::move(cmd)](ArgumentEncodingContext &enc) {
          auto src = enc.access(src_, cmd.Src.MipLevel, cmd.Src.ArraySlice, DXMT_ENCODER_RESOURCE_ACESS_READ);
          auto offset = cmd.DstOrigin.z * dst->bytesPerImage + cmd.DstOrigin.y * dst->bytesPerRow +
                        cmd.DstOrigin.x * cmd.DstFormat.BytesPerTexel;
          enc.encodeBlitCommand([=, &cmd, dst = dst->current, bpr = dst->bytesPerRow,
                                 bpi = dst->bytesPerImage](BlitCommandContext &ctx) {
            ctx.encoder->copyFromTexture(
                src, cmd.Src.ArraySlice, cmd.Src.MipLevel, cmd.SrcOrigin, cmd.SrcSize, dst, offset, bpr, bpi
            );
          });
        });
        promote_flush = true;
      } else {
        UNREACHABLE
      }
    } else if (auto dst = GetTexture(cmd.pDst)) {
      if (auto staging_src = GetStagingResource(cmd.pSrc, cmd.SrcSubresource)) {
        // copy from staging to default
        UseCopySource(staging_src);
        SwitchToBlitEncoder(CommandBufferState::UpdateBlitEncoderActive);
        EmitOP([dst_ = std::move(dst), src =std::move(staging_src),
              cmd = std::move(cmd)](ArgumentEncodingContext &enc) {
          auto dst = enc.access(dst_, cmd.Dst.MipLevel, cmd.Dst.ArraySlice, DXMT_ENCODER_RESOURCE_ACESS_WRITE);
          uint32_t offset;
          if (cmd.SrcFormat.Flag & MTL_DXGI_FORMAT_BC) {
            offset = cmd.SrcOrigin.z * src->bytesPerImage + (cmd.SrcOrigin.y >> 2) * src->bytesPerRow +
                     (cmd.SrcOrigin.x >> 2) * cmd.SrcFormat.BytesPerTexel;
          } else {
            offset = cmd.SrcOrigin.z * src->bytesPerImage + cmd.SrcOrigin.y * src->bytesPerRow +
                     cmd.SrcOrigin.x * cmd.SrcFormat.BytesPerTexel;
          }
          enc.encodeBlitCommand([=, &cmd, src = src->current, bpr = src->bytesPerRow,
                                 bpi = src->bytesPerImage](BlitCommandContext &ctx) {
            ctx.encoder->copyFromBuffer(
                src, offset, bpr, bpi, cmd.SrcSize, dst, cmd.Dst.ArraySlice, cmd.Dst.MipLevel, cmd.DstOrigin
            );
          });
        });
      } else if (auto src = GetTexture(cmd.pSrc)) {
        // on-device copy
        SwitchToBlitEncoder(CommandBufferState::BlitEncoderActive);
        EmitOP([dst_ = std::move(dst), src_ = std::move(src), cmd = std::move(cmd)](ArgumentEncodingContext &enc) {
          auto src_format = src_->pixelFormat();
          auto dst_format = dst_->pixelFormat();
          auto src = enc.access(src_, cmd.Src.MipLevel, cmd.Src.ArraySlice, DXMT_ENCODER_RESOURCE_ACESS_READ);
          auto dst = enc.access(dst_, cmd.Dst.MipLevel, cmd.Dst.ArraySlice, DXMT_ENCODER_RESOURCE_ACESS_WRITE);
          if (Forget_sRGB(dst_format) != Forget_sRGB(src_format)) {

            // bitcast, using a temporary buffer
            size_t bytes_per_row, bytes_per_image, bytes_total;
            if (cmd.SrcFormat.Flag & MTL_DXGI_FORMAT_BC) {
              bytes_per_row = (align(cmd.SrcSize.width, 4u) >> 2) * cmd.SrcFormat.BytesPerTexel;
              bytes_per_image = bytes_per_row * (align(cmd.SrcSize.height, 4u) >> 2);
            } else {
              bytes_per_row = cmd.SrcSize.width * cmd.SrcFormat.BytesPerTexel;
              bytes_per_image = bytes_per_row * cmd.SrcSize.height;
            }
            bytes_total = bytes_per_image * cmd.SrcSize.depth;

            auto [buffer, offset] = enc.allocateTempBuffer(bytes_total, 16);
            enc.encodeBlitCommand([=, &cmd = cmd](BlitCommandContext &ctx) {
              ctx.encoder->copyFromTexture(
                  src, cmd.Src.ArraySlice, cmd.Src.MipLevel, cmd.SrcOrigin, cmd.SrcSize, buffer, offset, bytes_per_row,
                  bytes_per_image
              );
              ctx.encoder->copyFromBuffer(
                  buffer, offset, bytes_per_row, bytes_per_image, cmd.SrcSize, dst, cmd.Dst.ArraySlice,
                  cmd.Dst.MipLevel, cmd.DstOrigin
              );
            });
            return;
          }
          enc.encodeBlitCommand([=, &cmd](BlitCommandContext &ctx) {
            ctx.encoder->copyFromTexture(
                src, cmd.Src.ArraySlice, cmd.Src.MipLevel, cmd.SrcOrigin, cmd.SrcSize, dst, cmd.Dst.ArraySlice,
                cmd.Dst.MipLevel, cmd.DstOrigin
            );
          });
        });
      } else {
        UNREACHABLE
      }
    } else {
      UNREACHABLE
    }
  }

  void
  CopyTextureFromCompressed(TextureCopyCommand &&cmd) {
    if (auto staging_dst = GetStagingResource(cmd.pDst, cmd.DstSubresource)) {
      IMPLEMENT_ME
    } else if (auto dst = GetTexture(cmd.pDst)) {
      if (auto staging_src = GetStagingResource(cmd.pSrc, cmd.SrcSubresource)) {
        // copy from staging to default
        UNIMPLEMENTED("copy from compressed staging to default");
      } else if (auto src = GetTexture(cmd.pSrc)) {
        // on-device copy
        SwitchToBlitEncoder(CommandBufferState::BlitEncoderActive);
        EmitOP([dst_ = std::move(dst), src_ = std::move(src), cmd = std::move(cmd)](ArgumentEncodingContext &enc) {
          auto src = enc.access(src_, cmd.Src.MipLevel, cmd.Src.ArraySlice, DXMT_ENCODER_RESOURCE_ACESS_READ);
          auto dst = enc.access(dst_, cmd.Dst.MipLevel, cmd.Dst.ArraySlice, DXMT_ENCODER_RESOURCE_ACESS_WRITE);
          auto block_w = (align(cmd.SrcSize.width, 4u) >> 2);
          auto block_h = (align(cmd.SrcSize.height, 4u) >> 2);
          auto bytes_per_row = block_w * cmd.SrcFormat.BytesPerTexel;
          auto bytes_per_image = bytes_per_row * block_h;
          auto [buffer, offset] = enc.allocateTempBuffer(bytes_per_image * cmd.SrcSize.depth, 16);
          enc.encodeBlitCommand([=, &cmd](BlitCommandContext &ctx) {
            ctx.encoder->copyFromTexture(
                src, cmd.Src.ArraySlice, cmd.Src.MipLevel, cmd.SrcOrigin, cmd.SrcSize, buffer, offset, bytes_per_row,
                bytes_per_image
            );
            ctx.encoder->copyFromBuffer(
                buffer, offset, bytes_per_row, bytes_per_image, MTL::Size::Make(block_w, block_h, cmd.SrcSize.depth),
                dst, cmd.Dst.ArraySlice, cmd.Dst.MipLevel, cmd.DstOrigin
            );
          });
        });
      } else {
        UNREACHABLE
      }
    } else {
      UNREACHABLE
    }
  }

  void
  CopyTextureToCompressed(TextureCopyCommand &&cmd) {
    if (auto staging_dst = GetStagingResource(cmd.pDst, cmd.DstSubresource)) {
      IMPLEMENT_ME
    } else if (auto dst = GetTexture(cmd.pDst)) {
      if (auto staging_src = GetStagingResource(cmd.pSrc, cmd.SrcSubresource)) {
        // copy from staging to default
        UNIMPLEMENTED("copy from staging to compressed default");
      } else if (auto src = GetTexture(cmd.pSrc)) {
        // on-device copy
        SwitchToBlitEncoder(CommandBufferState::BlitEncoderActive);
        EmitOP([dst_ = std::move(dst), src_ = std::move(src), cmd = std::move(cmd)](ArgumentEncodingContext &enc) {
          auto src = enc.access(src_, cmd.Src.MipLevel, cmd.Src.ArraySlice, DXMT_ENCODER_RESOURCE_ACESS_READ);
          auto dst = enc.access(dst_, cmd.Dst.MipLevel, cmd.Dst.ArraySlice, DXMT_ENCODER_RESOURCE_ACESS_WRITE);
          auto bytes_per_row = cmd.SrcSize.width * cmd.SrcFormat.BytesPerTexel;
          auto bytes_per_image = cmd.SrcSize.height * bytes_per_row;
          auto [buffer, offset] = enc.allocateTempBuffer(bytes_per_image * cmd.SrcSize.depth, 16);
          auto clamped_src_width = std::min(
              cmd.SrcSize.width << 2, std::max<uint32_t>(dst->width() >> cmd.Dst.MipLevel, 1u) - cmd.DstOrigin.x
          );
          auto clamped_src_height = std::min(
              cmd.SrcSize.height << 2, std::max<uint32_t>(dst->height() >> cmd.Dst.MipLevel, 1u) - cmd.DstOrigin.y
          );
          enc.encodeBlitCommand([=, &cmd](BlitCommandContext &ctx) {
            ctx.encoder->copyFromTexture(
                src, cmd.Src.ArraySlice, cmd.Src.MipLevel, cmd.SrcOrigin, cmd.SrcSize, buffer, offset, bytes_per_row,
                bytes_per_image
            );
            ctx.encoder->copyFromBuffer(
                buffer, offset, bytes_per_row, bytes_per_image,
                MTL::Size::Make(clamped_src_width, clamped_src_height, cmd.SrcSize.depth), dst, cmd.Dst.ArraySlice,
                cmd.Dst.MipLevel, cmd.DstOrigin
            );
          });
        });
      } else {
        UNREACHABLE
      }
    } else {
      UNREACHABLE
    }
  }

  void
  UpdateTexture(
      TextureUpdateCommand &&cmd, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags
  ) {
    if (cmd.Invalid)
      return;

    if (auto dst = GetTexture(cmd.pDst)) {
      auto bytes_per_depth_slice = cmd.EffectiveRows * cmd.EffectiveBytesPerRow;
      auto [ptr, staging_buffer, offset] = AllocateStagingBuffer(bytes_per_depth_slice * cmd.DstRegion.size.depth, 16);
      if (cmd.EffectiveBytesPerRow == SrcRowPitch) {
        for (unsigned depthSlice = 0; depthSlice < cmd.DstRegion.size.depth; depthSlice++) {
          char *dst = ((char *)ptr) + depthSlice * bytes_per_depth_slice;
          const char *src = ((const char *)pSrcData) + depthSlice * SrcDepthPitch;
          memcpy(dst, src, bytes_per_depth_slice);
        }
      } else {
        for (unsigned depthSlice = 0; depthSlice < cmd.DstRegion.size.depth; depthSlice++) {
          for (unsigned row = 0; row < cmd.EffectiveRows; row++) {
            char *dst = ((char *)ptr) + row * cmd.EffectiveBytesPerRow + depthSlice * bytes_per_depth_slice;
            const char *src = ((const char *)pSrcData) + row * SrcRowPitch + depthSlice * SrcDepthPitch;
            memcpy(dst, src, cmd.EffectiveBytesPerRow);
          }
        }
      }
      SwitchToBlitEncoder(CommandBufferState::UpdateBlitEncoderActive);
      EmitOP([staging_buffer, offset, dst = std::move(dst), cmd = std::move(cmd),
            bytes_per_depth_slice](ArgumentEncodingContext &enc) {
        auto texture = enc.access(dst, cmd.Dst.MipLevel, cmd.Dst.ArraySlice, DXMT_ENCODER_RESOURCE_ACESS_WRITE);
        enc.encodeBlitCommand([&, texture](BlitCommandContext &ctx) {
          ctx.encoder->copyFromBuffer(
              staging_buffer, offset, cmd.EffectiveBytesPerRow, bytes_per_depth_slice, cmd.DstRegion.size,
              texture, cmd.Dst.ArraySlice, cmd.Dst.MipLevel, cmd.DstRegion.origin
          );
        });
      });
    } else if (auto staging_dst = GetStagingResource(cmd.pDst, cmd.DstSubresource)) {
      // staging: ...
      UNIMPLEMENTED("update staging texture");
    } else {
      UNIMPLEMENTED("unknown texture");
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
  bool
  InvalidateCurrentPass(bool defer_commit = false) {
    switch (cmdbuf_state) {
    case CommandBufferState::Idle:
      break;
    case CommandBufferState::RenderEncoderActive:
    case CommandBufferState::RenderPipelineReady:
    case CommandBufferState::TessellationRenderPipelineReady: {
      // EmitCommand([](CommandChunk::context &ctx) {
      //   ctx.render_encoder->endEncoding();
      //   ctx.render_encoder = nullptr;
      //   ctx.dsv_planar_flags = 0;
      // });
      // vro_state.endEncoder();
      EmitST([](ArgumentEncodingContext& enc) {
        enc.endPass();
      });
      break;
    }
    case CommandBufferState::ComputeEncoderActive:
    case CommandBufferState::ComputePipelineReady:
      EmitST([](ArgumentEncodingContext& enc) {
        enc.endPass();
      });
      break;
    case CommandBufferState::UpdateBlitEncoderActive:
    case CommandBufferState::ReadbackBlitEncoderActive:
    case CommandBufferState::BlitEncoderActive:
      EmitST([](ArgumentEncodingContext& enc) {
        enc.endPass();
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
  void
  InvalidateRenderPipeline() {
    if (cmdbuf_state != CommandBufferState::RenderPipelineReady &&
        cmdbuf_state != CommandBufferState::TessellationRenderPipelineReady)
      return;
    previous_render_pipeline_state = cmdbuf_state;
    cmdbuf_state = CommandBufferState::RenderEncoderActive;
  }

  /**
  Compute pipeline can be invalidate by reasons:
  - shader program changes
  TOOD: maybe we don't need this, since compute shader can be pre-compiled
  */
  void
  InvalidateComputePipeline() {
    if (cmdbuf_state != CommandBufferState::ComputePipelineReady)
      return;
    cmdbuf_state = CommandBufferState::ComputeEncoderActive;
  }

  void
  ClearRenderTargetView(IMTLD3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4]) {
    InvalidateCurrentPass();
    auto clear_color = MTL::ClearColor::Make(ColorRGBA[0], ColorRGBA[1], ColorRGBA[2], ColorRGBA[3]);
    auto &props = pRenderTargetView->GetAttachmentDesc();

    EmitOP([texture = pRenderTargetView->__texture(), view = pRenderTargetView->__viewId(),
          clear_color = std::move(clear_color), array_length = props.RenderTargetArrayLength](ArgumentEncodingContext &enc) mutable {
      enc.clearColor(forward_rc(texture), view, array_length, clear_color);
    });
  }

  void
  ClearDepthStencilView(IMTLD3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil) {
    if (ClearFlags == 0)
      return;
    InvalidateCurrentPass();
    auto &props = pDepthStencilView->GetAttachmentDesc();

    EmitOP([texture = pDepthStencilView->__texture(), view = pDepthStencilView->__viewId(),
          renamable = pDepthStencilView->__renamable(), array_length = props.RenderTargetArrayLength,
          ClearFlags = ClearFlags & 0b11, Depth, Stencil](ArgumentEncodingContext &enc) mutable {
      if (renamable.ptr() && ClearFlags == DepthStencilPlanarFlags(texture->pixelFormat())) {
        texture->rename(renamable->getNext(enc.currentSeqId()));
      }
      enc.clearDepthStencil(forward_rc(texture), view, array_length, ClearFlags, Depth, Stencil);
    });
  }

  void
  ResolveSubresource(D3D11ResourceCommon *pSrc, UINT SrcSlice, D3D11ResourceCommon *pDst, UINT DstLevel, UINT DstSlice) {
    InvalidateCurrentPass();
    EmitOP([src = pSrc->texture(), dst = pDst->texture(), SrcSlice, DstSlice, DstLevel](ArgumentEncodingContext &enc
         ) mutable { enc.resolveTexture(forward_rc(src), SrcSlice, forward_rc(dst), DstSlice, DstLevel); });
  }

  /**
  Switch to render encoder and set all states (expect for pipeline state)
  */
  bool
  SwitchToRenderEncoder() {
    if (cmdbuf_state == CommandBufferState::RenderPipelineReady)
      return true;
    if (cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady)
      return true;
    if (cmdbuf_state == CommandBufferState::RenderEncoderActive)
      return true;
    InvalidateCurrentPass();

    // set dirty state
    state_.ShaderStages[PipelineStage::Vertex].ConstantBuffers.set_dirty();
    state_.ShaderStages[PipelineStage::Vertex].Samplers.set_dirty();
    state_.ShaderStages[PipelineStage::Vertex].SRVs.set_dirty();
    state_.ShaderStages[PipelineStage::Pixel].ConstantBuffers.set_dirty();
    state_.ShaderStages[PipelineStage::Pixel].Samplers.set_dirty();
    state_.ShaderStages[PipelineStage::Pixel].SRVs.set_dirty();
    state_.ShaderStages[PipelineStage::Hull].ConstantBuffers.set_dirty();
    state_.ShaderStages[PipelineStage::Hull].Samplers.set_dirty();
    state_.ShaderStages[PipelineStage::Hull].SRVs.set_dirty();
    state_.ShaderStages[PipelineStage::Domain].ConstantBuffers.set_dirty();
    state_.ShaderStages[PipelineStage::Domain].Samplers.set_dirty();
    state_.ShaderStages[PipelineStage::Domain].SRVs.set_dirty();
    state_.InputAssembler.VertexBuffers.set_dirty();
    dirty_state.set(
        DirtyState::BlendFactorAndStencilRef, DirtyState::RasterizerState, DirtyState::DepthStencilState,
        DirtyState::Viewport, DirtyState::Scissors
    );

    // should assume: render target is properly set
    {
      /* Setup RenderCommandEncoder */
      struct RENDER_TARGET_STATE {
        Rc<Texture> Texture;
        unsigned viewId;
        UINT RenderTargetIndex;
        UINT DepthPlane;
        MTL::PixelFormat PixelFormat = MTL::PixelFormatInvalid;
        MTL::LoadAction LoadAction{MTL::LoadActionLoad};
      };

      uint32_t effective_render_target = 0;
      // FIXME: is this value always valid?
      uint32_t render_target_array = state_.OutputMerger.ArrayLength;
      auto rtvs = AllocateCommandData<RENDER_TARGET_STATE>(state_.OutputMerger.NumRTVs);
      for (unsigned i = 0; i < state_.OutputMerger.NumRTVs; i++) {
        auto &rtv = state_.OutputMerger.RTVs[i];
        if (rtv) {
          auto props = rtv->GetAttachmentDesc();
          rtvs[i] = {rtv->__texture(), rtv->__viewId(), i, props.DepthPlane, rtv->GetPixelFormat()};
          D3D11_ASSERT(rtv->GetPixelFormat() != MTL::PixelFormatInvalid);
          effective_render_target++;
        } else {
          rtvs[i].RenderTargetIndex = i;
        }
      }
      struct DEPTH_STENCIL_STATE {
        Rc<Texture> Texture{};
        unsigned viewId{};
        MTL::PixelFormat PixelFormat = MTL::PixelFormatInvalid;
        MTL::LoadAction DepthLoadAction{MTL::LoadActionLoad};
        MTL::LoadAction StencilLoadAction{MTL::LoadActionLoad};
      };
      // auto &dsv = state_.OutputMerger.DSV;
      DEPTH_STENCIL_STATE dsv_info;
      uint32_t uav_only_render_target_width = 0;
      uint32_t uav_only_render_target_height = 0;
      bool uav_only = false;
      uint32_t uav_only_sample_count = 0;
      if (state_.OutputMerger.DSV) {
        dsv_info.Texture = state_.OutputMerger.DSV->__texture();
        dsv_info.viewId = state_.OutputMerger.DSV->__viewId();
        dsv_info.PixelFormat = state_.OutputMerger.DSV->GetPixelFormat();
      } else if (effective_render_target == 0) {
        if (!state_.OutputMerger.UAVs.any_bound()) {
          ERR("No rendering attachment or uav is bounded");
          return false;
        }
        D3D11_ASSERT(state_.Rasterizer.NumViewports);
        IMTLD3D11RasterizerState *state =
            state_.Rasterizer.RasterizerState ? state_.Rasterizer.RasterizerState : default_rasterizer_state;
        auto &viewport = state_.Rasterizer.viewports[0];
        uav_only_render_target_width = viewport.Width;
        uav_only_render_target_height = viewport.Height;
        uav_only_sample_count = state->UAVOnlySampleCount();
        if (!(uav_only_render_target_width && uav_only_render_target_height)) {
          ERR("uav only rendering is enabled but viewport is empty");
          return false;
        }
        uav_only = true;
      }

      EmitST([rtvs = std::move(rtvs), dsv = std::move(dsv_info), effective_render_target, uav_only,
            uav_only_render_target_height, uav_only_render_target_width, uav_only_sample_count,
            render_target_array](ArgumentEncodingContext &ctx) {
        auto pool = transfer(NS::AutoreleasePool::alloc()->init());
        auto renderPassDescriptor = transfer(MTL::RenderPassDescriptor::alloc()->init());
        for (auto &rtv : rtvs.span()) {
          if (rtv.PixelFormat == MTL::PixelFormatInvalid) {
            continue;
          }
          auto colorAttachment = renderPassDescriptor->colorAttachments()->object(rtv.RenderTargetIndex);
          colorAttachment->setTexture(rtv.Texture->view(rtv.viewId));
          colorAttachment->setDepthPlane(rtv.DepthPlane);
          colorAttachment->setLoadAction(rtv.LoadAction);
          colorAttachment->setStoreAction(MTL::StoreActionStore);
        };
        uint32_t dsv_planar_flags = 0;

        if (dsv.Texture.ptr()) {
          dsv_planar_flags = DepthStencilPlanarFlags(dsv.PixelFormat);
          MTL::Texture *texture = dsv.Texture->view(dsv.viewId);
          // TODO: ...should know more about store behavior (e.g. DiscardView)
          if (dsv_planar_flags & 1) {
            auto depthAttachment = renderPassDescriptor->depthAttachment();
            depthAttachment->setTexture(texture);
            depthAttachment->setLoadAction(dsv.DepthLoadAction);
            depthAttachment->setStoreAction(MTL::StoreActionStore);
          }

          if (dsv_planar_flags & 2) {
            auto stencilAttachment = renderPassDescriptor->stencilAttachment();
            stencilAttachment->setTexture(texture);
            stencilAttachment->setLoadAction(dsv.StencilLoadAction);
            stencilAttachment->setStoreAction(MTL::StoreActionStore);
          }
        }
        if (effective_render_target == 0) {
          if (uav_only) {
            renderPassDescriptor->setRenderTargetHeight(uav_only_render_target_height);
            renderPassDescriptor->setRenderTargetWidth(uav_only_render_target_width);
            renderPassDescriptor->setDefaultRasterSampleCount(uav_only_sample_count);
          } else {
            D3D11_ASSERT(dsv_planar_flags);
            auto dsv_tex = dsv.Texture->view(dsv.viewId);
            renderPassDescriptor->setRenderTargetHeight(dsv_tex->height());
            renderPassDescriptor->setRenderTargetWidth(dsv_tex->width());
          }
        }

        renderPassDescriptor->setRenderTargetArrayLength(render_target_array);

        ctx.startRenderPass(std::move(renderPassDescriptor), dsv_planar_flags, rtvs.size());

        for (auto &rtv : rtvs.span()) {
          if (rtv.PixelFormat == MTL::PixelFormatInvalid) {
            continue;
          }
          ctx.access(rtv.Texture, rtv.viewId, DXMT_ENCODER_RESOURCE_ACESS_READ | DXMT_ENCODER_RESOURCE_ACESS_WRITE);
        };

        if (dsv.Texture.ptr()) {
          ctx.access(dsv.Texture, dsv.viewId, DXMT_ENCODER_RESOURCE_ACESS_READ | DXMT_ENCODER_RESOURCE_ACESS_WRITE);
        }
      });
    }

    cmdbuf_state = CommandBufferState::RenderEncoderActive;
    previous_render_pipeline_state = cmdbuf_state;
    return true;
  }

  /**
  Switch to blit encoder
  */
  void
  SwitchToBlitEncoder(CommandBufferState BlitKind) {
    if (cmdbuf_state == BlitKind)
      return;

    /**
    Don't flush if it's a readback/upload blit pass
     */
    InvalidateCurrentPass(BlitKind != CommandBufferState::BlitEncoderActive);

    EmitST([](ArgumentEncodingContext &enc) { enc.startBlitPass(); });

    cmdbuf_state = BlitKind;
  }

  /**
  Switch to compute encoder and set all states (expect for pipeline state)
  */
  void
  SwitchToComputeEncoder() {
    if (cmdbuf_state == CommandBufferState::ComputePipelineReady)
      return;
    if (cmdbuf_state == CommandBufferState::ComputeEncoderActive)
      return;
    InvalidateCurrentPass();

    // set dirty state
    state_.ShaderStages[PipelineStage::Compute].ConstantBuffers.set_dirty();
    state_.ShaderStages[PipelineStage::Compute].Samplers.set_dirty();
    state_.ShaderStages[PipelineStage::Compute].SRVs.set_dirty();

    EmitST([](ArgumentEncodingContext &enc) { enc.startComputePass(); });

    cmdbuf_state = CommandBufferState::ComputeEncoderActive;
  }

  template <PipelineStage Type>
  ManagedShader
  GetManagedShader() {
    auto ptr = state_.ShaderStages[Type].Shader.ptr();
    return ptr ? ptr->GetManagedShader() : nullptr;
  };

  template <bool IndexedDraw>
  void
  InitializeGraphicsPipelineDesc(MTL_GRAPHICS_PIPELINE_DESC &Desc) {
    // TODO: reduce branching
    auto PS = GetManagedShader<PipelineStage::Pixel>();
    auto GS = GetManagedShader<PipelineStage::Geometry>();
    Desc.VertexShader = GetManagedShader<PipelineStage::Vertex>();
    Desc.PixelShader = PS;
    // FIXME: ensure valid state: hull and domain shader none or both are bound
    Desc.HullShader = GetManagedShader<PipelineStage::Hull>();
    Desc.DomainShader = GetManagedShader<PipelineStage::Domain>();
    if (state_.InputAssembler.InputLayout) {
      Desc.InputLayout = state_.InputAssembler.InputLayout->GetManagedInputLayout();
    } else {
      Desc.InputLayout = nullptr;
    }
    auto so_layout = com_cast<IMTLD3D11StreamOutputLayout>(state_.ShaderStages[PipelineStage::Geometry].Shader.ptr());
    if (unlikely(so_layout)) {
      Desc.SOLayout = so_layout.ptr();
    } else {
      Desc.SOLayout = nullptr;
    }
    Desc.NumColorAttachments = state_.OutputMerger.NumRTVs;
    for (unsigned i = 0; i < ARRAYSIZE(state_.OutputMerger.RTVs); i++) {
      auto &rtv = state_.OutputMerger.RTVs[i];
      if (rtv && i < Desc.NumColorAttachments) {
        Desc.ColorAttachmentFormats[i] = state_.OutputMerger.RTVs[i]->GetPixelFormat();
      } else {
        Desc.ColorAttachmentFormats[i] = MTL::PixelFormatInvalid;
      }
    }
    Desc.BlendState = state_.OutputMerger.BlendState ? state_.OutputMerger.BlendState : default_blend_state;
    Desc.DepthStencilFormat =
        state_.OutputMerger.DSV ? state_.OutputMerger.DSV->GetPixelFormat() : MTL::PixelFormatInvalid;
    Desc.TopologyClass = to_metal_primitive_topology(state_.InputAssembler.Topology);
    bool ds_enabled =
        (state_.OutputMerger.DepthStencilState ? state_.OutputMerger.DepthStencilState : default_depth_stencil_state)
            ->IsEnabled();
    // FIXME: corner case: DSV is not bound or missing planar
    Desc.RasterizationEnabled = PS || ds_enabled;
    Desc.SampleMask = state_.OutputMerger.SampleMask;
    Desc.GSPassthrough = GS ? GS->reflection().GeometryShader.GSPassThrough : ~0u;
    if constexpr (IndexedDraw) {
      Desc.IndexBufferFormat = state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT
                                   ? SM50_INDEX_BUFFER_FORMAT_UINT32
                                   : SM50_INDEX_BUFFER_FORMAT_UINT16;
    } else {
      Desc.IndexBufferFormat = SM50_INDEX_BUFFER_FORMAT_NONE;
    }
    Desc.SampleCount = state_.OutputMerger.SampleCount;
  }

  template <bool IndexedDraw>
  bool
  FinalizeTessellationRenderPipeline() {
    if (cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady)
      return true;
    auto HS = GetManagedShader<PipelineStage::Hull>();
    if (!HS) {
      return false;
    }
    switch (HS->reflection().Tessellator.OutputPrimitive) {
    case MTL_TESSELLATOR_OUTPUT_LINE:
      EmitST([](ArgumentEncodingContext &enc) {
        enc.setCompatibilityFlag(FeatureCompatibility::UnsupportedTessellationOutputPrimitive);
      });
      return false;
    case MTL_TESSELLATOR_OUTPUT_POINT:
      EmitST([](ArgumentEncodingContext &enc) {
        enc.setCompatibilityFlag(FeatureCompatibility::UnsupportedTessellationOutputPrimitive);
      });
      return false;
    default:
      break;
    }
    if (!state_.ShaderStages[PipelineStage::Domain].Shader) {
      return false;
    }
    auto GS = GetManagedShader<PipelineStage::Geometry>();
    if (GS && GS->reflection().GeometryShader.GSPassThrough == ~0u) {
      EmitST([](ArgumentEncodingContext &enc) {
        enc.setCompatibilityFlag(FeatureCompatibility::UnsupportedGeometryTessellationDraw);
      });
      return false;
    }

    if (!SwitchToRenderEncoder()) {
      return false;
    }

    Com<IMTLCompiledTessellationPipeline> pipeline;

    MTL_GRAPHICS_PIPELINE_DESC pipelineDesc;
    InitializeGraphicsPipelineDesc<IndexedDraw>(pipelineDesc);

    device->CreateTessellationPipeline(&pipelineDesc, &pipeline);

    EmitST([pso = std::move(pipeline)](ArgumentEncodingContext &enc) {
      auto render_encoder = enc.currentRenderEncoder();
      render_encoder->use_tessellation = 1;
      MTL_COMPILED_TESSELLATION_PIPELINE GraphicsPipeline{};
      pso->GetPipeline(&GraphicsPipeline); // may block
      enc.tess_num_output_control_point_element = GraphicsPipeline.NumControlPointOutputElement;
      enc.tess_num_output_patch_constant_scalar = GraphicsPipeline.NumPatchConstantOutputScalar;
      enc.tess_threads_per_patch = GraphicsPipeline.ThreadsPerPatch;
      if (!(GraphicsPipeline.MeshPipelineState && GraphicsPipeline.RasterizationPipelineState))
        return;
      enc.encodePreTessCommand([pso = GraphicsPipeline.MeshPipelineState](RenderCommandContext &ctx) {
        ctx.encoder->setRenderPipelineState(pso);
      });
      enc.encodeRenderCommand([pso = GraphicsPipeline.RasterizationPipelineState](RenderCommandContext &ctx) {
        ctx.encoder->setRenderPipelineState(pso);
      });
    });

    cmdbuf_state = CommandBufferState::TessellationRenderPipelineReady;

    if (previous_render_pipeline_state != CommandBufferState::TessellationRenderPipelineReady) {
      state_.InputAssembler.VertexBuffers.set_dirty();
      state_.ShaderStages[PipelineStage::Vertex].ConstantBuffers.set_dirty();
      state_.ShaderStages[PipelineStage::Vertex].SRVs.set_dirty();
      state_.ShaderStages[PipelineStage::Vertex].Samplers.set_dirty();
      state_.ShaderStages[PipelineStage::Domain].ConstantBuffers.set_dirty();
      state_.ShaderStages[PipelineStage::Domain].SRVs.set_dirty();
      state_.ShaderStages[PipelineStage::Domain].Samplers.set_dirty();
      previous_render_pipeline_state = CommandBufferState::TessellationRenderPipelineReady;
    }

    return true;
  }

  /**
  Assume we have all things needed to build PSO
  If the current encoder is not a render encoder, switch to it.
  */
  template <bool IndexedDraw>
  bool
  FinalizeCurrentRenderPipeline() {
    if (state_.ShaderStages[PipelineStage::Hull].Shader) {
      return FinalizeTessellationRenderPipeline<IndexedDraw>();
    }
    if (cmdbuf_state == CommandBufferState::RenderPipelineReady)
      return true;
    auto GS = GetManagedShader<PipelineStage::Geometry>();
    if (GS) {
      if (GS->reflection().GeometryShader.GSPassThrough == ~0u) {
        EmitST([](ArgumentEncodingContext &enc) {
          enc.setCompatibilityFlag(FeatureCompatibility::UnsupportedGeometryDraw);
        });
        return false;
      }
    }

    if (!SwitchToRenderEncoder()) {
      return false;
    }

    Com<IMTLCompiledGraphicsPipeline> pipeline;

    MTL_GRAPHICS_PIPELINE_DESC pipelineDesc;
    InitializeGraphicsPipelineDesc<IndexedDraw>(pipelineDesc);

    device->CreateGraphicsPipeline(&pipelineDesc, &pipeline);
    EmitST([pso = std::move(pipeline)](ArgumentEncodingContext& enc) {
      MTL_COMPILED_GRAPHICS_PIPELINE GraphicsPipeline{};
      pso->GetPipeline(&GraphicsPipeline); // may block
      if (!GraphicsPipeline.PipelineState)
        return;
      enc.encodeRenderCommand([pso = GraphicsPipeline.PipelineState](RenderCommandContext& ctx) {
        ctx.encoder->setRenderPipelineState(pso);
      });
    });

    cmdbuf_state = CommandBufferState::RenderPipelineReady;

    if (previous_render_pipeline_state != CommandBufferState::RenderPipelineReady) {
      state_.InputAssembler.VertexBuffers.set_dirty();
      state_.ShaderStages[PipelineStage::Vertex].ConstantBuffers.set_dirty();
      state_.ShaderStages[PipelineStage::Vertex].SRVs.set_dirty();
      state_.ShaderStages[PipelineStage::Vertex].Samplers.set_dirty();
      previous_render_pipeline_state = CommandBufferState::RenderPipelineReady;
    }

    return true;
  }

  template <bool IndexedDraw>
  bool
  PreDraw() {
    if (!FinalizeCurrentRenderPipeline<IndexedDraw>()) {
      return false;
    }
    UpdateVertexBuffer();
    UpdateSOTargets();
    if (dirty_state.any(DirtyState::DepthStencilState)) {
      IMTLD3D11DepthStencilState *state =
          state_.OutputMerger.DepthStencilState ? state_.OutputMerger.DepthStencilState : default_depth_stencil_state;
      EmitST([state, stencil_ref = state_.OutputMerger.StencilRef](ArgumentEncodingContext& enc) {
        enc.encodeRenderCommand([&](RenderCommandContext& ctx) {
          ctx.encoder->setDepthStencilState(state->GetDepthStencilState(ctx.dsv_planar_flags));
          ctx.encoder->setStencilReferenceValue(stencil_ref);
        });
      });
    }
    if (dirty_state.any(DirtyState::RasterizerState)) {
      IMTLD3D11RasterizerState *state =
          state_.Rasterizer.RasterizerState ? state_.Rasterizer.RasterizerState : default_rasterizer_state;
      EmitST([state](ArgumentEncodingContext& enc) {
        enc.encodeRenderCommand([&](RenderCommandContext& ctx) {
          state->SetupRasterizerState(ctx.encoder);
        });
      });
    }
    if (dirty_state.any(DirtyState::BlendFactorAndStencilRef)) {
      EmitST([r = state_.OutputMerger.BlendFactor[0], g = state_.OutputMerger.BlendFactor[1],
            b = state_.OutputMerger.BlendFactor[2], a = state_.OutputMerger.BlendFactor[3],
            stencil_ref = state_.OutputMerger.StencilRef](ArgumentEncodingContext &enc) {
        enc.encodeRenderCommand([&](RenderCommandContext &ctx) {
          ctx.encoder->setBlendColor(r, g, b, a);
          ctx.encoder->setStencilReferenceValue(stencil_ref);
        });
      });
    }
    IMTLD3D11RasterizerState *current_rs =
        state_.Rasterizer.RasterizerState ? state_.Rasterizer.RasterizerState : default_rasterizer_state;
    bool allow_scissor = current_rs->IsScissorEnabled();
    if (dirty_state.any(DirtyState::Viewport)) {
      auto viewports = AllocateCommandData<MTL::Viewport>(state_.Rasterizer.NumViewports);
      for (unsigned i = 0; i < state_.Rasterizer.NumViewports; i++) {
        auto &d3dViewport = state_.Rasterizer.viewports[i];
        viewports[i] = {d3dViewport.TopLeftX, d3dViewport.TopLeftY, d3dViewport.Width,
                        d3dViewport.Height,   d3dViewport.MinDepth, d3dViewport.MaxDepth};
      }
      EmitST([viewports = std::move(viewports)](ArgumentEncodingContext& enc) {
        enc.encodeRenderCommand([&](RenderCommandContext& ctx) {
          ctx.encoder->setViewports(viewports.data(), viewports.size());
        });
      });
    }
    if (dirty_state.any(DirtyState::Scissors)) {
      auto scissors = AllocateCommandData<MTL::ScissorRect>(state_.Rasterizer.NumViewports);
      for (unsigned i = 0; i < state_.Rasterizer.NumViewports; i++) {
        if (allow_scissor) {
          if (i < state_.Rasterizer.NumScissorRects) {
            auto &d3dRect = state_.Rasterizer.scissor_rects[i];
            scissors[i] = {
                (UINT)d3dRect.left, (UINT)d3dRect.top, (UINT)d3dRect.right - d3dRect.left,
                (UINT)d3dRect.bottom - d3dRect.top
            };
          } else {
            scissors[i] = {0, 0, 0, 0};
          }
        } else {
          auto &d3dViewport = state_.Rasterizer.viewports[i];
          scissors[i] = {
              (UINT)d3dViewport.TopLeftX, (UINT)d3dViewport.TopLeftY, (UINT)d3dViewport.Width, (UINT)d3dViewport.Height
          };
        }
      }
      EmitST([scissors = std::move(scissors)](ArgumentEncodingContext& enc) {
        enc.encodeRenderCommand([&](RenderCommandContext& ctx) {
          ctx.encoder->setScissorRects(scissors.data(), scissors.size());
        });
      });
    }
    dirty_state.clrAll();
    if (cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady) {
      return UploadShaderStageResourceBinding<PipelineStage::Vertex, true>() &&
             UploadShaderStageResourceBinding<PipelineStage::Pixel, true>() &&
             UploadShaderStageResourceBinding<PipelineStage::Hull, true>() &&
             UploadShaderStageResourceBinding<PipelineStage::Domain, true>();
    }
    return UploadShaderStageResourceBinding<PipelineStage::Vertex, false>() &&
           UploadShaderStageResourceBinding<PipelineStage::Pixel, false>();
  }

  /**
  Assume we have all things needed to build PSO
  If the current encoder is not a compute encoder, switch to it.
  */
  bool
  FinalizeCurrentComputePipeline() {
    if (cmdbuf_state == CommandBufferState::ComputePipelineReady)
      return true;

    SwitchToComputeEncoder();

    auto CS = GetManagedShader<PipelineStage::Compute>();

    if (!CS) {
      ERR("Shader not found?");
      return false;
    }
    Com<IMTLCompiledComputePipeline> pipeline;
    MTL_COMPUTE_PIPELINE_DESC desc{CS};
    device->CreateComputePipeline(&desc, &pipeline);

    EmitST([pso = std::move(pipeline), tg_size = MTL::Size::Make(
                                         CS->reflection().ThreadgroupSize[0], CS->reflection().ThreadgroupSize[1],
                                         CS->reflection().ThreadgroupSize[2]
                                     )](ArgumentEncodingContext &enc) {
      MTL_COMPILED_COMPUTE_PIPELINE ComputePipeline;
      pso->GetPipeline(&ComputePipeline); // may block
      if (!ComputePipeline.PipelineState)
        return;
      enc.encodeComputeCommand([&, pso = ComputePipeline.PipelineState](ComputeCommandContext &ctx) {
        ctx.encoder->setComputePipelineState(pso);
        ctx.threadgroup_size = tg_size;
      });
    });

    cmdbuf_state = CommandBufferState::ComputePipelineReady;
    return true;
  }

  bool
  PreDispatch() {
    if (!FinalizeCurrentComputePipeline()) {
      return false;
    }
    UploadShaderStageResourceBinding<PipelineStage::Compute, false>();
    return true;
  }

  void
  UpdateSOTargets() {
    if (state_.ShaderStages[PipelineStage::Pixel].Shader) {
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
        EmitST([slot0 = so_slot0.Buffer->buffer()](ArgumentEncodingContext &enc) {
          auto buffer = enc.access(slot0, 0, slot0->length(), DXMT_ENCODER_RESOURCE_ACESS_WRITE);
          enc.encodeRenderCommand([buffer](RenderCommandContext &ctx) {
            ctx.encoder->setVertexBuffer(buffer, 0, 20);
          });
          enc.setCompatibilityFlag(FeatureCompatibility::UnsupportedStreamOutputAppending);
        });
      } else {
        EmitST([slot0 = so_slot0.Buffer->buffer(), offset = so_slot0.Offset](ArgumentEncodingContext &enc) {
          auto buffer = enc.access(slot0, 0, slot0->length(), DXMT_ENCODER_RESOURCE_ACESS_WRITE);
          enc.encodeRenderCommand([buffer, offset](RenderCommandContext &ctx) {
            ctx.encoder->setVertexBuffer(buffer, offset, 20);
          });
        });
      }
    } else {
      EmitST([](ArgumentEncodingContext &enc) {
        enc.encodeRenderCommand([](RenderCommandContext &ctx) { ctx.encoder->setVertexBuffer(nullptr, 0, 20); });
      });
    }
    state_.StreamOutput.Targets.clear_dirty(0);
    if (state_.StreamOutput.Targets.any_dirty()) {
      EmitST([](ArgumentEncodingContext &enc) {
        enc.setCompatibilityFlag(FeatureCompatibility::UnsupportedMultipleStreamOutput);
      });
    }
  }

  template <PipelineStage Stage>
  void RestoreEncodingContextStageState() {
      auto &ShaderStage = state_.ShaderStages[Stage];
      for (const auto &[slot, entry] : ShaderStage.ConstantBuffers) {
        EmitST([=, buffer = entry.Buffer->buffer(), offset = entry.FirstConstant << 4](ArgumentEncodingContext &enc
             ) mutable { enc.bindConstantBuffer<Stage>(slot, offset, forward_rc(buffer)); });
      }
      for (const auto &[slot, entry] : ShaderStage.Samplers) {
        EmitST([=, sampler = entry.Sampler](ArgumentEncodingContext &enc) {
          enc.bindSampler<Stage>(slot, sampler->GetSamplerState(), sampler->GetLODBias());
        });
      }
      for (const auto &[slot, entry] : ShaderStage.SRVs) {
        auto pView = entry.SRV.ptr();
        if (auto buffer = pView->buffer()) {
          EmitST([=, buffer = std::move(buffer), viewId = pView->viewId(),
                slice = pView->bufferSlice()](ArgumentEncodingContext &enc) mutable {
            enc.bindBuffer<Stage>(slot, forward_rc(buffer), viewId, slice);
          });
        } else {
          EmitST([=, texture = pView->texture(), viewId = pView->viewId()](ArgumentEncodingContext &enc) mutable {
            enc.bindTexture<Stage>(slot, forward_rc(texture), viewId);
          });
        }
      }
  }

  void
  RestoreEncodingContextState() {
    for (const auto &[slot, element] : state_.InputAssembler.VertexBuffers) {
      EmitST([=, buffer = element.Buffer->buffer(), offset = element.Offset,
            stride = element.Stride](ArgumentEncodingContext &enc) mutable {
        enc.bindVertexBuffer(slot, offset, stride, forward_rc(buffer));
      });
    }

    if (state_.InputAssembler.IndexBuffer) {
      EmitST([buffer = state_.InputAssembler.IndexBuffer->buffer()](ArgumentEncodingContext &enc) mutable {
        enc.bindIndexBuffer(forward_rc(buffer));
      });
    }

    {
      RestoreEncodingContextStageState<PipelineStage::Vertex>();
      RestoreEncodingContextStageState<PipelineStage::Pixel>();
      RestoreEncodingContextStageState<PipelineStage::Geometry>();
      RestoreEncodingContextStageState<PipelineStage::Hull>();
      RestoreEncodingContextStageState<PipelineStage::Domain>();
      RestoreEncodingContextStageState<PipelineStage::Compute>();
    }

    for (const auto &[slot, entry] : state_.OutputMerger.UAVs) {
      auto pUAV = entry.View.ptr();
      if (auto buffer = pUAV->buffer()) {
        EmitST([=, buffer = std::move(buffer), viewId = pUAV->viewId(), counter = pUAV->counter(),
              slice = pUAV->bufferSlice()](ArgumentEncodingContext &enc) mutable {
          enc.bindOutputBuffer<PipelineStage::Pixel>(slot, forward_rc(buffer), viewId, forward_rc(counter), slice);
        });
      } else {
        EmitST([=, texture = pUAV->texture(), viewId = pUAV->viewId()](ArgumentEncodingContext &enc) mutable {
          enc.bindOutputTexture<PipelineStage::Pixel>(slot, forward_rc(texture), viewId);
        });
      }
    }
    for (const auto &[slot, entry] : state_.OutputMerger.UAVs) {
      auto pUAV = entry.View.ptr();
      if (auto buffer = pUAV->buffer()) {
        EmitST([=, buffer = std::move(buffer), viewId = pUAV->viewId(), counter = pUAV->counter(),
              slice = pUAV->bufferSlice()](ArgumentEncodingContext &enc) mutable {
          enc.bindOutputBuffer<PipelineStage::Compute>(slot, forward_rc(buffer), viewId, forward_rc(counter), slice);
        });
      } else {
        EmitST([=, texture = pUAV->texture(), viewId = pUAV->viewId()](ArgumentEncodingContext &enc) mutable {
          enc.bindOutputTexture<PipelineStage::Compute>(slot, forward_rc(texture), viewId);
        });
      }
    }
  }

  void ResetEncodingContextState() {
    InvalidateCurrentPass(true);
    // TODO: optimize by clearing only bound resource
    EmitST([](ArgumentEncodingContext& enc) {
      enc.clearState();
    });
  }

  void ResetD3D11ContextState() {
    state_ = {};
  }

protected:
  MTLD3D11Device *device;
  CommandBufferState cmdbuf_state = CommandBufferState::Idle;
  CommandBufferState previous_render_pipeline_state = CommandBufferState::Idle;
  ContextInternalState &ctx_state;

  IMTLD3D11RasterizerState *default_rasterizer_state;
  IMTLD3D11DepthStencilState *default_depth_stencil_state;
  IMTLD3D11BlendState *default_blend_state;

public:
  enum class DirtyState {
    DepthStencilState,
    RasterizerState,
    BlendFactorAndStencilRef,
    Viewport,
    Scissors,
  };

  Flags<DirtyState> dirty_state = 0;

#pragma endregion

protected:
  D3D11ContextState state_;
  D3D11UserDefinedAnnotation annotation_;
  MTLD3D11ContextExt<ContextInternalState> ext_;

public:
  MTLD3D11DeviceContextImplBase(MTLD3D11Device *pDevice, ContextInternalState &ctx_state) :
      MTLD3D11DeviceChild(pDevice),
      device(pDevice),
      ctx_state(ctx_state),
      state_(),
      annotation_(this),
      ext_(this) {
    pDevice->CreateRasterizerState2(&kDefaultRasterizerDesc, (ID3D11RasterizerState2 **)&default_rasterizer_state);
    pDevice->CreateBlendState1(&kDefaultBlendDesc, (ID3D11BlendState1 **)&default_blend_state);
    pDevice->CreateDepthStencilState(
        &kDefaultDepthStencilDesc, (ID3D11DepthStencilState **)&default_depth_stencil_state
    );
    /* we don't need the extra reference as they are always valid */
    default_rasterizer_state->Release();
    default_blend_state->Release();
    default_depth_stencil_state->Release();
  }
};

extern "C" MTLFX::TemporalScaler *
CreateMTLFXTemporalScaler(MTLFX::TemporalScalerDescriptor *desc,
                          MTL::Device *device);

template <typename ContextInternalState>
class MTLD3D11ContextExt : public IMTLD3D11ContextExt {
  class CachedTemporalScaler {
  public:
    MTL::PixelFormat color_pixel_format;
    MTL::PixelFormat output_pixel_format;
    MTL::PixelFormat depth_pixel_format;
    MTL::PixelFormat motion_vector_pixel_format;
    bool auto_exposure;
    uint32_t input_width;
    uint32_t input_height;
    uint32_t output_width;
    uint32_t output_height;
    Obj<MTLFX::TemporalScaler> scaler;
  };
public:
  MTLD3D11ContextExt(MTLD3D11DeviceContextImplBase<ContextInternalState> *context) : ctx_(context){};

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    return ctx_->QueryInterface(riid, ppvObject);
  }

  ULONG STDMETHODCALLTYPE AddRef() final { return ctx_->AddRef(); }

  ULONG STDMETHODCALLTYPE Release() final { return ctx_->Release(); }

  MTL::PixelFormat GetCorrectMotionVectorFormat(MTL::PixelFormat format) {
    switch (format) {
    case MTL::PixelFormatRG16Uint:
    case MTL::PixelFormatRG16Float:
    case MTL::PixelFormatRG16Sint:
    case MTL::PixelFormatRG16Snorm:
    case MTL::PixelFormatRG16Unorm:
      return MTL::PixelFormatRG16Float;
    case MTL::PixelFormatRG32Uint:
    case MTL::PixelFormatRG32Float:
    case MTL::PixelFormatRG32Sint:
      return MTL::PixelFormatRG32Float;
    default:
      break;
    }
    return MTL::PixelFormatInvalid;
  }

  void STDMETHODCALLTYPE TemporalUpscale(const MTL_TEMPORAL_UPSCALE_D3D11_DESC *pDesc) final {
    Rc<Texture> input, output, depth, motion_vector, exposure;
    if (!(input = static_cast<D3D11ResourceCommon *>(pDesc->Color)->texture()))
      return;
    if (!(output = static_cast<D3D11ResourceCommon *>(pDesc->Output)->texture()))
      return;
    if (!(depth = static_cast<D3D11ResourceCommon *>(pDesc->Depth)->texture()))
      return;
    if (!(motion_vector = static_cast<D3D11ResourceCommon *>(pDesc->MotionVector)->texture()))
      return;
    if (pDesc->ExposureTexture && !(exposure = static_cast<D3D11ResourceCommon *>(pDesc->ExposureTexture)->texture()))
      return;

    MTL::PixelFormat motion_vector_format = GetCorrectMotionVectorFormat(motion_vector->pixelFormat());
    if (motion_vector_format == MTL::PixelFormatInvalid) {
      ERR("TemporalUpscale: invalid motion vector format ", motion_vector->pixelFormat());
      return;
    }

    if (motion_vector->width() > input->width() || motion_vector->height() > input->height()) {
      ERR("TemporalUpscale: resolution of motion vector is larger than input resolution");
      return;
    }

    Obj<MTLFX::TemporalScaler> scaler;

    for(CachedTemporalScaler& entry: scaler_cache_) {
      if(pDesc->AutoExposure != entry.auto_exposure) continue;
      if(input->width() != entry.input_width) continue;
      if(input->height() != entry.input_height) continue;
      if(output->width() != entry.output_width) continue;
      if(output->height() != entry.output_height) continue;
      if(input->pixelFormat() != entry.color_pixel_format) continue;
      if(output->pixelFormat() != entry.output_pixel_format) continue;
      if(depth->pixelFormat() != entry.depth_pixel_format) continue;
      if(motion_vector_format != entry.motion_vector_pixel_format) continue;

      scaler = entry.scaler;
      break;
    }

    if (!scaler) {
      CachedTemporalScaler scaler_entry;
      scaler_entry.color_pixel_format = input->pixelFormat();
      scaler_entry.output_pixel_format = output->pixelFormat();
      scaler_entry.depth_pixel_format = depth->pixelFormat();
      scaler_entry.motion_vector_pixel_format = motion_vector_format;
      Obj<MTLFX::TemporalScalerDescriptor> descriptor = transfer(MTLFX::TemporalScalerDescriptor::alloc()->init());
      descriptor->setColorTextureFormat(scaler_entry.color_pixel_format);
      descriptor->setOutputTextureFormat(scaler_entry.output_pixel_format);
      descriptor->setDepthTextureFormat(scaler_entry.depth_pixel_format);
      descriptor->setMotionTextureFormat(motion_vector_format);
      scaler_entry.auto_exposure = pDesc->AutoExposure;
      scaler_entry.input_width = input->width();
      scaler_entry.input_height = input->height();
      scaler_entry.output_width = output->width();
      scaler_entry.output_height = output->height();
      descriptor->setAutoExposureEnabled(scaler_entry.auto_exposure);
      descriptor->setInputWidth(scaler_entry.input_width);
      descriptor->setInputHeight(scaler_entry.input_height);
      descriptor->setOutputWidth(scaler_entry.output_width);
      descriptor->setOutputHeight(scaler_entry.output_height);
      descriptor->setInputContentPropertiesEnabled(true);
      descriptor->setInputContentMinScale(1.0f);
      descriptor->setInputContentMaxScale(3.0f);
      descriptor->setRequiresSynchronousInitialization(true);
      scaler_entry.scaler = transfer<MTLFX::TemporalScaler>(CreateMTLFXTemporalScaler(descriptor, ctx_->device->GetMTLDevice()));
      scaler = scaler_entry.scaler;
      scaler_cache_.push_back(std::move(scaler_entry));
      // to simplify implementation, the created scalers are never destroyed
      // as they are not expected to be created frequently
    }

    ctx_->InvalidateCurrentPass();
    ctx_->EmitOP([input = std::move(input), output = std::move(output),
                  depth = std::move(depth),
                  motion_vector = std::move(motion_vector),
                  exposure = std::move(exposure), scaler = std::move(scaler),
                  props =
                      TemporalScalerProps{
                          pDesc->InputContentWidth,
                          pDesc->InputContentHeight,
                          (bool)pDesc->InReset,
                          (bool)pDesc->DepthReversed,
                          pDesc->MotionVectorScaleX,
                          pDesc->MotionVectorScaleY,
                          pDesc->JitterOffsetX,
                          pDesc->JitterOffsetY,
                          pDesc->PreExposure,
                      },
                  motion_vector_format](ArgumentEncodingContext &enc) mutable {
      auto mv_view = motion_vector->createView(
          {.format = motion_vector_format,
           .type = MTL::TextureType2D,
           .usage = motion_vector->current()->texture()->usage(),
           .firstMiplevel = 0,
           .miplevelCount = 1,
           .firstArraySlice = 0,
           .arraySize = 1});
      enc.upscaleTemporal(input, output, depth, motion_vector, mv_view,
                          exposure, scaler, props);
    });
  }

  void STDMETHODCALLTYPE BeginUAVOverlap() final {
    // TODO
  }

  void STDMETHODCALLTYPE EndUAVOverlap() final {
    // TODO
  }

private:

  MTLD3D11DeviceContextImplBase<ContextInternalState> *ctx_;
  std::vector<CachedTemporalScaler> scaler_cache_;
};

struct used_dynamic_buffer {
  Rc<DynamicBuffer> buffer;
  Rc<BufferAllocation> allocation;
  bool latest;
};

struct used_dynamic_texture {
  Rc<DynamicTexture> texture;
  Rc<TextureAllocation> allocation;
  bool latest;
};

class MTLD3D11CommandList : public ManagedDeviceChild<ID3D11CommandList> {
public:
  MTLD3D11CommandList(MTLD3D11Device *pDevice, MTLD3D11CommandListPoolBase *pPool, UINT context_flag) :
      ManagedDeviceChild(pDevice),
      context_flag(context_flag),
      cmdlist_pool(pPool),
      staging_allocator(
          pDevice->GetMTLDevice(), MTL::ResourceOptionCPUCacheModeWriteCombined |
                                       MTL::ResourceHazardTrackingModeUntracked | MTL::ResourceStorageModeShared
      ) {
    cpu_argument_heap = (char *)malloc(kCommandChunkCPUHeapSize);
  };

  ~MTLD3D11CommandList() {
    Reset();
    staging_allocator.free_blocks(~0uLL);
    free(cpu_argument_heap);
  }

  ULONG
  AddRef() override {
    uint32_t refCount = this->m_refCount++;
    if (unlikely(!refCount))
      this->m_parent->AddRef();

    return refCount + 1;
  }

  ULONG
  Release() override {
    uint32_t refCount = --this->m_refCount;
    D3D11_ASSERT(refCount != ~0u && "try to release a 0 reference object");
    if (unlikely(!refCount)) {
      this->m_parent->Release();
      Reset();
      cmdlist_pool->RecycleCommandList(this);
    }

    return refCount;
  }

  void
  Reset() {
    auto current_seq_id = m_parent->GetDXMTDevice().queue().CurrentSeqId();

    while (!used_dynamic_buffers.empty()) {
      auto &buffer = used_dynamic_buffers.back();
      buffer.buffer->recycle(current_seq_id, std::move(buffer.allocation));
      used_dynamic_buffers.pop_back();
    }
    while (!used_dynamic_textures.empty()) {
      auto &texture = used_dynamic_textures.back();
      texture.texture->recycle(current_seq_id, std::move(texture.allocation));
      used_dynamic_textures.pop_back();
    }
    read_staging_resources.clear();
    written_staging_resources.clear();
    visibility_query_count = 0;
    issued_visibility_query.clear();
    issued_event_query.clear();
    list.reset();
    staging_allocator.free_blocks(++local_coherence);

    cpu_arugment_heap_offset = 0;
    promote_flush = false;
  };

  HRESULT
  QueryInterface(REFIID riid, void **ppvObject) override {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) || riid == __uuidof(ID3D11CommandList)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11CommandList), riid)) {
      WARN("D3D11CommandList: Unknown interface query ", str::format(riid));
    }
    return E_NOINTERFACE;
  }

  UINT
  GetContextFlags() override {
    return context_flag;
  };

#pragma region DeferredContext-related

  template <typename T>
  moveonly_list<T>
  AllocateCommandData(size_t n) {
    return moveonly_list<T>((T *)allocate_cpu_heap(sizeof(T) * n, alignof(T)), n);
  }

  std::tuple<void *, MTL::Buffer *, uint64_t>
  AllocateStagingBuffer(size_t size, size_t alignment) {
    return staging_allocator.allocate(1, 0, size, alignment);
  }

  void *
  allocate_cpu_heap(size_t size, size_t alignment) {
    std::size_t adjustment = align_forward_adjustment((void *)cpu_arugment_heap_offset, alignment);
    auto aligned = cpu_arugment_heap_offset + adjustment;
    cpu_arugment_heap_offset = aligned + size;
    if (cpu_arugment_heap_offset >= kCommandChunkCPUHeapSize) {
      ERR(cpu_arugment_heap_offset, " - cpu argument heap overflow, expect error.");
    }
    return ptr_add(cpu_argument_heap, aligned);
  }

  template <typename T>
  T *
  allocate_cpu_heap() {
    return (T *)allocate_cpu_heap(sizeof(T), alignof(T));
  }

  template <CommandWithContext<ArgumentEncodingContext> cmd>
  void Emit(cmd &&fn) {
    list.emit(std::forward<cmd>(fn),
              allocate_cpu_heap(list.calculateCommandSize<cmd>(), 16));
  }

#pragma endregion

#pragma region ImmediateContext-related

  void Execute(ArgumentEncodingContext& ctx) {
    list.execute(ctx);
  };

#pragma endregion

  bool promote_flush = false;

  std::vector<used_dynamic_buffer> used_dynamic_buffers;
  std::vector<used_dynamic_texture> used_dynamic_textures;
  std::vector<Rc<StagingResource>> read_staging_resources;
  std::vector<Rc<StagingResource>> written_staging_resources;
  uint32_t visibility_query_count = 0;
  std::vector<std::pair<Com<IMTLD3DOcclusionQuery>, uint32_t>> issued_visibility_query;
  std::vector<Com<MTLD3D11EventQuery>> issued_event_query;

private:
  UINT context_flag;
  MTLD3D11CommandListPoolBase *cmdlist_pool;

  uint64_t cpu_arugment_heap_offset = 0;
  char *cpu_argument_heap;

  CommandList<ArgumentEncodingContext> list;

  RingBumpAllocator<true, kStagingBlockSizeForDeferredContext> staging_allocator;

  uint64_t local_coherence = 0;
};

} // namespace dxmt