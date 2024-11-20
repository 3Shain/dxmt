
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
#include "d3d11_private.h"
#include "d3d11_context_state.hpp"
#include "d3d11_device.hpp"
#include "d3d11_pipeline.hpp"
#include "d3d11_query.hpp"
#include "dxmt_format.hpp"
#include "dxmt_occlusion_query.hpp"
#include "mtld11_resource.hpp"
#include "util_flags.hpp"
#include "util_math.hpp"
#include <unordered_set>

namespace dxmt {

inline std::pair<MTL::PrimitiveType, uint32_t>
to_metal_primitive_type(D3D11_PRIMITIVE_TOPOLOGY topo) {

  switch (topo) {
  case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
    return {MTL::PrimitiveTypePoint, 0};
  case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
    return {MTL::PrimitiveTypeLine, 0};
  case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
    return {MTL::PrimitiveTypeLineStrip, 0};
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
    return {MTL::PrimitiveTypeTriangle, 0};
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
    return {MTL::PrimitiveTypeTriangleStrip, 0};
  case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
    // FIXME
    return {MTL::PrimitiveTypePoint, 0};
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
    // FIXME
    return {MTL::PrimitiveTypePoint, topo - 32};
  default:
    D3D11_ASSERT(0 && "Invalid topology");
  }
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
}

template <bool Tessellation>
inline MTL_BINDABLE_RESIDENCY_MASK
GetResidencyMask(ShaderType type, bool read, bool write) {
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
    return (read ? MTL_RESIDENCY_READ : MTL_RESIDENCY_NULL) | (write ? MTL_RESIDENCY_WRITE : MTL_RESIDENCY_NULL);
  }
}

inline MTL::ResourceUsage
GetUsageFromResidencyMask(MTL_BINDABLE_RESIDENCY_MASK mask) {
  return ((mask & MTL_RESIDENCY_READ) ? MTL::ResourceUsageRead : 0) |
         ((mask & MTL_RESIDENCY_WRITE) ? MTL::ResourceUsageWrite : 0);
}

inline MTL::RenderStages
GetStagesFromResidencyMask(MTL_BINDABLE_RESIDENCY_MASK mask) {
  return ((mask & (MTL_RESIDENCY_FRAGMENT_READ | MTL_RESIDENCY_FRAGMENT_WRITE)) ? MTL::RenderStageFragment : 0) |
         ((mask & (MTL_RESIDENCY_VERTEX_READ | MTL_RESIDENCY_VERTEX_WRITE)) ? MTL::RenderStageVertex : 0) |
         ((mask & (MTL_RESIDENCY_OBJECT_READ | MTL_RESIDENCY_OBJECT_WRITE)) ? MTL::RenderStageObject : 0) |
         ((mask & (MTL_RESIDENCY_MESH_READ | MTL_RESIDENCY_MESH_WRITE)) ? MTL::RenderStageMesh : 0);
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
  Subresource Dst;
  MTL::Region DstRegion;
  uint32_t EffectiveBytesPerRow;
  uint32_t EffectiveRows;

  MTL_DXGI_FORMAT_DESC DstFormat;

  bool Invalid = true;

  TextureUpdateCommand(BlitObject &Dst_, UINT DstSubresource, const D3D11_BOX *pDstBox) :
      pDst(Dst_.pResource),
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

template <typename ContextInternalState> class MTLD3D11DeviceContextImplBase : public MTLD3D11DeviceContextBase {
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
    if (auto expected = com_cast<IMTLD3D11RenderTargetView>(pRenderTargetView)) {
      ClearRenderTargetView(expected.ptr(), ColorRGBA);
    }
  }

  void
  ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView *pUnorderedAccessView, const UINT Values[4]) override {
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
    pUnorderedAccessView->GetDesc(&desc);
    Com<IMTLBindable> bindable;
    pUnorderedAccessView->QueryInterface(IID_PPV_ARGS(&bindable));
    auto value = std::array<uint32_t, 4>({Values[0], Values[1], Values[2], Values[3]});
    switch (desc.ViewDimension) {
    case D3D11_UAV_DIMENSION_UNKNOWN:
      return;
    case D3D11_UAV_DIMENSION_BUFFER: {
      if (desc.Buffer.Flags & D3D11_BUFFER_UAV_FLAG_RAW) {
        EmitComputeCommandChk<true>([buffer = Use(bindable), value](auto encoder, auto &ctx) {
          ctx.queue->clear_cmd.ClearBufferUint(encoder, buffer.buffer(), buffer.offset(), buffer.width() >> 2, value);
        });
      } else {
        if (desc.Format == DXGI_FORMAT_UNKNOWN) {
          EmitComputeCommandChk<true>([buffer = Use(bindable), value](auto encoder, auto &ctx) {
            ctx.queue->clear_cmd.ClearBufferUint(encoder, buffer.buffer(), buffer.offset(), buffer.width() >> 2, value);
          });
        } else {
          EmitComputeCommandChk<true>([tex = Use(bindable), value](auto encoder, auto &ctx) {
            ctx.queue->clear_cmd.ClearTextureBufferUint(encoder, tex.texture(&ctx), value);
          });
        }
      }
      break;
    }
    case D3D11_UAV_DIMENSION_TEXTURE1D:
      D3D11_ASSERT(0 && "tex1d clear");
      break;
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
      D3D11_ASSERT(0 && "tex1darr clear");
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2D:
      EmitComputeCommandChk<true>([tex = Use(bindable), value](auto encoder, auto &ctx) {
        ctx.queue->clear_cmd.ClearTexture2DUint(encoder, tex.texture(&ctx), value);
      });
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
      D3D11_ASSERT(0 && "tex2darr clear");
      break;
    case D3D11_UAV_DIMENSION_TEXTURE3D:
      EmitComputeCommandChk<true>([tex = Use(bindable), value](auto encoder, auto &ctx) {
        ctx.queue->clear_cmd.ClearTexture3DUint(encoder, tex.texture(&ctx), value);
      });
      break;
    }
    InvalidateComputePipeline();
  }

  void
  ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView *pUnorderedAccessView, const FLOAT Values[4]) override {
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
    pUnorderedAccessView->GetDesc(&desc);
    Com<IMTLBindable> bindable;
    pUnorderedAccessView->QueryInterface(IID_PPV_ARGS(&bindable));
    auto value = std::array<float, 4>({Values[0], Values[1], Values[2], Values[3]});
    switch (desc.ViewDimension) {
    case D3D11_UAV_DIMENSION_UNKNOWN:
      return;
    case D3D11_UAV_DIMENSION_BUFFER: {
      if (desc.Buffer.Flags & D3D11_BUFFER_UAV_FLAG_RAW) {
        EmitComputeCommandChk<true>([buffer = Use(bindable), value](auto encoder, auto &ctx) {
          ctx.queue->clear_cmd.ClearBufferFloat(encoder, buffer.buffer(), buffer.offset(), buffer.width() >> 2, value);
        });
      } else {
        if (desc.Format == DXGI_FORMAT_UNKNOWN) {
          EmitComputeCommandChk<true>([buffer = Use(bindable), value](auto encoder, auto &ctx) {
            ctx.queue->clear_cmd.ClearBufferFloat(
                encoder, buffer.buffer(), buffer.offset(), buffer.width() >> 2, value
            );
          });
        } else {
          EmitComputeCommandChk<true>([tex = Use(bindable), value](auto encoder, auto &ctx) {
            ctx.queue->clear_cmd.ClearTextureBufferFloat(encoder, tex.texture(&ctx), value);
          });
        }
      }
      break;
    }
    case D3D11_UAV_DIMENSION_TEXTURE1D:
      D3D11_ASSERT(0 && "tex1d clear");
      break;
    case D3D11_UAV_DIMENSION_TEXTURE1DARRAY:
      D3D11_ASSERT(0 && "tex1darr clear");
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2D:
      EmitComputeCommandChk<true>([tex = Use(bindable), value](auto encoder, auto &ctx) {
        ctx.queue->clear_cmd.ClearTexture2DFloat(encoder, tex.texture(&ctx), value);
      });
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
      D3D11_ASSERT(0 && "tex2darr clear");
      break;
    case D3D11_UAV_DIMENSION_TEXTURE3D:
      D3D11_ASSERT(0 && "tex3d clear");
      break;
    }
    InvalidateComputePipeline();
  }

  void
  ClearDepthStencilView(ID3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil)
      override {
    if (auto expected = com_cast<IMTLD3D11DepthStencilView>(pDepthStencilView)) {
      ClearDepthStencilView(expected.ptr(), ClearFlags, Depth, Stencil);
    }
  }

  void
  ClearView(ID3D11View *pView, const FLOAT Color[4], const D3D11_RECT *pRect, UINT NumRects) override {
    IMPLEMENT_ME
  }

  void
  GenerateMips(ID3D11ShaderResourceView *pShaderResourceView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    pShaderResourceView->GetDesc(&desc);
    if (desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER || desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX) {
      return;
    }
    if (auto com = com_cast<IMTLBindable>(pShaderResourceView)) {
      EmitBlitCommand<true>([tex = Use(com)](MTL::BlitCommandEncoder *enc, CommandChunk::context &ctx) {
        enc->generateMipmaps(tex.texture(&ctx));
      });
    } else {
      // FIXME: any other possible case?
      D3D11_ASSERT(0 && "unhandled genmips on staging or dynamic resource?");
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
    if (auto dst = com_cast<IMTLBindable>(pDstResource)) {
      if (auto src = com_cast<IMTLBindable>(pSrcResource)) {
        ResolveSubresource(src.ptr(), SrcSubresource, dst.ptr(), dst_level, dst_slice);
      }
    }
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

      if (auto dynamic = com_cast<IMTLDynamicBuffer>(pDstResource)) {
        if (copy_len == desc.ByteWidth) {
          D3D11_MAPPED_SUBRESOURCE mapped;
          Map(pDstResource, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
          memcpy(mapped.pData, pSrcData, copy_len);
          Unmap(pDstResource, 0);
          return;
        }
      }
      if (auto bindable = com_cast<IMTLBindable>(pDstResource)) {
        if (auto _ = UseImmediate(bindable.ptr())) {
          auto buffer = _.buffer();
          memcpy(((char *)buffer->contents()) + copy_offset, pSrcData, copy_len);
          buffer->didModifyRange(NS::Range::Make(copy_offset, copy_len));
          return;
        }
        auto [ptr, staging_buffer, offset] = AllocateStagingBuffer(copy_len, 16);
        memcpy(ptr, pSrcData, copy_len);
        EmitBlitCommand<true>(
            [staging_buffer, offset, dst = Use(bindable), copy_offset,
             copy_len](MTL::BlitCommandEncoder *enc, auto &ctx) {
              enc->copyFromBuffer(staging_buffer, offset, dst.buffer(), copy_offset, copy_len);
            },
            CommandBufferState::UpdateBlitEncoderActive
        );
      } else {
        D3D11_ASSERT(0 && "UpdateSubresource1: TODO: staging?");
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
    if (!PreDraw<false>())
      return;
    auto [Primitive, ControlPointCount] = to_metal_primitive_type(state_.InputAssembler.Topology);
    if (ControlPointCount) {
      return TessellationDraw(ControlPointCount, VertexCount, 1, StartVertexLocation, 0);
    }
    // TODO: skip invalid topology
    EmitRenderCommand([Primitive, StartVertexLocation, VertexCount](MTL::RenderCommandEncoder *encoder) {
      encoder->drawPrimitives(Primitive, StartVertexLocation, VertexCount);
    });
  }

  void
  DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) override {
    if (!PreDraw<true>())
      return;
    auto [Primitive, ControlPointCount] = to_metal_primitive_type(state_.InputAssembler.Topology);
    if (ControlPointCount) {
      return TessellationDrawIndexed(ControlPointCount, IndexCount, StartIndexLocation, BaseVertexLocation, 1, 0);
    };
    auto IndexType =
        state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT ? MTL::IndexTypeUInt32 : MTL::IndexTypeUInt16;
    auto IndexBufferOffset =
        state_.InputAssembler.IndexBufferOffset +
        StartIndexLocation * (state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT ? 4 : 2);
    EmitCommand([IndexType, IndexBufferOffset, Primitive, IndexCount, BaseVertexLocation](CommandChunk::context &ctx) {
      D3D11_ASSERT(ctx.current_index_buffer_ref);
      ctx.render_encoder->drawIndexedPrimitives(
          Primitive, IndexCount, IndexType, ctx.current_index_buffer_ref, IndexBufferOffset, 1, BaseVertexLocation, 0
      );
    });
  }

  void
  DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
      override {
    if (!PreDraw<false>())
      return;
    auto [Primitive, ControlPointCount] = to_metal_primitive_type(state_.InputAssembler.Topology);
    if (ControlPointCount) {
      return TessellationDraw(
          ControlPointCount, VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation
      );
    }
    // TODO: skip invalid topology
    EmitRenderCommand([Primitive, StartVertexLocation, VertexCountPerInstance, InstanceCount,
                       StartInstanceLocation](MTL::RenderCommandEncoder *encoder) {
      encoder->drawPrimitives(
          Primitive, StartVertexLocation, VertexCountPerInstance, InstanceCount, StartInstanceLocation
      );
    });
  }

  void
  DrawIndexedInstanced(
      UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation,
      UINT StartInstanceLocation
  ) override {
    if (!PreDraw<true>())
      return;
    auto [Primitive, ControlPointCount] = to_metal_primitive_type(state_.InputAssembler.Topology);
    if (ControlPointCount) {
      return TessellationDrawIndexed(
          ControlPointCount, IndexCountPerInstance, StartIndexLocation, BaseVertexLocation, InstanceCount,
          StartInstanceLocation
      );
      return;
    }
    // TODO: skip invalid topology
    auto IndexType =
        state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT ? MTL::IndexTypeUInt32 : MTL::IndexTypeUInt16;
    auto IndexBufferOffset =
        state_.InputAssembler.IndexBufferOffset +
        StartIndexLocation * (state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT ? 4 : 2);
    EmitCommand([IndexType, IndexBufferOffset, Primitive, InstanceCount, BaseVertexLocation, StartInstanceLocation,
                 IndexCountPerInstance](CommandChunk::context &ctx) {
      D3D11_ASSERT(ctx.current_index_buffer_ref);
      ctx.render_encoder->drawIndexedPrimitives(
          Primitive, IndexCountPerInstance, IndexType, ctx.current_index_buffer_ref, IndexBufferOffset, InstanceCount,
          BaseVertexLocation, StartInstanceLocation
      );
    });
  }

  void
  TessellationDraw(
      UINT NumControlPoint, UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation,
      UINT StartInstanceLocation
  ) {
    assert(NumControlPoint);

    EmitCommand([=](CommandChunk::context &ctx) {
      // allocate draw arguments
      auto [heap, offset] = ctx.chk->allocate_gpu_heap(4 * 5, 4);
      auto PatchCountPerInstance = VertexCountPerInstance / NumControlPoint;
      DXMT_DRAW_ARGUMENTS *draw_arugment = (DXMT_DRAW_ARGUMENTS *)(((char *)heap->contents()) + offset);
      draw_arugment->BaseVertex = StartVertexLocation;
      draw_arugment->IndexCount = VertexCountPerInstance;
      draw_arugment->StartIndex = 0;
      draw_arugment->InstanceCount = InstanceCount;
      draw_arugment->StartInstance = StartInstanceLocation;
      auto &encoder = ctx.render_encoder;
      encoder->setObjectBuffer(heap, offset, 21);
      assert(ctx.tess_num_output_control_point_element);
      auto PatchPerGroup = 32 / ctx.tess_threads_per_patch;
      auto PatchPerMeshInstance = (PatchCountPerInstance - 1) / PatchPerGroup + 1;
      auto [_0, cp_buffer, cp_offset] = ctx.queue->AllocateTempBuffer(
          ctx.chk->chunk_id, ctx.tess_num_output_control_point_element * 16 * VertexCountPerInstance * InstanceCount, 16
      );
      auto [_1, pc_buffer, pc_offset] = ctx.queue->AllocateTempBuffer(
          ctx.chk->chunk_id, ctx.tess_num_output_patch_constant_scalar * 4 * PatchCountPerInstance * InstanceCount, 4
      );
      auto [_2, tess_factor_buffer, tess_factor_offset] =
          ctx.queue->AllocateTempBuffer(ctx.chk->chunk_id, 6 * 2 * PatchCountPerInstance * InstanceCount, 4);
      encoder->setMeshBuffer(cp_buffer, cp_offset, 20);
      encoder->setMeshBuffer(pc_buffer, pc_offset, 21);
      encoder->setMeshBuffer(tess_factor_buffer, tess_factor_offset, 22);
      encoder->setRenderPipelineState(ctx.tess_mesh_pso);

      encoder->drawMeshThreadgroups(
          MTL::Size(PatchPerMeshInstance, InstanceCount, 1),
          MTL::Size(ctx.tess_threads_per_patch, 32 / ctx.tess_threads_per_patch, 1),
          MTL::Size(ctx.tess_threads_per_patch, 32 / ctx.tess_threads_per_patch, 1)
      );

      encoder->memoryBarrier(MTL::BarrierScopeBuffers, MTL::RenderStageMesh, MTL::RenderStageVertex);

      encoder->setRenderPipelineState(ctx.tess_raster_pso);
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

    EmitCommand([=](CommandChunk::context &ctx) {
      // allocate draw arguments
      auto [heap, offset] = ctx.chk->allocate_gpu_heap(4 * 5, 4);
      auto PatchCountPerInstance = IndexCountPerInstance / NumControlPoint;
      DXMT_DRAW_ARGUMENTS *draw_arugment = (DXMT_DRAW_ARGUMENTS *)(((char *)heap->contents()) + offset);
      draw_arugment->BaseVertex = BaseVertexLocation;
      draw_arugment->IndexCount = IndexCountPerInstance;
      draw_arugment->StartIndex = 0; // already provided offset
      draw_arugment->InstanceCount = InstanceCount;
      draw_arugment->StartInstance = BaseInstance;
      D3D11_ASSERT(ctx.current_index_buffer_ref);
      auto &encoder = ctx.render_encoder;
      encoder->setObjectBuffer(ctx.current_index_buffer_ref, IndexBufferOffset, 20);
      encoder->setObjectBuffer(heap, offset, 21);
      assert(ctx.tess_num_output_control_point_element);
      auto PatchPerGroup = 32 / ctx.tess_threads_per_patch;
      auto PatchPerMeshInstance = (PatchCountPerInstance - 1) / PatchPerGroup + 1;
      auto [_0, cp_buffer, cp_offset] = ctx.queue->AllocateTempBuffer(
          ctx.chk->chunk_id, ctx.tess_num_output_control_point_element * 16 * IndexCountPerInstance * InstanceCount, 16
      );
      auto [_1, pc_buffer, pc_offset] = ctx.queue->AllocateTempBuffer(
          ctx.chk->chunk_id, ctx.tess_num_output_patch_constant_scalar * 4 * PatchCountPerInstance * InstanceCount, 4
      );
      auto [_2, tess_factor_buffer, tess_factor_offset] =
          ctx.queue->AllocateTempBuffer(ctx.chk->chunk_id, 6 * 2 * PatchCountPerInstance * InstanceCount, 4);
      encoder->setMeshBuffer(cp_buffer, cp_offset, 20);
      encoder->setMeshBuffer(pc_buffer, pc_offset, 21);
      encoder->setMeshBuffer(tess_factor_buffer, tess_factor_offset, 22);
      encoder->setRenderPipelineState(ctx.tess_mesh_pso);

      encoder->drawMeshThreadgroups(
          MTL::Size(PatchPerMeshInstance, InstanceCount, 1),
          MTL::Size(ctx.tess_threads_per_patch, 32 / ctx.tess_threads_per_patch, 1),
          MTL::Size(ctx.tess_threads_per_patch, 32 / ctx.tess_threads_per_patch, 1)
      );

      encoder->memoryBarrier(MTL::BarrierScopeBuffers, MTL::RenderStageMesh, MTL::RenderStageVertex);

      encoder->setRenderPipelineState(ctx.tess_raster_pso);
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
  }

  void
  DrawIndexedInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs) override {
    if (!PreDraw<true>())
      return;
    auto [Primitive, ControlPointCount] = to_metal_primitive_type(state_.InputAssembler.Topology);
    if (ControlPointCount) {
      return;
    }
    auto IndexType =
        state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT ? MTL::IndexTypeUInt32 : MTL::IndexTypeUInt16;
    auto IndexBufferOffset = state_.InputAssembler.IndexBufferOffset;
    if (auto bindable = com_cast<IMTLBindable>(pBufferForArgs)) {
      EmitCommand([IndexType, IndexBufferOffset, Primitive, ArgBuffer = Use(bindable),
                   AlignedByteOffsetForArgs](CommandChunk::context &ctx) {
        D3D11_ASSERT(ctx.current_index_buffer_ref);
        ctx.render_encoder->drawIndexedPrimitives(
            Primitive, IndexType, ctx.current_index_buffer_ref, IndexBufferOffset, ArgBuffer.buffer(),
            AlignedByteOffsetForArgs
        );
      });
    }
  }

  void
  DrawInstancedIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs) override {
    if (!PreDraw<true>())
      return;
    auto [Primitive, ControlPointCount] = to_metal_primitive_type(state_.InputAssembler.Topology);
    if (ControlPointCount) {
      return;
    }
    if (auto bindable = com_cast<IMTLBindable>(pBufferForArgs)) {
      EmitCommand([Primitive, ArgBuffer = Use(bindable), AlignedByteOffsetForArgs](CommandChunk::context &ctx) {
        D3D11_ASSERT(ctx.current_index_buffer_ref);
        ctx.render_encoder->drawPrimitives(Primitive, ArgBuffer.buffer(), AlignedByteOffsetForArgs);
      });
    }
  }

  void
  DrawAuto() override {
    ERR("DrawAuto: ignored");
    return;
  }

  void
  Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ) override {
    if (!PreDispatch())
      return;
    EmitComputeCommand<true>([ThreadGroupCountX, ThreadGroupCountY,
                              ThreadGroupCountZ](MTL::ComputeCommandEncoder *encoder, MTL::Size &tg_size) {
      encoder->dispatchThreadgroups(MTL::Size::Make(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ), tg_size);
    });
  }

  void
  DispatchIndirect(ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs) override {
    if (!PreDispatch())
      return;
    if (auto bindable = com_cast<IMTLBindable>(pBufferForArgs)) {
      EmitComputeCommand<true>([AlignedByteOffsetForArgs,
                                ArgBuffer = Use(bindable)](MTL::ComputeCommandEncoder *encoder, MTL::Size &tg_size) {
        encoder->dispatchThreadgroups(ArgBuffer.buffer(), AlignedByteOffsetForArgs, tg_size);
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

    ERR_ONCE("Stub");
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
    state_ = {};
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
    if (auto expected = com_cast<IMTLBindable>(pIndexBuffer)) {
      state_.InputAssembler.IndexBuffer = std::move(expected);
    } else {
      state_.InputAssembler.IndexBuffer = nullptr;
    }
    state_.InputAssembler.IndexBufferFormat = Format;
    state_.InputAssembler.IndexBufferOffset = Offset;
    dirty_state.set(DirtyState::IndexBuffer);
  }
  void
  IAGetIndexBuffer(ID3D11Buffer **pIndexBuffer, DXGI_FORMAT *Format, UINT *Offset) override {
    if (pIndexBuffer) {
      if (state_.InputAssembler.IndexBuffer) {
        state_.InputAssembler.IndexBuffer->GetLogicalResourceOrView(IID_PPV_ARGS(pIndexBuffer));
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
    SetShader<ShaderType::Vertex, ID3D11VertexShader>(pVertexShader, ppClassInstances, NumClassInstances);
  }

  void
  VSGetShader(ID3D11VertexShader **ppVertexShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
      override {
    GetShader<ShaderType::Vertex, ID3D11VertexShader>(ppVertexShader, ppClassInstances, pNumClassInstances);
  }

  void
  VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    SetShaderResource<ShaderType::Vertex>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override {
    GetShaderResource<ShaderType::Vertex>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override {
    SetSamplers<ShaderType::Vertex>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override {
    GetSamplers<ShaderType::Vertex>(StartSlot, NumSamplers, ppSamplers);
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
    SetConstantBuffer<ShaderType::Vertex>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  VSGetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants
  ) override {
    GetConstantBuffer<ShaderType::Vertex>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

#pragma endregion

#pragma region PixelShader

  void
  PSSetShader(ID3D11PixelShader *pPixelShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
      override {
    SetShader<ShaderType::Pixel, ID3D11PixelShader>(pPixelShader, ppClassInstances, NumClassInstances);
  }

  void
  PSGetShader(ID3D11PixelShader **ppPixelShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
      override {
    GetShader<ShaderType::Pixel, ID3D11PixelShader>(ppPixelShader, ppClassInstances, pNumClassInstances);
  }

  void
  PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    SetShaderResource<ShaderType::Pixel>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override {
    GetShaderResource<ShaderType::Pixel>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override {
    SetSamplers<ShaderType::Pixel>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override {

    GetSamplers<ShaderType::Pixel>(StartSlot, NumSamplers, ppSamplers);
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
    SetConstantBuffer<ShaderType::Pixel>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  PSGetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants
  ) override {
    GetConstantBuffer<ShaderType::Pixel>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

#pragma endregion

#pragma region GeometryShader

  void
  GSSetShader(ID3D11GeometryShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
      override {
    SetShader<ShaderType::Geometry>(pShader, ppClassInstances, NumClassInstances);
  }

  void
  GSGetShader(ID3D11GeometryShader **ppGeometryShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
      override {
    GetShader<ShaderType::Geometry>(ppGeometryShader, ppClassInstances, pNumClassInstances);
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
    SetConstantBuffer<ShaderType::Geometry>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override {
    GSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void
  GSGetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants
  ) override {
    GetConstantBuffer<ShaderType::Geometry>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    SetShaderResource<ShaderType::Geometry>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override {
    GetShaderResource<ShaderType::Geometry>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override {
    SetSamplers<ShaderType::Geometry>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override {
    GetSamplers<ShaderType::Geometry>(StartSlot, NumSamplers, ppSamplers);
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
        if (auto bindable = com_cast<IMTLBindable>(pBuffer)) {
          entry.Buffer = std::move(bindable);
          entry.Offset = pOffsets ? pOffsets[slot] : 0;
        }
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
    GetShaderResource<ShaderType::Hull>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  HSGetShader(ID3D11HullShader **ppHullShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
      override {
    GetShader<ShaderType::Hull>(ppHullShader, ppClassInstances, pNumClassInstances);
  }

  void
  HSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override {
    GetSamplers<ShaderType::Hull>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) override {
    HSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void
  HSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    SetShaderResource<ShaderType::Hull>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  HSSetShader(ID3D11HullShader *pHullShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
      override {
    SetShader<ShaderType::Hull>(pHullShader, ppClassInstances, NumClassInstances);
  }

  void
  HSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override {
    SetSamplers<ShaderType::Hull>(StartSlot, NumSamplers, ppSamplers);
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
    SetConstantBuffer<ShaderType::Hull>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  HSGetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants
  ) override {
    GetConstantBuffer<ShaderType::Hull>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

#pragma endregion

#pragma region DomainShader

  void
  DSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    SetShaderResource<ShaderType::Domain>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  DSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override {
    GetShaderResource<ShaderType::Domain>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  DSSetShader(ID3D11DomainShader *pDomainShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances)
      override {
    SetShader<ShaderType::Domain, ID3D11DomainShader>(pDomainShader, ppClassInstances, NumClassInstances);
  }

  void
  DSGetShader(ID3D11DomainShader **ppDomainShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
      override {
    GetShader<ShaderType::Domain, ID3D11DomainShader>(ppDomainShader, ppClassInstances, pNumClassInstances);
  }

  void
  DSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override {
    GetSamplers<ShaderType::Domain>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  DSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override {
    SetSamplers<ShaderType::Domain>(StartSlot, NumSamplers, ppSamplers);
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
    SetConstantBuffer<ShaderType::Domain>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
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
    GetConstantBuffer<ShaderType::Domain>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

#pragma endregion

#pragma region ComputeShader

  void
  CSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) override {
    GetShaderResource<ShaderType::Compute>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  CSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    SetShaderResource<ShaderType::Compute>(StartSlot, NumViews, ppShaderResourceViews);
  }

  void
  CSSetUnorderedAccessViews(
      UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
      const UINT *pUAVInitialCounts
  ) override {
    SetUnorderedAccessView<ShaderType::Compute>(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
  }

  void
  CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews) override {
    for (auto i = 0u; i < NumUAVs; i++) {
      if (state_.ComputeStageUAV.UAVs.test_bound(StartSlot + i)) {
        state_.ComputeStageUAV.UAVs[StartSlot + i].View->GetLogicalResourceOrView(
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
    SetShader<ShaderType::Compute>(pComputeShader, ppClassInstances, NumClassInstances);
  }

  void
  CSGetShader(ID3D11ComputeShader **ppComputeShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances)
      override {
    GetShader<ShaderType::Compute>(ppComputeShader, ppClassInstances, pNumClassInstances);
  }

  void
  CSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) override {
    SetSamplers<ShaderType::Compute>(StartSlot, NumSamplers, ppSamplers);
  }

  void
  CSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) override {
    GetSamplers<ShaderType::Compute>(StartSlot, NumSamplers, ppSamplers);
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
    SetConstantBuffer<ShaderType::Compute>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
  }

  void
  CSGetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants
  ) override {
    GetConstantBuffer<ShaderType::Compute>(StartSlot, NumBuffers, ppConstantBuffers, pFirstConstant, pNumConstants);
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

    if (dsv) {
      ref = &dsv->GetAttachmentDesc();
    }

    for (unsigned i = 0; i < NumRTVs; i++) {
      auto rtv = static_cast<IMTLD3D11RenderTargetView *>(ppRenderTargetViews[i]);
      if (rtv) {
        // TODO: render target type and size should be checked as well
        if (ref) {
          auto &props = rtv->GetAttachmentDesc();
          if (props.SampleCount != ref->SampleCount)
            return false;
          if (props.RenderTargetArrayLength != ref->RenderTargetArrayLength)
            return false;
        } else {
          ref = &rtv->GetAttachmentDesc();
        }
      }
    }

    if (ref) {
      state_.OutputMerger.SampleCount = ref->SampleCount;
      state_.OutputMerger.ArrayLength = ref->RenderTargetArrayLength;
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
      if (!ValidateSetRenderTargets(NumRTVs, ppRenderTargetViews, pDepthStencilView))
        return;
      auto &BoundRTVs = state_.OutputMerger.RTVs;
      constexpr unsigned RTVSlotCount = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
      for (unsigned rtv_index = 0; rtv_index < RTVSlotCount; rtv_index++) {
        if (rtv_index < NumRTVs && ppRenderTargetViews[rtv_index]) {
          if (auto expected = com_cast<IMTLD3D11RenderTargetView>(ppRenderTargetViews[rtv_index])) {
            if (BoundRTVs[rtv_index].ptr() == expected.ptr())
              continue;
            BoundRTVs[rtv_index] = std::move(expected);
            should_invalidate_pass = true;
          } else {
            D3D11_ASSERT(0 && "wtf");
          }
        } else {
          if (BoundRTVs[rtv_index]) {
            should_invalidate_pass = true;
          }
          BoundRTVs[rtv_index] = nullptr;
        }
      }
      state_.OutputMerger.NumRTVs = NumRTVs;

      if (auto expected = com_cast<IMTLD3D11DepthStencilView>(pDepthStencilView)) {
        if (state_.OutputMerger.DSV.ptr() != expected.ptr()) {
          state_.OutputMerger.DSV = std::move(expected);
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
      SetUnorderedAccessView<ShaderType::Pixel>(UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
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
        ppDepthStencilView = nullptr;
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

  template <typename Fn> void EmitCommand(Fn &&fn);

  BindingRef Use(IMTLBindable *bindable);
  BindingRef
  Use(Com<IMTLBindable> bindable) {
    return Use(bindable.ptr());
  }
  BindingRef Use(IMTLD3D11RenderTargetView *rtv);
  BindingRef Use(IMTLD3D11DepthStencilView *dsv);
  BindingRef Use(IMTLDynamicBuffer *dynamic_buffer);

  ENCODER_INFO *GetLastEncoder();
  ENCODER_CLEARPASS_INFO *MarkClearPass();
  ENCODER_RENDER_INFO *MarkRenderPass();
  ENCODER_INFO *MarkPass(EncoderKind kind);

  template <typename T> moveonly_list<T> AllocateCommandData(size_t n = 1);

  void UpdateUAVCounter(IMTLD3D11UnorderedAccessView *uav, uint32_t value);

  std::tuple<void *, MTL::Buffer *, uint64_t> AllocateStagingBuffer(size_t size, size_t alignment);

  bool UseCopyDestination(
      IMTLD3D11Staging *pResource, uint32_t Subresource, MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
      uint32_t *pBytesPerImage
  );
  bool UseCopySource(
      IMTLD3D11Staging *pResource, uint32_t Subresource, MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
      uint32_t *pBytesPerImage
  );

  // on deferred context, it always returns null
  BindingRef UseImmediate(IMTLBindable *bindable);

  template <ShaderType stage, bool Tessellation> bool UploadShaderStageResourceBinding();

  void UpdateVertexBuffer();

  virtual void Commit() = 0;

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
  void
  SetShader(IShader *pShader, ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    if (pShader) {
      if (auto expected = com_cast<IMTLD3D11Shader>(pShader)) {
        if (ShaderStage.Shader.ptr() == expected.ptr()) {
          return;
        }
        ShaderStage.Shader = std::move(expected);
        ShaderStage.ConstantBuffers.set_dirty();
        ShaderStage.SRVs.set_dirty();
        ShaderStage.Samplers.set_dirty();
        if (Type == ShaderType::Compute) {
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

    if (Type == ShaderType::Compute) {
      InvalidateComputePipeline();
    } else {
      InvalidateRenderPipeline();
    }
  }

  template <ShaderType Type, typename IShader>
  void
  GetShader(IShader **ppShader, ID3D11ClassInstance **ppClassInstances, UINT *pNumClassInstances) {
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
  void
  SetConstantBuffer(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers, const UINT *pFirstConstant,
      const UINT *pNumConstants
  ) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    for (unsigned slot = StartSlot; slot < StartSlot + NumBuffers; slot++) {
      auto pConstantBuffer = ppConstantBuffers[slot - StartSlot];
      if (pConstantBuffer) {
        bool replaced = false;
        auto &entry = ShaderStage.ConstantBuffers.bind(slot, {pConstantBuffer}, replaced);
        if (!replaced) {
          if (pFirstConstant && pFirstConstant[slot - StartSlot] != entry.FirstConstant) {
            ShaderStage.ConstantBuffers.set_dirty(slot);
            entry.FirstConstant = pFirstConstant[slot - StartSlot];
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
        if (auto expected = com_cast<IMTLBindable>(pConstantBuffer)) {
          entry.Buffer = std::move(expected);
        } else {
          D3D11_ASSERT(0 && "unexpected constant buffer object");
        }
      } else {
        // BIND NULL
        ShaderStage.ConstantBuffers.unbind(slot);
      }
    }
  }

  template <ShaderType Type>
  void
  GetConstantBuffer(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers, UINT *pFirstConstant, UINT *pNumConstants
  ) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    for (auto i = 0u; i < NumBuffers; i++) {
      if (ShaderStage.ConstantBuffers.test_bound(StartSlot + i)) {
        auto &cb = ShaderStage.ConstantBuffers.at(StartSlot + i);
        if (ppConstantBuffers) {
          cb.Buffer->GetLogicalResourceOrView(IID_PPV_ARGS(&ppConstantBuffers[i]));
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
  SetShaderResource(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView *const *ppShaderResourceViews) {

    auto &ShaderStage = state_.ShaderStages[(UINT)Type];
    for (unsigned slot = StartSlot; slot < StartSlot + NumViews; slot++) {
      auto pView = ppShaderResourceViews[slot - StartSlot];
      if (pView) {
        bool replaced = false;
        auto &entry = ShaderStage.SRVs.bind(slot, {pView}, replaced);
        if (!replaced)
          continue;
        else if (auto expected = com_cast<IMTLBindable>(pView)) {
          entry.SRV = std::move(expected);
        } else {
          D3D11_ASSERT(0 && "unexpected shader resource object");
        }
      } else {
        // BIND NULL
        ShaderStage.SRVs.unbind(slot);
      }
    }
  }

  template <ShaderType Type>
  void
  GetShaderResource(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView **ppShaderResourceViews) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    if (!ppShaderResourceViews)
      return;
    for (auto i = 0u; i < NumViews; i++) {
      if (ShaderStage.SRVs.test_bound(StartSlot + i)) {
        ShaderStage.SRVs[StartSlot + i].SRV->GetLogicalResourceOrView(IID_PPV_ARGS(&ppShaderResourceViews[i]));
      } else {
        ppShaderResourceViews[i] = nullptr;
      }
    }
  }

  template <ShaderType Type>
  void
  SetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState *const *ppSamplers) {
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
  void
  GetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState **ppSamplers) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];
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

  template <ShaderType Type>
  void
  SetUnorderedAccessView(
      UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
      const UINT *pUAVInitialCounts
  ) {
    auto &binding_set = Type == ShaderType::Compute ? state_.ComputeStageUAV.UAVs : state_.OutputMerger.UAVs;

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

    for (unsigned slot = StartSlot; slot < StartSlot + NumUAVs; slot++) {
      auto pUAV = ppUnorderedAccessViews[slot - StartSlot];
      auto InitialCount = pUAVInitialCounts ? pUAVInitialCounts[slot - StartSlot] : ~0u;
      if (pUAV) {
        auto uav = com_cast<IMTLD3D11UnorderedAccessView>(pUAV);
        bool replaced = false;
        auto &entry = binding_set.bind(slot, {pUAV}, replaced);
        if (InitialCount != ~0u) {
          UpdateUAVCounter(uav.ptr(), InitialCount);
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
          continue;
        }
        if (auto expected = com_cast<IMTLBindable>(pVertexBuffer)) {
          entry.Buffer = std::move(expected);
        } else {
          D3D11_ASSERT(0 && "unexpected vertex buffer object");
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
      } else {
        VertexBuffers.unbind(slot);
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
        VertexBuffer.Buffer->GetLogicalResourceOrView(IID_PPV_ARGS(&ppVertexBuffers[i]));
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
    if (auto staging_dst = com_cast<IMTLD3D11Staging>(pDstResource)) {
      if (auto staging_src = com_cast<IMTLD3D11Staging>(pSrcResource)) {
        D3D11_ASSERT(0 && "todo: copy between staging");
      } else if (auto src = com_cast<IMTLBindable>(pSrcResource)) {
        // copy from device to staging
        MTL_STAGING_RESOURCE dst_bind;
        uint32_t bytes_per_row, bytes_per_image;
        if (!UseCopyDestination(staging_dst.ptr(), DstSubresource, &dst_bind, &bytes_per_row, &bytes_per_image))
          return;
        EmitBlitCommand<true>(
            [src_ = Use(src), dst = Obj(dst_bind.Buffer), DstX, SrcBox](MTL::BlitCommandEncoder *encoder, auto &ctx) {
              auto src = src_.buffer();
              encoder->copyFromBuffer(src, SrcBox.left, dst, DstX, SrcBox.right - SrcBox.left);
            },
            CommandBufferState::ReadbackBlitEncoderActive
        );
        promote_flush = true;
      } else {
        D3D11_ASSERT(0 && "todo");
      }
    } else if (auto dst = com_cast<IMTLBindable>(pDstResource)) {
      if (auto staging_src = com_cast<IMTLD3D11Staging>(pSrcResource)) {
        // copy from staging to default
        MTL_STAGING_RESOURCE src_bind;
        uint32_t bytes_per_row, bytes_per_image;
        if (!UseCopySource(staging_src.ptr(), SrcSubresource, &src_bind, &bytes_per_row, &bytes_per_image))
          return;
        EmitBlitCommand<true>([dst_ = Use(dst), src = Obj(src_bind.Buffer), DstX,
                               SrcBox](MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto dst = dst_.buffer();
          // FIXME: offste should be calculated from SrcBox
          encoder->copyFromBuffer(src, SrcBox.left, dst, DstX, SrcBox.right - SrcBox.left);
        });
      } else if (auto src = com_cast<IMTLBindable>(pSrcResource)) {
        // on-device copy
        EmitBlitCommand<true>([dst_ = Use(dst), src_ = Use(src), DstX,
                               SrcBox](MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto src = src_.buffer();
          auto dst = dst_.buffer();
          encoder->copyFromBuffer(src, SrcBox.left, dst, DstX, SrcBox.right - SrcBox.left);
        });
      } else {
        D3D11_ASSERT(0 && "todo");
      }
    } else {
      D3D11_ASSERT(0 && "todo");
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
    if (auto staging_dst = com_cast<IMTLD3D11Staging>(cmd.pDst)) {
      if (auto staging_src = com_cast<IMTLD3D11Staging>(cmd.pSrc)) {
        D3D11_ASSERT(0 && "TODO: copy between staging");
      } else if (auto src = com_cast<IMTLBindable>(cmd.pSrc)) {
        // copy from device to staging
        MTL_STAGING_RESOURCE dst_bind;
        uint32_t bytes_per_row, bytes_per_image;
        if (!UseCopyDestination(staging_dst.ptr(), cmd.DstSubresource, &dst_bind, &bytes_per_row, &bytes_per_image))
          return;
        EmitBlitCommand<true>(
            [src_ = Use(src), dst = Obj(dst_bind.Buffer), bytes_per_row, bytes_per_image,
             cmd = std::move(cmd)](MTL::BlitCommandEncoder *encoder, auto &ctx) {
              auto src = src_.texture(&ctx);
              // auto offset = DstOrigin.z * bytes_per_image +
              //               DstOrigin.y * bytes_per_row +
              //               DstOrigin.x * bytes_per_texel;
              encoder->copyFromTexture(
                  src, cmd.Src.ArraySlice, cmd.Src.MipLevel, cmd.SrcOrigin, cmd.SrcSize, dst.ptr(), 0 /* offset */,
                  bytes_per_row, bytes_per_image
              );
            },
            CommandBufferState::ReadbackBlitEncoderActive
        );
        promote_flush = true;
      } else {
        D3D11_ASSERT(0 && "TODO: copy from dynamic to staging");
      }
    } else if (auto dst = com_cast<IMTLBindable>(cmd.pDst)) {
      if (auto staging_src = com_cast<IMTLD3D11Staging>(cmd.pSrc)) {
        // copy from staging to default
        MTL_STAGING_RESOURCE src_bind;
        uint32_t bytes_per_row, bytes_per_image;
        if (!UseCopySource(staging_src.ptr(), cmd.SrcSubresource, &src_bind, &bytes_per_row, &bytes_per_image))
          return;
        EmitBlitCommand<true>([dst_ = Use(dst), src = Obj(src_bind.Buffer), bytes_per_row, bytes_per_image,
                               cmd = std::move(cmd)](MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto dst = dst_.texture(&ctx);
          uint32_t offset;
          if (cmd.SrcFormat.Flag & MTL_DXGI_FORMAT_BC) {
            offset = cmd.SrcOrigin.z * bytes_per_image + (cmd.SrcOrigin.y >> 2) * bytes_per_row +
                     (cmd.SrcOrigin.x >> 2) * cmd.SrcFormat.BytesPerTexel;
          } else {
            offset = cmd.SrcOrigin.z * bytes_per_image + cmd.SrcOrigin.y * bytes_per_row +
                     cmd.SrcOrigin.x * cmd.SrcFormat.BytesPerTexel;
          }
          encoder->copyFromBuffer(
              src, offset, bytes_per_row, bytes_per_image, cmd.SrcSize, dst, cmd.Dst.ArraySlice, cmd.Dst.MipLevel,
              cmd.DstOrigin
          );
        });
      } else if (auto src = com_cast<IMTLBindable>(cmd.pSrc)) {
        // on-device copy
        EmitBlitCommand<true>([dst_ = Use(dst), src_ = Use(src),
                               cmd = std::move(cmd)](MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto src = src_.texture(&ctx);
          auto dst = dst_.texture(&ctx);
          auto src_format = src->pixelFormat();
          auto dst_format = dst->pixelFormat();
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
            auto [_, buffer, offset] = ctx.queue->AllocateTempBuffer(ctx.chk->chunk_id, bytes_total, 16);
            encoder->copyFromTexture(
                src, cmd.Src.ArraySlice, cmd.Src.MipLevel, cmd.SrcOrigin, cmd.SrcSize, buffer, offset, bytes_per_row,
                bytes_per_image
            );
            encoder->copyFromBuffer(
                buffer, offset, bytes_per_row, bytes_per_image, cmd.SrcSize, dst, cmd.Dst.ArraySlice, cmd.Dst.MipLevel,
                cmd.DstOrigin
            );
            return;
          }
          encoder->copyFromTexture(
              src, cmd.Src.ArraySlice, cmd.Src.MipLevel, cmd.SrcOrigin, cmd.SrcSize, dst, cmd.Dst.ArraySlice,
              cmd.Dst.MipLevel, cmd.DstOrigin
          );
        });
      } else if (auto dynamic_src = com_cast<IMTLDynamicBuffer>(cmd.pSrc)) {
        uint32_t bytes_per_row, bytes_per_image;
        dynamic_src->GetSize(&bytes_per_row, &bytes_per_image);
        EmitBlitCommand<true>([dst_ = Use(dst), src_buffer = Use(dynamic_src.ptr()), bytes_per_row, bytes_per_image,
                               cmd = std::move(cmd)](MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto dst = dst_.texture(&ctx);
          auto src = src_buffer.buffer();
          uint32_t offset;
          if (cmd.SrcFormat.Flag & MTL_DXGI_FORMAT_BC) {
            D3D11_ASSERT(0 && "Unexpected dynamic BC texture");
            offset = cmd.SrcOrigin.z * bytes_per_image + (cmd.SrcOrigin.y >> 2) * bytes_per_row +
                     (cmd.SrcOrigin.x >> 2) * cmd.SrcFormat.BytesPerTexel;
          } else {
            offset = cmd.SrcOrigin.z * bytes_per_image + cmd.SrcOrigin.y * bytes_per_row +
                     cmd.SrcOrigin.x * cmd.SrcFormat.BytesPerTexel;
          }
          encoder->copyFromBuffer(
              src, offset, bytes_per_row, bytes_per_image, cmd.SrcSize, dst, cmd.Dst.ArraySlice, cmd.Dst.MipLevel,
              cmd.DstOrigin
          );
        });
      } else {
        D3D11_ASSERT(0 && "Unexpected texture copy source");
      }
    } else {
      D3D11_ASSERT(0 && "TODO: copy to dynamic?");
    }
  }

  void
  CopyTextureFromCompressed(TextureCopyCommand &&cmd) {
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
        EmitBlitCommand<true>([dst_ = Use(dst), src_ = Use(src),
                               cmd = std::move(cmd)](MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto src = src_.texture(&ctx);
          auto dst = dst_.texture(&ctx);
          auto block_w = (align(cmd.SrcSize.width, 4u) >> 2);
          auto block_h = (align(cmd.SrcSize.height, 4u) >> 2);
          auto bytes_per_row = block_w * cmd.SrcFormat.BytesPerTexel;
          auto bytes_per_image = bytes_per_row * block_h;
          auto [_, buffer, offset] =
              ctx.queue->AllocateTempBuffer(ctx.chk->chunk_id, bytes_per_image * cmd.SrcSize.depth, 16);
          encoder->copyFromTexture(
              src, cmd.Src.ArraySlice, cmd.Src.MipLevel, cmd.SrcOrigin, cmd.SrcSize, buffer, offset, bytes_per_row,
              bytes_per_image
          );
          encoder->copyFromBuffer(
              buffer, offset, bytes_per_row, bytes_per_image, MTL::Size::Make(block_w, block_h, cmd.SrcSize.depth), dst,
              cmd.Dst.ArraySlice, cmd.Dst.MipLevel, cmd.DstOrigin
          );
          return;
        });
      } else {
        D3D11_ASSERT(0 && "TODO: copy from compressed dynamic to device");
      }
    } else {
      D3D11_ASSERT(0 && "TODO: copy from compressed ? to dynamic");
    }
  }

  void
  CopyTextureToCompressed(TextureCopyCommand &&cmd) {
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
        EmitBlitCommand<true>([dst_ = Use(dst), src_ = Use(src),
                               cmd = std::move(cmd)](MTL::BlitCommandEncoder *encoder, auto &ctx) {
          auto src = src_.texture(&ctx);
          auto dst = dst_.texture(&ctx);
          auto bytes_per_row = cmd.SrcSize.width * cmd.SrcFormat.BytesPerTexel;
          auto bytes_per_image = cmd.SrcSize.height * bytes_per_row;
          auto [_, buffer, offset] =
              ctx.queue->AllocateTempBuffer(ctx.chk->chunk_id, bytes_per_image * cmd.SrcSize.depth, 16);
          encoder->copyFromTexture(
              src, cmd.Src.ArraySlice, cmd.Src.MipLevel, cmd.SrcOrigin, cmd.SrcSize, buffer, offset, bytes_per_row,
              bytes_per_image
          );
          auto clamped_src_width = std::min(
              cmd.SrcSize.width << 2, std::max<uint32_t>(dst->width() >> cmd.Dst.MipLevel, 1u) - cmd.DstOrigin.x
          );
          auto clamped_src_height = std::min(
              cmd.SrcSize.height << 2, std::max<uint32_t>(dst->height() >> cmd.Dst.MipLevel, 1u) - cmd.DstOrigin.y
          );
          encoder->copyFromBuffer(
              buffer, offset, bytes_per_row, bytes_per_image,
              MTL::Size::Make(clamped_src_width, clamped_src_height, cmd.SrcSize.depth), dst, cmd.Dst.ArraySlice,
              cmd.Dst.MipLevel, cmd.DstOrigin
          );
          return;
        });
      } else {
        D3D11_ASSERT(0 && "TODO: copy from dynamic to compressed device");
      }
    } else {
      D3D11_ASSERT(0 && "TODO: copy to compressed dynamic");
    }
  }

  void
  UpdateTexture(
      TextureUpdateCommand &&cmd, const void *pSrcData, UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags
  ) {
    if (cmd.Invalid)
      return;

    if (auto bindable = com_cast<IMTLBindable>(cmd.pDst)) {
      if (auto dst = UseImmediate(bindable.ptr())) {
        auto texture = dst.texture();
        if (texture) {
          texture->replaceRegion(
              cmd.DstRegion, cmd.Dst.MipLevel, cmd.Dst.ArraySlice, pSrcData, SrcRowPitch, SrcDepthPitch
          );
          return;
        }
      }
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
      EmitBlitCommand<true>(
          [staging_buffer, offset, dst = Use(bindable), cmd = std::move(cmd),
           bytes_per_depth_slice](MTL::BlitCommandEncoder *enc, auto &ctx) {
            enc->copyFromBuffer(
                staging_buffer, offset, cmd.EffectiveBytesPerRow, bytes_per_depth_slice, cmd.DstRegion.size,
                dst.texture(&ctx), cmd.Dst.ArraySlice, cmd.Dst.MipLevel, cmd.DstRegion.origin
            );
          },
          CommandBufferState::UpdateBlitEncoderActive
      );
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
  bool
  InvalidateCurrentPass(bool defer_commit = false) {
    switch (cmdbuf_state) {
    case CommandBufferState::Idle:
      break;
    case CommandBufferState::RenderEncoderActive:
    case CommandBufferState::RenderPipelineReady:
    case CommandBufferState::TessellationRenderPipelineReady: {
      EmitCommand([](CommandChunk::context &ctx) {
        ctx.render_encoder->endEncoding();
        ctx.render_encoder = nullptr;
        ctx.dsv_planar_flags = 0;
      });
      vro_state.endEncoder();
      break;
    }
    case CommandBufferState::ComputeEncoderActive:
    case CommandBufferState::ComputePipelineReady:
      EmitCommand([](CommandChunk::context &ctx) {
        ctx.compute_encoder->endEncoding();
        ctx.compute_encoder = nullptr;
      });
      break;
    case CommandBufferState::UpdateBlitEncoderActive:
    case CommandBufferState::ReadbackBlitEncoderActive:
    case CommandBufferState::BlitEncoderActive:
      EmitCommand([](CommandChunk::context &ctx) {
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
  EncodeClearPass(ENCODER_CLEARPASS_INFO *clear_pass) {
    EmitCommand([clear_pass, _ = clear_pass->use_clearpass()](CommandChunk::context &ctx) {
      if (clear_pass->skipped) {
        return;
      }
      auto pool = transfer(NS::AutoreleasePool::alloc()->init());
      auto enc_descriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
      if (clear_pass->depth_stencil_flags) {
        MTL::Texture *texture = clear_pass->clear_attachment.texture(&ctx);
        uint32_t planar_flags = DepthStencilPlanarFlags(texture->pixelFormat());
        if (clear_pass->depth_stencil_flags & planar_flags & D3D11_CLEAR_DEPTH) {
          auto attachmentz = enc_descriptor->depthAttachment();
          attachmentz->setClearDepth(clear_pass->depth_stencil.depth);
          attachmentz->setTexture(texture);
          attachmentz->setLoadAction(MTL::LoadActionClear);
          attachmentz->setStoreAction(MTL::StoreActionStore);
          attachmentz->setSlice(clear_pass->slice);
          attachmentz->setLevel(clear_pass->level);
          attachmentz->setDepthPlane(clear_pass->depth_plane);
        }
        if (clear_pass->depth_stencil_flags & planar_flags & D3D11_CLEAR_STENCIL) {
          auto attachmentz = enc_descriptor->stencilAttachment();
          attachmentz->setClearStencil(clear_pass->depth_stencil.stencil);
          attachmentz->setTexture(texture);
          attachmentz->setLoadAction(MTL::LoadActionClear);
          attachmentz->setStoreAction(MTL::StoreActionStore);
          attachmentz->setSlice(clear_pass->slice);
          attachmentz->setLevel(clear_pass->level);
          attachmentz->setDepthPlane(clear_pass->depth_plane);
        }
        enc_descriptor->setRenderTargetHeight(texture->height());
        enc_descriptor->setRenderTargetWidth(texture->width());
      } else {
        auto attachmentz = enc_descriptor->colorAttachments()->object(0);
        attachmentz->setClearColor(clear_pass->color);
        attachmentz->setTexture(clear_pass->clear_attachment.texture(&ctx));
        attachmentz->setLoadAction(MTL::LoadActionClear);
        attachmentz->setStoreAction(MTL::StoreActionStore);
        attachmentz->setSlice(clear_pass->slice);
        attachmentz->setLevel(clear_pass->level);
        attachmentz->setDepthPlane(clear_pass->depth_plane);
      }
      enc_descriptor->setRenderTargetArrayLength(clear_pass->array_length);
      auto enc = ctx.cmdbuf->renderCommandEncoder(enc_descriptor);
      enc->setLabel(NS::String::string("ClearPass", NS::ASCIIStringEncoding));
      enc->endEncoding();
    });
  };

  void
  ClearRenderTargetView(IMTLD3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4]) {
    InvalidateCurrentPass();

    auto &props = pRenderTargetView->GetAttachmentDesc();
    auto clear_color = MTL::ClearColor::Make(ColorRGBA[0], ColorRGBA[1], ColorRGBA[2], ColorRGBA[3]);

    auto previous_encoder = GetLastEncoder();

    auto clear_pass = MarkClearPass();
    clear_pass->clear_attachment = Use(pRenderTargetView);
    clear_pass->slice = props.Slice;
    clear_pass->level = props.Level;
    clear_pass->depth_plane = props.DepthPlane;
    clear_pass->array_length = props.RenderTargetArrayLength;
    clear_pass->color = clear_color;

    if (previous_encoder->kind == EncoderKind::ClearPass)
      clear_pass->previous_clearpass = (ENCODER_CLEARPASS_INFO *)previous_encoder;

    EncodeClearPass(clear_pass);
  }

  void
  ClearDepthStencilView(IMTLD3D11DepthStencilView *pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil) {
    if (ClearFlags == 0)
      return;
    InvalidateCurrentPass();

    auto &props = pDepthStencilView->GetAttachmentDesc();

    auto previous_encoder = GetLastEncoder();

    auto clear_pass = MarkClearPass();
    clear_pass->clear_attachment = Use(pDepthStencilView);
    clear_pass->depth_stencil_flags = ClearFlags & 0b11;
    clear_pass->slice = props.Slice;
    clear_pass->level = props.Level;
    clear_pass->depth_plane = props.DepthPlane;
    clear_pass->array_length = props.RenderTargetArrayLength;
    if (ClearFlags & D3D11_CLEAR_DEPTH)
      clear_pass->depth_stencil.depth = Depth;
    if (ClearFlags & D3D11_CLEAR_STENCIL)
      clear_pass->depth_stencil.stencil = Stencil;

    if (previous_encoder->kind == EncoderKind::ClearPass)
      clear_pass->previous_clearpass = (ENCODER_CLEARPASS_INFO *)previous_encoder;

    EncodeClearPass(clear_pass);
  }

  void
  ResolveSubresource(IMTLBindable *pSrc, UINT SrcSlice, IMTLBindable *pDst, UINT DstLevel, UINT DstSlice) {
    InvalidateCurrentPass();
    MarkPass(EncoderKind::Resolve);
    EmitCommand([src = Use(pSrc), dst = Use(pDst), SrcSlice, DstSlice, DstLevel](CommandChunk::context &ctx) {
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
    state_.ShaderStages[0].ConstantBuffers.set_dirty();
    state_.ShaderStages[0].Samplers.set_dirty();
    state_.ShaderStages[0].SRVs.set_dirty();
    state_.ShaderStages[1].ConstantBuffers.set_dirty();
    state_.ShaderStages[1].Samplers.set_dirty();
    state_.ShaderStages[1].SRVs.set_dirty();
    state_.ShaderStages[3].ConstantBuffers.set_dirty();
    state_.ShaderStages[3].Samplers.set_dirty();
    state_.ShaderStages[3].SRVs.set_dirty();
    state_.ShaderStages[4].ConstantBuffers.set_dirty();
    state_.ShaderStages[4].Samplers.set_dirty();
    state_.ShaderStages[4].SRVs.set_dirty();
    state_.InputAssembler.VertexBuffers.set_dirty();
    dirty_state.set(
        DirtyState::BlendFactorAndStencilRef, DirtyState::RasterizerState, DirtyState::DepthStencilState,
        DirtyState::Viewport, DirtyState::Scissors, DirtyState::IndexBuffer
    );

    // should assume: render target is properly set
    {
      /* Setup RenderCommandEncoder */
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
      // FIXME: is this value always valid?
      uint32_t render_target_array = state_.OutputMerger.ArrayLength;
      auto rtvs = AllocateCommandData<RENDER_TARGET_STATE>(state_.OutputMerger.NumRTVs);
      for (unsigned i = 0; i < state_.OutputMerger.NumRTVs; i++) {
        auto &rtv = state_.OutputMerger.RTVs[i];
        if (rtv) {
          auto props = rtv->GetAttachmentDesc();
          rtvs[i] = {Use(rtv.ptr()), i, props.Level, props.Slice, props.DepthPlane, rtv->GetPixelFormat()};
          D3D11_ASSERT(rtv->GetPixelFormat() != MTL::PixelFormatInvalid);
          effective_render_target++;
        } else {
          rtvs[i].RenderTargetIndex = i;
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
        dsv_info.Texture = Use(state_.OutputMerger.DSV.ptr());
        dsv_info.PixelFormat = state_.OutputMerger.DSV->GetPixelFormat();
      } else if (effective_render_target == 0) {
        if (state_.OutputMerger.NumRTVs) {
          ERR("NumRTVs is non-zero but all render targets are null.");
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

      auto previous_encoder = GetLastEncoder();
      if (previous_encoder->kind == EncoderKind::ClearPass) {
        auto previous_clearpass = (ENCODER_CLEARPASS_INFO *)previous_encoder;
        while (previous_clearpass) {
          if (previous_clearpass->depth_stencil_flags) {
            if (previous_clearpass->clear_attachment == dsv_info.Texture) {
              if ((previous_clearpass->depth_stencil_flags & D3D11_CLEAR_DEPTH) &&
                  dsv_info.DepthLoadAction != MTL::LoadActionClear) {
                dsv_info.DepthLoadAction = MTL::LoadActionClear;
                dsv_info.ClearDepth = previous_clearpass->depth_stencil.depth;
                previous_clearpass->depth_stencil_flags &= ~D3D11_CLEAR_DEPTH;
              }
              if ((previous_clearpass->depth_stencil_flags & D3D11_CLEAR_STENCIL) &&
                  dsv_info.StencilLoadAction != MTL::LoadActionClear) {
                dsv_info.StencilLoadAction = MTL::LoadActionClear;
                dsv_info.ClearStencil = previous_clearpass->depth_stencil.stencil;
                previous_clearpass->depth_stencil_flags &= ~D3D11_CLEAR_STENCIL;
              }
              if (previous_clearpass->depth_stencil_flags == 0) {
                previous_clearpass->skipped = 1;
                // you think this is unnecessary? not the case in C++!
                previous_clearpass->clear_attachment = {};
              }
            }
          } else {
            for (auto &rtv : rtvs.span()) {
              if (previous_clearpass->clear_attachment == rtv.Texture && rtv.LoadAction != MTL::LoadActionClear) {
                rtv.LoadAction = MTL::LoadActionClear;
                rtv.ClearColor = previous_clearpass->color;
                previous_clearpass->skipped = 1;
                // you think this is unnecessary? not the case in C++!
                previous_clearpass->clear_attachment = {};
                break; // all non-null rtvs are mutually different
              }
            }
          }
          previous_clearpass = previous_clearpass->previous_clearpass;
        }
      };

      auto pass_info = MarkRenderPass();

      vro_state.beginEncoder();

      EmitCommand([rtvs = std::move(rtvs), dsv = std::move(dsv_info), effective_render_target, uav_only,
                   uav_only_render_target_height, uav_only_render_target_width, uav_only_sample_count, pass_info,
                   render_target_array](CommandChunk::context &ctx) {
        auto pool = transfer(NS::AutoreleasePool::alloc()->init());
        auto renderPassDescriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
        for (auto &rtv : rtvs.span()) {
          if (rtv.PixelFormat == MTL::PixelFormatInvalid) {
            continue;
          }
          auto colorAttachment = renderPassDescriptor->colorAttachments()->object(rtv.RenderTargetIndex);
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
            renderPassDescriptor->setRenderTargetHeight(uav_only_render_target_height);
            renderPassDescriptor->setRenderTargetWidth(uav_only_render_target_width);
            renderPassDescriptor->setDefaultRasterSampleCount(uav_only_sample_count);
          } else {
            D3D11_ASSERT(dsv_planar_flags);
            auto dsv_tex = dsv.Texture.texture(&ctx);
            renderPassDescriptor->setRenderTargetHeight(dsv_tex->height());
            renderPassDescriptor->setRenderTargetWidth(dsv_tex->width());
          }
        }
        if (pass_info->use_visibility_result)
          renderPassDescriptor->setVisibilityResultBuffer(ctx.chk->visibility_result_heap);

        renderPassDescriptor->setRenderTargetArrayLength(render_target_array);
        ctx.render_encoder = ctx.cmdbuf->renderCommandEncoder(renderPassDescriptor);
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
    InvalidateCurrentPass();

    {
      /* Setup ComputeCommandEncoder */
      MarkPass(EncoderKind::Blit);
      EmitCommand([](CommandChunk::context &ctx) { ctx.blit_encoder = ctx.cmdbuf->blitCommandEncoder(); });
    }

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
    state_.ShaderStages[5].ConstantBuffers.set_dirty();
    state_.ShaderStages[5].Samplers.set_dirty();
    state_.ShaderStages[5].SRVs.set_dirty();

    {
      /* Setup ComputeCommandEncoder */
      MarkPass(EncoderKind::Compute);
      EmitCommand([](CommandChunk::context &ctx) {
        ctx.compute_encoder = ctx.cmdbuf->computeCommandEncoder();
        auto [heap, offset] = ctx.chk->inspect_gpu_heap();
        ctx.compute_encoder->setBuffer(heap, 0, 29);
        ctx.compute_encoder->setBuffer(heap, 0, 30);
      });
    }

    cmdbuf_state = CommandBufferState::ComputeEncoderActive;
  }

  template <ShaderType Type>
  ManagedShader
  GetManagedShader() {
    auto ptr = state_.ShaderStages[(UINT)Type].Shader.ptr();
    return ptr ? ptr->GetManagedShader() : nullptr;
  };

  template <bool IndexedDraw>
  void
  InitializeGraphicsPipelineDesc(MTL_GRAPHICS_PIPELINE_DESC &Desc) {
    // TODO: reduce branching
    auto PS = GetManagedShader<ShaderType::Pixel>();
    auto GS = GetManagedShader<ShaderType::Geometry>();
    Desc.VertexShader = GetManagedShader<ShaderType::Vertex>();
    Desc.PixelShader = PS;
    // FIXME: ensure valid state: hull and domain shader none or both are bound
    Desc.HullShader = GetManagedShader<ShaderType::Hull>();
    Desc.DomainShader = GetManagedShader<ShaderType::Domain>();
    if (state_.InputAssembler.InputLayout) {
      Desc.InputLayout = state_.InputAssembler.InputLayout->GetManagedInputLayout();
    } else {
      Desc.InputLayout = nullptr;
    }
    if (auto so_layout =
            com_cast<IMTLD3D11StreamOutputLayout>(state_.ShaderStages[(UINT)ShaderType::Geometry].Shader.ptr())) {
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
    auto HS = GetManagedShader<ShaderType::Hull>();
    if (!HS) {
      return false;
    }
    switch (HS->reflection().Tessellator.OutputPrimitive) {
    case MTL_TESSELLATOR_OUTPUT_LINE:
      ERR("skip isoline tessellation");
      return false;
    case MTL_TESSELLATOR_OUTPUT_POINT:
      ERR("skip point tessellation");
      return false;
    default:
      break;
    }
    if (!state_.ShaderStages[(UINT)ShaderType::Domain].Shader) {
      return false;
    }
    auto GS = GetManagedShader<ShaderType::Geometry>();
    if (GS && GS->reflection().GeometryShader.GSPassThrough == ~0u) {
      ERR("tessellation-geometry pipeline is not supported yet, skip drawcall");
      return false;
    }

    if (!SwitchToRenderEncoder()) {
      return false;
    }

    auto previous_encoder = GetLastEncoder();
    if (previous_encoder->kind == EncoderKind::Render) {
      auto previous_clearpass = (ENCODER_RENDER_INFO *)previous_encoder;
      previous_clearpass->tessellation_pass = 1;
    }

    Com<IMTLCompiledTessellationPipeline> pipeline;

    MTL_GRAPHICS_PIPELINE_DESC pipelineDesc;
    InitializeGraphicsPipelineDesc<IndexedDraw>(pipelineDesc);

    device->CreateTessellationPipeline(&pipelineDesc, &pipeline);

    EmitCommand([pso = std::move(pipeline)](CommandChunk::context &ctx) {
      MTL_COMPILED_TESSELLATION_PIPELINE GraphicsPipeline{};
      pso->GetPipeline(&GraphicsPipeline); // may block
      D3D11_ASSERT(GraphicsPipeline.MeshPipelineState);
      D3D11_ASSERT(GraphicsPipeline.RasterizationPipelineState);
      ctx.tess_mesh_pso = GraphicsPipeline.MeshPipelineState;
      ctx.tess_raster_pso = GraphicsPipeline.RasterizationPipelineState;
      ctx.tess_num_output_control_point_element = GraphicsPipeline.NumControlPointOutputElement;
      ctx.tess_num_output_patch_constant_scalar = GraphicsPipeline.NumPatchConstantOutputScalar;
      ctx.tess_threads_per_patch = GraphicsPipeline.ThreadsPerPatch;
    });

    cmdbuf_state = CommandBufferState::TessellationRenderPipelineReady;

    if (previous_render_pipeline_state != CommandBufferState::TessellationRenderPipelineReady) {
      state_.InputAssembler.VertexBuffers.set_dirty();
      state_.ShaderStages[(UINT)ShaderType::Vertex].ConstantBuffers.set_dirty();
      state_.ShaderStages[(UINT)ShaderType::Vertex].SRVs.set_dirty();
      state_.ShaderStages[(UINT)ShaderType::Vertex].Samplers.set_dirty();
      state_.ShaderStages[(UINT)ShaderType::Domain].ConstantBuffers.set_dirty();
      state_.ShaderStages[(UINT)ShaderType::Domain].SRVs.set_dirty();
      state_.ShaderStages[(UINT)ShaderType::Domain].Samplers.set_dirty();
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
    if (state_.ShaderStages[(UINT)ShaderType::Hull].Shader) {
      return FinalizeTessellationRenderPipeline<IndexedDraw>();
    }
    if (cmdbuf_state == CommandBufferState::RenderPipelineReady)
      return true;
    auto GS = GetManagedShader<ShaderType::Geometry>();
    if (GS) {
      if (GS->reflection().GeometryShader.GSPassThrough == ~0u) {
        ERR("geometry shader is not supported yet, skip drawcall");
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

    EmitCommand([pso = std::move(pipeline)](CommandChunk::context &ctx) {
      MTL_COMPILED_GRAPHICS_PIPELINE GraphicsPipeline{};
      pso->GetPipeline(&GraphicsPipeline); // may block
      D3D11_ASSERT(GraphicsPipeline.PipelineState);
      ctx.render_encoder->setRenderPipelineState(GraphicsPipeline.PipelineState);
    });

    cmdbuf_state = CommandBufferState::RenderPipelineReady;

    if (previous_render_pipeline_state != CommandBufferState::RenderPipelineReady) {
      state_.InputAssembler.VertexBuffers.set_dirty();
      state_.ShaderStages[(UINT)ShaderType::Vertex].ConstantBuffers.set_dirty();
      state_.ShaderStages[(UINT)ShaderType::Vertex].SRVs.set_dirty();
      state_.ShaderStages[(UINT)ShaderType::Vertex].Samplers.set_dirty();
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
      EmitCommand([state, stencil_ref = state_.OutputMerger.StencilRef](CommandChunk::context &ctx) {
        auto encoder = ctx.render_encoder;
        encoder->setDepthStencilState(state->GetDepthStencilState(ctx.dsv_planar_flags));
        encoder->setStencilReferenceValue(stencil_ref);
      });
    }
    if (dirty_state.any(DirtyState::RasterizerState)) {
      IMTLD3D11RasterizerState *state =
          state_.Rasterizer.RasterizerState ? state_.Rasterizer.RasterizerState : default_rasterizer_state;
      EmitCommand([state](CommandChunk::context &ctx) {
        auto &encoder = ctx.render_encoder;
        state->SetupRasterizerState(encoder);
      });
    }
    if (dirty_state.any(DirtyState::BlendFactorAndStencilRef)) {
      EmitCommand([r = state_.OutputMerger.BlendFactor[0], g = state_.OutputMerger.BlendFactor[1],
                   b = state_.OutputMerger.BlendFactor[2], a = state_.OutputMerger.BlendFactor[3],
                   stencil_ref = state_.OutputMerger.StencilRef](CommandChunk::context &ctx) {
        auto &encoder = ctx.render_encoder;
        encoder->setBlendColor(r, g, b, a);
        encoder->setStencilReferenceValue(stencil_ref);
      });
    }
    IMTLD3D11RasterizerState *current_rs =
        state_.Rasterizer.RasterizerState ? state_.Rasterizer.RasterizerState : default_rasterizer_state;
    bool allow_scissor =
        current_rs->IsScissorEnabled() && state_.Rasterizer.NumViewports == state_.Rasterizer.NumScissorRects;
    // FIXME: multiple viewports with one scissor should be allowed?
    if (current_rs->IsScissorEnabled() && state_.Rasterizer.NumViewports > 1 &&
        state_.Rasterizer.NumScissorRects == 1) {
      ERR("FIXME: handle multiple viewports with single scissor rect.");
    }
    if (dirty_state.any(DirtyState::Viewport)) {
      auto viewports = AllocateCommandData<MTL::Viewport>(state_.Rasterizer.NumViewports);
      for (unsigned i = 0; i < state_.Rasterizer.NumViewports; i++) {
        auto &d3dViewport = state_.Rasterizer.viewports[i];
        viewports[i] = {d3dViewport.TopLeftX, d3dViewport.TopLeftY, d3dViewport.Width,
                        d3dViewport.Height,   d3dViewport.MinDepth, d3dViewport.MaxDepth};
      }
      EmitCommand([viewports = std::move(viewports)](CommandChunk::context &ctx) {
        auto &encoder = ctx.render_encoder;
        encoder->setViewports(viewports.data(), viewports.size());
      });
    }
    if (dirty_state.any(DirtyState::Scissors)) {
      auto scissors = AllocateCommandData<MTL::ScissorRect>(
          allow_scissor ? state_.Rasterizer.NumScissorRects : state_.Rasterizer.NumViewports
      );
      if (allow_scissor) {
        for (unsigned i = 0; i < state_.Rasterizer.NumScissorRects; i++) {
          auto &d3dRect = state_.Rasterizer.scissor_rects[i];
          scissors[i] = {
              (UINT)d3dRect.left, (UINT)d3dRect.top, (UINT)d3dRect.right - d3dRect.left,
              (UINT)d3dRect.bottom - d3dRect.top
          };
        }
      } else {
        for (unsigned i = 0; i < state_.Rasterizer.NumViewports; i++) {
          auto &d3dViewport = state_.Rasterizer.viewports[i];
          scissors[i] = {
              (UINT)d3dViewport.TopLeftX, (UINT)d3dViewport.TopLeftY, (UINT)d3dViewport.Width, (UINT)d3dViewport.Height
          };
        }
      }
      EmitCommand([scissors = std::move(scissors)](CommandChunk::context &ctx) {
        auto &encoder = ctx.render_encoder;
        encoder->setScissorRects(scissors.data(), scissors.size());
      });
    }
    if (dirty_state.any(DirtyState::IndexBuffer)) {
      // we should be able to retrieve it from chunk
      if (state_.InputAssembler.IndexBuffer) {
        EmitCommand([buffer = Use(state_.InputAssembler.IndexBuffer)](CommandChunk::context &ctx) {
          ctx.current_index_buffer_ref = buffer.buffer();
        });
      } else {
        EmitCommand([](CommandChunk::context &ctx) { ctx.current_index_buffer_ref = nullptr; });
      }
    }
    auto previous_encoder = GetLastEncoder();
    assert(previous_encoder->kind == EncoderKind::Render);
    auto previous_renderpass = (ENCODER_RENDER_INFO *)previous_encoder;
    if (!active_occlusion_queries.empty()) {
      previous_renderpass->use_visibility_result = 1;
      if (auto next_offset = vro_state.getNextWriteOffset(true); next_offset != ~0uLL) {
        EmitCommand([next_offset](CommandChunk::context &ctx) {
          ctx.render_encoder->setVisibilityResultMode(
              MTL::VisibilityResultModeCounting, (ctx.visibility_offset_base + next_offset) << 3
          );
        });
      }
    } else {
      if (vro_state.getNextWriteOffset(false) != ~0uLL && previous_renderpass->use_visibility_result) {
        EmitCommand([](CommandChunk::context &ctx) {
          ctx.render_encoder->setVisibilityResultMode(MTL::VisibilityResultModeDisabled, 0);
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
  bool
  FinalizeCurrentComputePipeline() {
    if (cmdbuf_state == CommandBufferState::ComputePipelineReady)
      return true;

    SwitchToComputeEncoder();

    auto CS = GetManagedShader<ShaderType::Compute>();

    if (!CS) {
      ERR("Shader not found?");
      return false;
    }
    Com<IMTLCompiledComputePipeline> pipeline;
    MTL_COMPUTE_PIPELINE_DESC desc{CS};
    device->CreateComputePipeline(&desc, &pipeline);

    EmitCommand([pso = std::move(pipeline), tg_size = MTL::Size::Make(
                                                CS->reflection().ThreadgroupSize[0],
                                                CS->reflection().ThreadgroupSize[1], CS->reflection().ThreadgroupSize[2]
                                            )](CommandChunk::context &ctx) {
      MTL_COMPILED_COMPUTE_PIPELINE ComputePipeline;
      pso->GetPipeline(&ComputePipeline); // may block
      ctx.compute_encoder->setComputePipelineState(ComputePipeline.PipelineState);
      ctx.cs_threadgroup_size = tg_size;
    });

    cmdbuf_state = CommandBufferState::ComputePipelineReady;
    return true;
  }

  bool
  PreDispatch() {
    if (!FinalizeCurrentComputePipeline()) {
      return false;
    }
    UploadShaderStageResourceBinding<ShaderType::Compute, false>();
    return true;
  }

  template <typename Fn>
  void
  EmitRenderCommand(Fn &&fn) {
    D3D11_ASSERT(
        cmdbuf_state == CommandBufferState::RenderEncoderActive ||
        cmdbuf_state == CommandBufferState::RenderPipelineReady ||
        cmdbuf_state == CommandBufferState::TessellationRenderPipelineReady
    );
    EmitCommand([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) { std::invoke(fn, ctx.render_encoder.ptr()); });
  };

  template <bool Force = false, typename Fn>
  void
  EmitComputeCommand(Fn &&fn) {
    if (Force) {
      SwitchToComputeEncoder();
    }
    if (cmdbuf_state == CommandBufferState::ComputeEncoderActive ||
        cmdbuf_state == CommandBufferState::ComputePipelineReady) {
      EmitCommand([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) {
        std::invoke(fn, ctx.compute_encoder.ptr(), ctx.cs_threadgroup_size);
      });
    }
  };

  template <bool Force = false, typename Fn>
  void
  EmitComputeCommandChk(Fn &&fn) {
    if (Force) {
      SwitchToComputeEncoder();
    }
    if (cmdbuf_state == CommandBufferState::ComputeEncoderActive ||
        cmdbuf_state == CommandBufferState::ComputePipelineReady) {
      EmitCommand([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) {
        std::invoke(fn, ctx.compute_encoder.ptr(), ctx);
      });
    }
  };

  template <bool Force = false, typename Fn>
  void
  EmitBlitCommand(Fn &&fn, CommandBufferState BlitKind = CommandBufferState::BlitEncoderActive) {
    if (Force) {
      SwitchToBlitEncoder(BlitKind);
    }
    if (cmdbuf_state == BlitKind) {
      EmitCommand([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) { fn(ctx.blit_encoder.ptr(), ctx); });
    }
  };

  void
  UpdateSOTargets() {
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
        EmitRenderCommand([buffer = Use(so_slot0.Buffer)](MTL::RenderCommandEncoder *encoder) {
          encoder->setVertexBuffer(buffer.buffer(), 0, 20);
        });
      } else {
        EmitRenderCommand([buffer = Use(so_slot0.Buffer), offset = so_slot0.Offset](MTL::RenderCommandEncoder *encoder
                          ) { encoder->setVertexBuffer(buffer.buffer(), offset, 20); });
      }
    } else {
      EmitRenderCommand([](MTL::RenderCommandEncoder *encoder) { encoder->setVertexBuffer(nullptr, 0, 20); });
    }
    state_.StreamOutput.Targets.clear_dirty(0);
    if (state_.StreamOutput.Targets.any_dirty()) {
      ERR("UpdateSOTargets: non-zero slot is marked dirty but not bound, "
          "expect problem");
    }
  }

protected:
  std::unordered_set<IMTLD3DOcclusionQuery*> active_occlusion_queries;
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
    IndexBuffer,
    Viewport,
    Scissors,
  };

  Flags<DirtyState> dirty_state = 0;

#pragma endregion

protected:
  D3D11ContextState state_;
  D3D11UserDefinedAnnotation annotation_;
  VisibilityResultOffsetBumpState vro_state;

public:
  MTLD3D11DeviceContextImplBase(MTLD3D11Device *pDevice, ContextInternalState &ctx_state) :
      MTLD3D11DeviceChild(pDevice),
      device(pDevice),
      ctx_state(ctx_state),
      state_(),
      annotation_(this) {
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

template <typename F> class DestructorWrapper {
  F f;
  bool destroyed = false;

public:
  DestructorWrapper(const DestructorWrapper &) = delete;
  DestructorWrapper(DestructorWrapper &&move) : f(std::forward<F>(move.f)), destroyed(move.destroyed) {
    move.destroyed = true;
  };
  DestructorWrapper(F &&f, std::nullptr_t) : f(std::forward<F>(f)) {}
  ~DestructorWrapper() {
    if (!destroyed) {
      std::invoke(f);
      destroyed = true;
    }
  };
};

class MTLD3D11CommandList : public ManagedDeviceChild<ID3D11CommandList> {
public:
  MTLD3D11CommandList(MTLD3D11Device *pDevice, MTLD3D11CommandListPoolBase *pPool, UINT context_flag) :
      ManagedDeviceChild(pDevice),
      context_flag(context_flag),
      cmdlist_pool(pPool),
      monoid(),
      monoid_list(&monoid, nullptr),
      list_end(&monoid_list),
      monoid_event(),
      monoid_list_event(&monoid_event),
      list_end_event(&monoid_list_event),
      last_encoder_info(&init_encoder_info),
      staging_allocator(
          pDevice->GetMTLDevice(), MTL::ResourceOptionCPUCacheModeWriteCombined |
                                       MTL::ResourceHazardTrackingModeUntracked | MTL::ResourceStorageModeShared
      ) {
    cpu_argument_heap = (char *)malloc(kCommandChunkCPUHeapSize);
    cpu_dynamic_heap = (char *)malloc(kCommandChunkCPUHeapSize);
  };

  ~MTLD3D11CommandList() {
    Reset();
    staging_allocator.free_blocks(~0uLL);
    free(cpu_argument_heap);
    free(cpu_dynamic_heap);
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
    staging_allocator.free_blocks(++local_coherence);
    {
      auto cur = monoid_list.next;
      while (cur) {
        assert((uint64_t)cur->value >= (uint64_t)cpu_argument_heap);
        assert((uint64_t)cur->value < ((uint64_t)cpu_argument_heap + cpu_arugment_heap_offset));
        cur->value->~BFunc<CommandChunk::context>();
        cur = cur->next;
      }
    }
    {
      auto cur = monoid_list_event.next;
      while (cur) {
        assert((uint64_t)cur->value >= (uint64_t)cpu_argument_heap);
        assert((uint64_t)cur->value < ((uint64_t)cpu_argument_heap + cpu_arugment_heap_offset));
        cur->value->~BFunc<EventContext>();
        cur = cur->next;
      }
    }

    {
      for (auto bindable : track_bindable)
        bindable->Release();
      for (auto rtv : track_rtv)
        rtv->Release();
      for (auto dsv : track_dsv)
        dsv->Release();
      track_bindable.clear();
      track_rtv.clear();
      track_dsv.clear();
    }
    {
      dynamic_buffer.clear();
      dynamic_buffer_map.clear();
      residency_tracker.clear();
      dynamic_view.clear();
    }
    cpu_arugment_heap_offset = 0;
    cpu_dynamic_heap_offset = 0;
    gpu_arugment_heap_offset = 0;
    monoid_list.next = nullptr;
    list_end = &monoid_list;
    monoid_list_event.next = nullptr;
    list_end_event = &monoid_list_event;
    last_encoder_info = &init_encoder_info;
    encoder_seq_local = 1;

    num_argument_data = 0;
    num_bindingref = 0;
    num_visibility_result = 0;
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

  struct EventContext {
    CommandQueue &cmd_queue;
    MTLD3D11Device *device;
    uint64_t *gpu_argument_heap_contents;
    std::vector<ArgumentData> &argument_table;
    std::vector<BindingRef> &binding_table;
    uint64_t visibility_result_offset;
    std::vector<Com<IMTLD3DOcclusionQuery>> &pending_occlusion_queries;
  };

  template <typename Fn>
  void
  EmitEvent(Fn &&fn) {
    using TT = CommandChunk::EFunc<EventContext, Fn>;
    using NodeT = CommandChunk::Node<CommandChunk::BFunc<EventContext> *>;
    auto ptr = allocate_cpu_heap<TT>();
    new (ptr) TT(std::forward<Fn>(fn));
    auto ptr_node = allocate_cpu_heap<NodeT>();
    *ptr_node = {ptr, nullptr};
    list_end_event->next = ptr_node;
    list_end_event = ptr_node;
  }

  template <typename Fn>
  void
  EmitCommand(Fn &&fn) {
    using TT = CommandChunk::EFunc<CommandChunk::context, Fn>;
    using NodeT = CommandChunk::Node<CommandChunk::BFunc<CommandChunk::context> *>;
    auto ptr = allocate_cpu_heap<TT>();
    new (ptr) TT(std::forward<Fn>(fn));
    auto ptr_node = allocate_cpu_heap<NodeT>();
    *ptr_node = {ptr, nullptr};
    list_end->next = ptr_node;
    list_end = ptr_node;
  }

  BindingRef
  Use(IMTLBindable *bindable) {
    auto immediate = bindable->UseBindable(0);
    if (immediate) {
      if (track_bindable.insert(bindable).second) {
        bindable->AddRef();
      }
      return immediate;
    } else {
      auto index = num_bindingref++;
      EmitEvent([index, bindable](EventContext &ctx) {
        ctx.binding_table[index] = bindable->UseBindable(ctx.cmd_queue.CurrentSeqId());
      });
      Com<IMTLDynamicBuffer> dynamic;
      bindable->GetLogicalResourceOrView(IID_PPV_ARGS(&dynamic));
      auto& set = dynamic_view[dynamic.ptr()];
      set.insert(bindable);
      return BindingRef(index, deferred_binding_t{});
    }
  }

  BindingRef
  Use(IMTLD3D11RenderTargetView *rtv) {
    auto immediate = rtv->UseBindable(0);
    if (immediate) {
      if (track_rtv.insert(rtv).second) {
        rtv->AddRef();
      }
      return immediate;
    } else {
      D3D11_ASSERT(0 && "Unhandled dynamic RTV");
    }
  }

  BindingRef
  Use(IMTLD3D11DepthStencilView *dsv) {
    auto immediate = dsv->UseBindable(0);
    if (immediate) {
      if (track_dsv.insert(dsv).second) {
        dsv->AddRef();
      }
      return immediate;
    } else {
      D3D11_ASSERT(0 && "Unhandled dynamic DSV");
    }
  }

  BindingRef
  Use(IMTLDynamicBuffer *dynamic_buffer) {
    auto index = num_bindingref++;
    EmitEvent([index, dynamic_buffer = Com(dynamic_buffer)](EventContext &ctx) {
      ctx.binding_table[index] = dynamic_buffer->GetCurrentBufferBinding();
    });
    return BindingRef(index, deferred_binding_t{});
  }

  ENCODER_INFO *
  GetLastEncoder() {
    return last_encoder_info;
  }
  ENCODER_CLEARPASS_INFO *
  MarkClearPass() {
    dynamic_view.clear();
    residency_tracker.clear();
    auto ptr = allocate_cpu_heap<ENCODER_CLEARPASS_INFO>();
    new (ptr) ENCODER_CLEARPASS_INFO();
    ptr->encoder_id = encoder_seq_local++;
    last_encoder_info = (ENCODER_INFO *)ptr;
    return ptr;
  }
  ENCODER_RENDER_INFO *
  MarkRenderPass() {
    dynamic_view.clear();
    residency_tracker.clear();
    auto ptr = allocate_cpu_heap<ENCODER_RENDER_INFO>();
    new (ptr) ENCODER_RENDER_INFO();
    ptr->encoder_id = encoder_seq_local++;
    last_encoder_info = (ENCODER_INFO *)ptr;
    return ptr;
  }
  ENCODER_INFO *
  MarkPass(EncoderKind kind) {
    dynamic_view.clear();
    residency_tracker.clear();
    auto ptr = allocate_cpu_heap<ENCODER_INFO>();
    ptr->kind = kind;
    ptr->encoder_id = encoder_seq_local++;
    last_encoder_info = ptr;
    return ptr;
  }

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

  void *
  AllocateDynamicTempBuffer(IMTLDynamicBuffer *token, size_t size) {
    auto ptr = cpu_dynamic_heap + cpu_dynamic_heap_offset;
    cpu_dynamic_heap_offset+=size;
    if (cpu_dynamic_heap_offset >= kCommandChunkCPUHeapSize) {
      ERR(cpu_dynamic_heap_offset, " - cpu dynamic heap overflow, expect error.");
    }
    dynamic_buffer.push_back(ptr);
    dynamic_buffer_map[token] = ptr;
    for (auto view : dynamic_view[token]) {
      // insert doesn't hurt
      residency_tracker[view] = {};
    }
    return ptr;
  };

  void *
  GetDynamicTempBuffer(IMTLDynamicBuffer *token) {
    if (dynamic_buffer_map.contains(token)) {
      return dynamic_buffer_map.at(token);
    }
    return nullptr;
  };

  uint32_t
  GetArgumentDataId(IMTLBindable *bindable, SIMPLE_RESIDENCY_TRACKER **tracker) {
    auto index = num_argument_data++;
    EmitEvent([index, bindable = Com(bindable)](EventContext &ctx) {
      SIMPLE_RESIDENCY_TRACKER *_;
      ctx.argument_table[index] = bindable->GetArgumentData(&_);
    });
    *tracker = &residency_tracker.insert({bindable, {}}).first->second;
    return index;
  };

  uint64_t
  ReserveGpuHeap(uint64_t size, uint64_t alignment) {
    std::size_t adjustment = align_forward_adjustment((void *)gpu_arugment_heap_offset, alignment);
    auto aligned = gpu_arugment_heap_offset + adjustment;
    gpu_arugment_heap_offset = aligned + size;
    return aligned;
  };

#pragma endregion

#pragma region ImmediateContext-related

  bool
  Noop() {
    return cpu_arugment_heap_offset == 0 && gpu_arugment_heap_offset == 0;
  };

  void
  ProcessEvents(EventContext &ctx) {
    {
      for (auto bindable : track_bindable)
        bindable->UseBindable(ctx.cmd_queue.CurrentSeqId());
      for (auto rtv : track_rtv)
        rtv->UseBindable(ctx.cmd_queue.CurrentSeqId());
      for (auto dsv : track_dsv)
        dsv->UseBindable(ctx.cmd_queue.CurrentSeqId());
    }

    auto cur = monoid_list_event.next;
    while (cur) {
      assert((uint64_t)cur->value >= (uint64_t)cpu_argument_heap);
      assert((uint64_t)cur->value < ((uint64_t)cpu_argument_heap + cpu_arugment_heap_offset));
      cur->value->invoke(ctx);
      cur = cur->next;
    }
  }

  void
  EncodeCommands(CommandChunk::context &ctx) {
    auto cur = monoid_list.next;
    while (cur) {
      assert((uint64_t)cur->value >= (uint64_t)cpu_argument_heap);
      assert((uint64_t)cur->value < ((uint64_t)cpu_argument_heap + cpu_arugment_heap_offset));
      cur->value->invoke(ctx);
      cur = cur->next;
    }
  }

#pragma endregion

  uint32_t num_argument_data = 0;
  uint32_t num_bindingref = 0;
  uint64_t gpu_arugment_heap_offset = 0;
  uint64_t num_visibility_result = 0;
  bool promote_flush = false;

private:
  UINT context_flag;
  MTLD3D11CommandListPoolBase *cmdlist_pool;

  uint64_t cpu_arugment_heap_offset = 0;
  char *cpu_argument_heap;
  char *cpu_dynamic_heap;
  uint64_t cpu_dynamic_heap_offset = 0;

  CommandChunk::MFunc<CommandChunk::context> monoid;
  CommandChunk::Node<CommandChunk::BFunc<CommandChunk::context> *> monoid_list;
  CommandChunk::Node<CommandChunk::BFunc<CommandChunk::context> *> *list_end;

  CommandChunk::MFunc<EventContext> monoid_event;
  CommandChunk::Node<CommandChunk::BFunc<EventContext> *> monoid_list_event;
  CommandChunk::Node<CommandChunk::BFunc<EventContext> *> *list_end_event;

  ENCODER_INFO init_encoder_info{EncoderKind::Nil, 0};
  ENCODER_INFO *last_encoder_info;
  uint64_t encoder_seq_local = 1;

  std::unordered_set<IMTLBindable *> track_bindable;
  std::unordered_set<IMTLD3D11RenderTargetView *> track_rtv;
  std::unordered_set<IMTLD3D11DepthStencilView *> track_dsv;

  std::vector<void *> dynamic_buffer;
  std::unordered_map<IMTLDynamicBuffer *, void *> dynamic_buffer_map;

  std::unordered_map<IMTLBindable *, SIMPLE_RESIDENCY_TRACKER> residency_tracker;

  std::unordered_map<IMTLDynamicBuffer *, std::unordered_set<IMTLBindable *>> dynamic_view;

  RingBumpAllocator<true> staging_allocator;

  uint64_t local_coherence = 0;
};

} // namespace dxmt