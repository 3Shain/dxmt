

#include "d3d11_context.hpp"
#include "d3d11_enumerable.hpp"
#include "d3d11_private.h"
#include "d3d11_query.hpp"
#include "dxmt_command_queue.hpp"

#include "./d3d11_context_internal.cpp"

namespace dxmt {
template <typename F> class DestructorWrapper {
  F f;
  bool destroyed = false;

public:
  DestructorWrapper(const DestructorWrapper &) = delete;
  DestructorWrapper(DestructorWrapper &&move)
      : f(std::forward<F>(move.f)), destroyed(move.destroyed) {
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

auto to_metal_topology(D3D11_PRIMITIVE_TOPOLOGY topo) {

  switch (topo) {
  case D3D_PRIMITIVE_TOPOLOGY_POINTLIST:
    return MTL::PrimitiveTypePoint;
  case D3D_PRIMITIVE_TOPOLOGY_LINELIST:
    return MTL::PrimitiveTypeLine;
  case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP:
    return MTL::PrimitiveTypeLineStrip;
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST:
    return MTL::PrimitiveTypeTriangle;
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP:
    return MTL::PrimitiveTypeTriangleStrip;
  case D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ:
  case D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ:
    // FIXME
    return MTL::PrimitiveTypePoint;
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
    return MTL::PrimitiveTypePoint;
  case D3D_PRIMITIVE_TOPOLOGY_UNDEFINED:
    throw MTLD3DError("Invalid topology");
  }
}

class MTLD3D11DeviceContext : public MTLD3D11DeviceContextBase {
public:
  HRESULT QueryInterface(REFIID riid, void **ppvObject) override {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11DeviceContext) ||
        riid == __uuidof(ID3D11DeviceContext1) ||
        riid == __uuidof(ID3D11DeviceContext2) ||
        riid == __uuidof(ID3D11DeviceContext3) ||
        riid == __uuidof(ID3D11DeviceContext4) ||
        riid == __uuidof(IMTLD3D11DeviceContext)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IMTLD3D11DeviceContext), riid)) {
      WARN("D3D11DeviceContext: Unknown interface query ", str::format(riid));
    }
    return E_NOINTERFACE;
  }

  void Begin(ID3D11Asynchronous *pAsync) override {
    // in theory pAsync could be any of them: { Query, Predicate, Counter }.
    // However `Predicate` and `Counter` are not supported at all
    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT:
      break;
    case D3D11_QUERY_OCCLUSION: {
      if (auto observer = ((IMTLD3DOcclusionQuery *)pAsync)
                              ->Begin(ctx.NextOcclusionQuerySeq())) {
        cmd_queue.RegisterVisibilityResultObserver(observer);
      }
      break;
    }
    case D3D11_QUERY_TIMESTAMP:
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      // ignore
      break;
    }
    default:
      ERR("Unknown query type ", desc.Query);
      break;
    }
  }

  // See Begin()
  void End(ID3D11Asynchronous *pAsync) override {
    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT:
      ((IMTLD3DEventQuery *)pAsync)->Issue(cmd_queue.EncodedWorkFinishAt());
      break;
    case D3D11_QUERY_OCCLUSION: {
      ((IMTLD3DOcclusionQuery *)pAsync)->End(ctx.NextOcclusionQuerySeq());
      break;
    }
    case D3D11_QUERY_TIMESTAMP:
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      // ignore
      break;
    }
    default:
      ERR("Unknown query type ", desc.Query);
      break;
    }
  }

  HRESULT GetData(ID3D11Asynchronous *pAsync, void *pData, UINT DataSize,
                  UINT GetDataFlags) override {
    if (!pAsync || (DataSize && !pData))
      return E_INVALIDARG;

    // Allow dataSize to be zero
    if (DataSize && DataSize != pAsync->GetDataSize())
      return E_INVALIDARG;

    if (GetDataFlags != D3D11_ASYNC_GETDATA_DONOTFLUSH) {
      Flush();
      ctx.InvalidateCurrentPass();
    }

    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT: {
      BOOL null_data;
      BOOL *data_ptr = pData ? (BOOL *)pData : &null_data;
      return ((IMTLD3DEventQuery *)pAsync)->GetData(data_ptr, cmd_queue.CoherentSeqId());
    }
    case D3D11_QUERY_OCCLUSION: {
      uint64_t null_data;
      uint64_t *data_ptr = pData ? (uint64_t *)pData : &null_data;
      return ((IMTLD3DOcclusionQuery *)pAsync)->GetData(data_ptr);
    }
    case D3D11_QUERY_TIMESTAMP: {
      if (pData) {
        (*static_cast<uint64_t *>(pData)) = 0;
      }
      return S_OK;
    }
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      if (pData) {
        (*static_cast<D3D11_QUERY_DATA_TIMESTAMP_DISJOINT *>(pData)) = {1,
                                                                        TRUE};
      }
      return S_OK;
    }
    default:
      ERR("Unknown query type ", desc.Query);
      return E_FAIL;
    }
  }

  HRESULT Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType,
              UINT MapFlags,
              D3D11_MAPPED_SUBRESOURCE *pMappedResource) override {
    if (auto dynamic = com_cast<IMTLDynamicBuffer>(pResource)) {
      D3D11_MAPPED_SUBRESOURCE Out;
      switch (MapType) {
      case D3D11_MAP_READ:
      case D3D11_MAP_WRITE:
      case D3D11_MAP_READ_WRITE:
        return E_INVALIDARG;
      case D3D11_MAP_WRITE_DISCARD: {
        dynamic->RotateBuffer(this);
        Out.pData = dynamic->GetMappedMemory(&Out.RowPitch, &Out.DepthPitch);
        break;
      }
      case D3D11_MAP_WRITE_NO_OVERWRITE: {
        Out.pData = dynamic->GetMappedMemory(&Out.RowPitch, &Out.DepthPitch);
        break;
      }
      }
      if (pMappedResource) {
        *pMappedResource = Out;
      }
      return S_OK;
    }
    if (auto staging = com_cast<IMTLD3D11Staging>(pResource)) {
      while (true) {
        auto coh = cmd_queue.CoherentSeqId();
        auto ret = staging->TryMap(Subresource, coh, MapType, pMappedResource);
        if (ret < 0) {
          return E_FAIL;
        }
        if (ret == 0) {
          TRACE("staging map ready");
          return S_OK;
        }
        if (MapFlags & D3D11_MAP_FLAG_DO_NOT_WAIT) {
          return DXGI_ERROR_WAS_STILL_DRAWING;
        }
        // FIXME: bugprone
        if (ret + coh == cmd_queue.CurrentSeqId()) {
          TRACE("Map: forced flush");
          Flush();
          ctx.InvalidateCurrentPass();
        }
        TRACE("staging map block");
        cmd_queue.FIXME_YieldUntilCoherenceBoundaryUpdate(coh);
      };
    };
    D3D11_ASSERT(0 && "unknown mapped resource (USAGE_DEFAULT?)");
    IMPLEMENT_ME;
  }

  void Unmap(ID3D11Resource *pResource, UINT Subresource) override {
    if (auto dynamic = com_cast<IMTLDynamicBuffer>(pResource)) {
      return;
    }
    if (auto staging = com_cast<IMTLD3D11Staging>(pResource)) {
      staging->Unmap(Subresource);
      return;
    };
    D3D11_ASSERT(0 && "unknown mapped resource (USAGE_DEFAULT?)");
    IMPLEMENT_ME;
  }

  void Flush() override {
    if (cmd_queue.CurrentChunk()->has_no_work_encoded_yet()) {
      return;
    }
    ctx.promote_flush = true;
  }

  void ExecuteCommandList(ID3D11CommandList *pCommandList,
                          BOOL RestoreContextState) override{IMPLEMENT_ME}

  HRESULT FinishCommandList(BOOL RestoreDeferredContextState,
                            ID3D11CommandList **ppCommandList) override {
    return DXGI_ERROR_INVALID_CALL;
  }

  void SetResourceMinLOD(ID3D11Resource *pResource, FLOAT MinLOD) override {
    // FIXME: `min_lod_clamp` can do this but it's in the shader
    ERR_ONCE("Not implemented");
  }

  FLOAT GetResourceMinLOD(ID3D11Resource *pResource) override {
    ERR_ONCE("Not implemented");
    return 0.0f;
  }

#pragma region Resource Manipulation

  void ClearRenderTargetView(ID3D11RenderTargetView *pRenderTargetView,
                             const FLOAT ColorRGBA[4]) override {
    if (auto expected =
            com_cast<IMTLD3D11RenderTargetView>(pRenderTargetView)) {
      ctx.ClearRenderTargetView(expected.ptr(), ColorRGBA);
    }
  }

  void
  ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView *pUnorderedAccessView,
                               const UINT Values[4]) override {
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
    pUnorderedAccessView->GetDesc(&desc);
    Com<IMTLBindable> bindable;
    pUnorderedAccessView->QueryInterface(IID_PPV_ARGS(&bindable));
    auto value =
        std::array<uint32_t, 4>({Values[0], Values[1], Values[2], Values[3]});
    switch (desc.ViewDimension) {
    case D3D11_UAV_DIMENSION_UNKNOWN:
      return;
    case D3D11_UAV_DIMENSION_BUFFER: {
      if (desc.Buffer.Flags & D3D11_BUFFER_UAV_FLAG_RAW) {
        ctx.EmitComputeCommandChk<true>(
            [buffer = bindable->UseBindable(cmd_queue.CurrentSeqId()),
             value](auto encoder, auto &ctx) {
              ctx.queue->clear_cmd.ClearBufferUint(encoder, buffer.buffer(),
                                                   buffer.offset(),
                                                   buffer.width() >> 2, value);
            });
      } else {
        if (desc.Format == DXGI_FORMAT_UNKNOWN) {
          ctx.EmitComputeCommandChk<true>(
              [buffer = bindable->UseBindable(cmd_queue.CurrentSeqId()),
               value](auto encoder, auto &ctx) {
                ctx.queue->clear_cmd.ClearBufferUint(
                    encoder, buffer.buffer(), buffer.offset(),
                    buffer.width() >> 2, value);
              });
        } else {
          ctx.EmitComputeCommandChk<true>(
              [tex = bindable->UseBindable(cmd_queue.CurrentSeqId()),
               value](auto encoder, auto &ctx) {
                ctx.queue->clear_cmd.ClearTextureBufferUint(
                    encoder, tex.texture(&ctx), value);
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
      ctx.EmitComputeCommandChk<true>(
          [tex = bindable->UseBindable(cmd_queue.CurrentSeqId()),
           value](auto encoder, auto &ctx) {
            ctx.queue->clear_cmd.ClearTexture2DUint(encoder, tex.texture(&ctx),
                                                    value);
          });
      break;
    case D3D11_UAV_DIMENSION_TEXTURE2DARRAY:
      D3D11_ASSERT(0 && "tex2darr clear");
      break;
    case D3D11_UAV_DIMENSION_TEXTURE3D:
      D3D11_ASSERT(0 && "tex3d clear");
      break;
    }
  }

  void
  ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView *pUnorderedAccessView,
                                const FLOAT Values[4]) override {
    IMPLEMENT_ME
  }

  void ClearDepthStencilView(ID3D11DepthStencilView *pDepthStencilView,
                             UINT ClearFlags, FLOAT Depth,
                             UINT8 Stencil) override {
    if (auto expected =
            com_cast<IMTLD3D11DepthStencilView>(pDepthStencilView)) {
      ctx.ClearDepthStencilView(expected.ptr(), ClearFlags, Depth, Stencil);
    }
  }

  void ClearView(ID3D11View *pView, const FLOAT Color[4],
                 const D3D11_RECT *pRect, UINT NumRects) override {
    IMPLEMENT_ME
  }

  void GenerateMips(ID3D11ShaderResourceView *pShaderResourceView) override {
    D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    pShaderResourceView->GetDesc(&desc);
    if (desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER ||
        desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX) {
      return;
    }
    if (desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D) {
      if (auto com = com_cast<IMTLBindable>(pShaderResourceView)) {
        ctx.EmitBlitCommand<true>(
            [tex = com->UseBindable(cmd_queue.CurrentSeqId())](
                MTL::BlitCommandEncoder *enc, CommandChunk::context &ctx) {
              enc->generateMipmaps(tex.texture(&ctx));
            });
      } else {
        // FIXME: any other possible case?
        D3D11_ASSERT(0 && "unhandled genmips");
      }
      return;
    }
    IMPLEMENT_ME
  }

  void ResolveSubresource(ID3D11Resource *pDstResource, UINT DstSubresource,
                          ID3D11Resource *pSrcResource, UINT SrcSubresource,
                          DXGI_FORMAT Format) override {
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
    if (desc.SampleDesc.Count < 2) {
      ERR("ResolveSubresource: Source is not multisampled ", desc.SampleDesc.Count);
      return;
    }
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
    if (width != std::max(1u, desc.Width >> dst_level) ||
        height != std::max(1u, desc.Height >> dst_level)) {
      ERR("ResolveSubresource: Size doesn't match");
      return;
    }
    if (auto dst = com_cast<IMTLBindable>(pDstResource)) {
      if (auto src = com_cast<IMTLBindable>(pSrcResource)) {
        ctx.ResolveSubresource(src.ptr(), SrcSubresource, dst.ptr(), dst_level,
                               dst_slice);
      }
    }
  }

  void CopyResource(ID3D11Resource *pDstResource,
                    ID3D11Resource *pSrcResource) override {
    D3D11_RESOURCE_DIMENSION dst_dim, src_dim;
    pDstResource->GetType(&dst_dim);
    pSrcResource->GetType(&src_dim);
    if (dst_dim != src_dim)
      return;
    switch (dst_dim) {
    case D3D11_RESOURCE_DIMENSION_UNKNOWN:
      break;
    case D3D11_RESOURCE_DIMENSION_BUFFER: {
      ctx.CopyBuffer((ID3D11Buffer *)pDstResource, 0, 0, 0, 0,
                     (ID3D11Buffer *)pSrcResource, 0, nullptr);
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
      IMPLEMENT_ME
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
      D3D11_TEXTURE2D_DESC desc;
      ((ID3D11Texture2D *)pSrcResource)->GetDesc(&desc);
      for (auto sub : EnumerateSubresources(desc)) {
        ctx.CopyTexture2D((ID3D11Texture2D *)pDstResource, sub.SubresourceId, 0,
                          0, 0, (ID3D11Texture2D *)pSrcResource,
                          sub.SubresourceId, nullptr);
      }
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
      IMPLEMENT_ME
      break;
    }
    }
  }

  void CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset,
                          ID3D11UnorderedAccessView *pSrcView) override {
    IMPLEMENT_ME
  }

  void CopySubresourceRegion(ID3D11Resource *pDstResource, UINT DstSubresource,
                             UINT DstX, UINT DstY, UINT DstZ,
                             ID3D11Resource *pSrcResource, UINT SrcSubresource,
                             const D3D11_BOX *pSrcBox) override {
    CopySubresourceRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ,
                           pSrcResource, SrcSubresource, pSrcBox, 0);
  }

  void CopySubresourceRegion1(ID3D11Resource *pDstResource, UINT DstSubresource,
                              UINT DstX, UINT DstY, UINT DstZ,
                              ID3D11Resource *pSrcResource, UINT SrcSubresource,
                              const D3D11_BOX *pSrcBox,
                              UINT CopyFlags) override {
    D3D11_RESOURCE_DIMENSION dst_dim, src_dim;
    pDstResource->GetType(&dst_dim);
    pSrcResource->GetType(&src_dim);
    if (dst_dim != src_dim)
      return;
    switch (dst_dim) {
    case D3D11_RESOURCE_DIMENSION_UNKNOWN: {
      break;
    }
    case D3D11_RESOURCE_DIMENSION_BUFFER: {
      ctx.CopyBuffer((ID3D11Buffer *)pDstResource, DstSubresource, DstX, DstY,
                     DstZ, (ID3D11Buffer *)pSrcResource, SrcSubresource,
                     pSrcBox);
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D: {
      ctx.CopyTexture1D((ID3D11Texture1D *)pDstResource, DstSubresource, DstX,
                        DstY, DstZ, (ID3D11Texture1D *)pSrcResource,
                        SrcSubresource, pSrcBox);
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D: {
      ctx.CopyTexture2D((ID3D11Texture2D *)pDstResource, DstSubresource, DstX,
                        DstY, DstZ, (ID3D11Texture2D *)pSrcResource,
                        SrcSubresource, pSrcBox);
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
      D3D11_ASSERT(0 && "TODO: CopySubresourceRegion1 for tex3d");
      break;
    }
    }
  }

  void UpdateSubresource(ID3D11Resource *pDstResource, UINT DstSubresource,
                         const D3D11_BOX *pDstBox, const void *pSrcData,
                         UINT SrcRowPitch, UINT SrcDepthPitch) override {
    UpdateSubresource1(pDstResource, DstSubresource, pDstBox, pSrcData,
                       SrcRowPitch, SrcDepthPitch, 0);
  }

  void UpdateSubresource1(ID3D11Resource *pDstResource, UINT DstSubresource,
                          const D3D11_BOX *pDstBox, const void *pSrcData,
                          UINT SrcRowPitch, UINT SrcDepthPitch,
                          UINT CopyFlags) override {
    if (!pDstResource)
      return;
    if (pDstBox != NULL) {
      if (pDstBox->right <= pDstBox->left || pDstBox->bottom <= pDstBox->top ||
          pDstBox->back <= pDstBox->front) {
        return;
      }
    }
    D3D11_RESOURCE_DIMENSION dim;
    pDstResource->GetType(&dim);
    if (dim == D3D11_RESOURCE_DIMENSION_BUFFER) {
      D3D11_BUFFER_DESC desc;
      ((ID3D11Buffer *)pDstResource)->GetDesc(&desc);
      uint32_t copy_offset = 0;
      uint32_t copy_len = desc.ByteWidth;
      if (pDstBox) {
        copy_offset = pDstBox->left;
        copy_len = pDstBox->right - copy_offset;
      }

      if (auto bindable = com_cast<IMTLBindable>(pDstResource)) {
        if (!bindable->GetContentionState(cmd_queue.CoherentSeqId())) {
          auto _ = bindable->UseBindable(cmd_queue.CurrentSeqId());
          auto buffer = _.buffer();
          memcpy(((char *)buffer->contents()) + copy_offset, pSrcData,
                 copy_len);
          buffer->didModifyRange(NS::Range::Make(copy_offset, copy_len));
          return;
        }
        auto [ptr, staging_buffer, offset] =
            cmd_queue.AllocateStagingBuffer(copy_len, 16);
        memcpy(ptr, pSrcData, copy_len);
        ctx.EmitBlitCommand<true>(
            [staging_buffer, offset,
             dst = bindable->UseBindable(cmd_queue.CurrentSeqId()), copy_offset,
             copy_len](MTL::BlitCommandEncoder *enc, auto &ctx) {
              enc->copyFromBuffer(staging_buffer, offset, dst.buffer(),
                                  copy_offset, copy_len);
            });
      } else if (auto dynamic = com_cast<IMTLDynamicBindable>(pDstResource)) {
        D3D11_ASSERT(CopyFlags && "otherwise resource cannot be dynamic");
        D3D11_ASSERT(0 && "UpdateSubresource1: TODO");
      } else {
        D3D11_ASSERT(0 && "UpdateSubresource1: TODO: staging?");
      }
      return;
    }
    if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
      D3D11_TEXTURE2D_DESC desc;
      ((ID3D11Texture2D *)pDstResource)->GetDesc(&desc);
      if (DstSubresource >= desc.MipLevels * desc.ArraySize) {
        ERR("out of bound texture write");
        return;
      }
      auto slice = DstSubresource / desc.MipLevels;
      auto level = DstSubresource % desc.MipLevels;
      uint32_t copy_rows = std::max(desc.Height >> level, 1u);
      uint32_t copy_columns = std::max(desc.Width >> level, 1u);
      uint32_t origin_x = 0;
      uint32_t origin_y = 0;
      if (pDstBox) {
        copy_rows = pDstBox->bottom - pDstBox->top;
        copy_columns = pDstBox->right - pDstBox->left;
        origin_x = pDstBox->left;
        origin_y = pDstBox->top;
      }
      if (auto bindable = com_cast<IMTLBindable>(pDstResource)) {
        while (!bindable->GetContentionState(cmd_queue.CoherentSeqId())) {
          auto dst = bindable->UseBindable(cmd_queue.CurrentSeqId());
          auto texture = dst.texture();
          if (!texture)
            break;
          texture->replaceRegion(
              MTL::Region::Make2D(origin_x, origin_y, copy_columns, copy_rows),
              level, slice, pSrcData, SrcRowPitch, 0);
          return;
        }
        auto copy_len = copy_rows * SrcRowPitch;
        auto [ptr, staging_buffer, offset] =
            cmd_queue.AllocateStagingBuffer(copy_len, 16);
        memcpy(ptr, pSrcData, copy_len);
        ctx.EmitBlitCommand<true>(
            [staging_buffer, offset,
             dst = bindable->UseBindable(cmd_queue.CurrentSeqId()), SrcRowPitch,
             copy_rows, copy_columns, origin_x, origin_y, slice,
             level](MTL::BlitCommandEncoder *enc, auto &ctx) {
              enc->copyFromBuffer(staging_buffer, offset, SrcRowPitch, 0,
                                  MTL::Size::Make(copy_columns, copy_rows, 1),
                                  dst.texture(&ctx), slice, level,
                                  MTL::Origin::Make(origin_x, origin_y, 0));
            });
      } else if (auto dynamic = com_cast<IMTLDynamicBindable>(pDstResource)) {
        D3D11_ASSERT(CopyFlags && "otherwise resource cannot be dynamic");
        D3D11_ASSERT(0 && "UpdateSubresource1: TODO");
      } else {
        // staging: ...
        D3D11_ASSERT(0 && "UpdateSubresource1: TODO: texture2d");
      }
      return;
    }
    if (dim == D3D11_RESOURCE_DIMENSION_TEXTURE3D) {
      D3D11_TEXTURE3D_DESC desc;
      ((ID3D11Texture3D *)pDstResource)->GetDesc(&desc);
      if (DstSubresource >= desc.MipLevels) {
        ERR("out of bound texture write");
        return;
      }
      auto level = DstSubresource % desc.MipLevels;
      uint32_t copy_rows = std::max(desc.Height >> level, 1u);
      uint32_t copy_columns = std::max(desc.Width >> level, 1u);
      uint32_t copy_w = std::max(desc.Depth >> level, 1u);
      uint32_t origin_x = 0;
      uint32_t origin_y = 0;
      uint32_t origin_z = 0;
      if (pDstBox) {
        copy_rows = pDstBox->bottom - pDstBox->top;
        copy_columns = pDstBox->right - pDstBox->left;
        copy_w = pDstBox->back - pDstBox->front;
        origin_x = pDstBox->left;
        origin_y = pDstBox->top;
        origin_z = pDstBox->front;
      }
      if (auto bindable = com_cast<IMTLBindable>(pDstResource)) {
        while (!bindable->GetContentionState(cmd_queue.CoherentSeqId())) {
          auto dst = bindable->UseBindable(cmd_queue.CurrentSeqId());
          auto texture = dst.texture();
          if (!texture)
            break;
          texture->replaceRegion(
              MTL::Region::Make3D(origin_x, origin_y, origin_z, copy_columns,
                                  copy_rows, copy_w),
              level, 0, pSrcData, SrcRowPitch, SrcDepthPitch);
          return;
        }
        auto copy_len = copy_w * SrcDepthPitch;
        auto [ptr, staging_buffer, offset] =
            cmd_queue.AllocateStagingBuffer(copy_len, 16);
        memcpy(ptr, pSrcData, copy_len);
        ctx.EmitBlitCommand<true>(
            [staging_buffer, offset,
             dst = bindable->UseBindable(cmd_queue.CurrentSeqId()), SrcRowPitch,
             SrcDepthPitch, copy_rows, copy_columns, copy_w, origin_x, origin_y,
             origin_z, level](MTL::BlitCommandEncoder *enc, auto &ctx) {
              enc->copyFromBuffer(
                  staging_buffer, offset, SrcRowPitch, SrcDepthPitch,
                  MTL::Size::Make(copy_columns, copy_rows, copy_w),
                  dst.texture(&ctx), 0, level,
                  MTL::Origin::Make(origin_x, origin_y, origin_z));
            });
      } else if (auto dynamic = com_cast<IMTLDynamicBindable>(pDstResource)) {
        D3D11_ASSERT(CopyFlags && "otherwise resource cannot be dynamic");
        D3D11_ASSERT(0 && "UpdateSubresource1: TODO");
      } else {
        // staging: ...
        D3D11_ASSERT(0 && "UpdateSubresource1: TODO: texture2d");
      }
      return;
    }

    IMPLEMENT_ME
  }

  void DiscardResource(ID3D11Resource *pResource) override {
    /*
    All the Discard* API is not implemented and that's probably fine (as it's
    more like a hint of optimization, and Metal manages resources on its own)
    FIXME: for render targets we can use this information: LoadActionDontCare
    FIXME: A Map with D3D11_MAP_WRITE type could become D3D11_MAP_WRITE_DISCARD?
    */
    ERR_ONCE("Not implemented");
  }

  void DiscardView(ID3D11View *pResourceView) override {
    DiscardView1(pResourceView, 0, 0);
  }

  void DiscardView1(ID3D11View *pResourceView, const D3D11_RECT *pRects,
                    UINT NumRects) override {
    ERR_ONCE("Not implemented");
  }
#pragma endregion

#pragma region DrawCall

  void Draw(UINT VertexCount, UINT StartVertexLocation) override {
    if (!ctx.PreDraw())
      return;
    MTL::PrimitiveType Primitive =
        to_metal_topology(state_.InputAssembler.Topology);
    // TODO: skip invalid topology
    ctx.EmitRenderCommand([Primitive, StartVertexLocation,
                           VertexCount](MTL::RenderCommandEncoder *encoder) {
      encoder->drawPrimitives(Primitive, StartVertexLocation, VertexCount);
    });
  }

  void DrawIndexed(UINT IndexCount, UINT StartIndexLocation,
                   INT BaseVertexLocation) override {
    if (!ctx.PreDraw())
      return;
    MTL::PrimitiveType Primitive =
        to_metal_topology(state_.InputAssembler.Topology);
    auto IndexType =
        state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT
            ? MTL::IndexTypeUInt32
            : MTL::IndexTypeUInt16;
    auto IndexBufferOffset =
        state_.InputAssembler.IndexBufferOffset +
        StartIndexLocation *
            (state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT
                 ? 4
                 : 2);
    ctx.EmitRenderCommandChk([IndexType, IndexBufferOffset, Primitive,
                              IndexCount,
                              BaseVertexLocation](CommandChunk::context &ctx) {
      D3D11_ASSERT(ctx.current_index_buffer_ref);
      ctx.render_encoder->drawIndexedPrimitives(
          Primitive, IndexCount, IndexType, ctx.current_index_buffer_ref,
          IndexBufferOffset, 1, BaseVertexLocation, 0);
    });
  }

  void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount,
                     UINT StartVertexLocation,
                     UINT StartInstanceLocation) override {
    if (!ctx.PreDraw())
      return;
    MTL::PrimitiveType Primitive =
        to_metal_topology(state_.InputAssembler.Topology);
    // TODO: skip invalid topology
    ctx.EmitRenderCommand(
        [Primitive, StartVertexLocation, VertexCountPerInstance, InstanceCount,
         StartInstanceLocation](MTL::RenderCommandEncoder *encoder) {
          encoder->drawPrimitives(Primitive, StartVertexLocation,
                                  VertexCountPerInstance, InstanceCount,
                                  StartInstanceLocation);
        });
  }

  void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount,
                            UINT StartIndexLocation, INT BaseVertexLocation,
                            UINT StartInstanceLocation) override {
    if (!ctx.PreDraw())
      return;
    MTL::PrimitiveType Primitive =
        to_metal_topology(state_.InputAssembler.Topology);
    // TODO: skip invalid topology
    auto IndexType =
        state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT
            ? MTL::IndexTypeUInt32
            : MTL::IndexTypeUInt16;
    auto IndexBufferOffset =
        state_.InputAssembler.IndexBufferOffset +
        StartIndexLocation *
            (state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT
                 ? 4
                 : 2);
    ctx.EmitRenderCommandChk(
        [IndexType, IndexBufferOffset, Primitive, InstanceCount,
         BaseVertexLocation, StartInstanceLocation,
         IndexCountPerInstance](CommandChunk::context &ctx) {
          D3D11_ASSERT(ctx.current_index_buffer_ref);
          ctx.render_encoder->drawIndexedPrimitives(
              Primitive, IndexCountPerInstance, IndexType,
              ctx.current_index_buffer_ref, IndexBufferOffset, InstanceCount,
              BaseVertexLocation, StartInstanceLocation);
        });
  }

  void DrawIndexedInstancedIndirect(ID3D11Buffer *pBufferForArgs,
                                    UINT AlignedByteOffsetForArgs) override {
    if (!ctx.PreDraw())
      return;
    auto currentChunkId = cmd_queue.CurrentSeqId();
    MTL::PrimitiveType Primitive =
        to_metal_topology(state_.InputAssembler.Topology);
    // TODO: skip invalid topology
    auto IndexType =
        state_.InputAssembler.IndexBufferFormat == DXGI_FORMAT_R32_UINT
            ? MTL::IndexTypeUInt32
            : MTL::IndexTypeUInt16;
    auto IndexBufferOffset = state_.InputAssembler.IndexBufferOffset;
    if (auto bindable = com_cast<IMTLBindable>(pBufferForArgs)) {
      ctx.EmitRenderCommandChk(
          [IndexType, IndexBufferOffset, Primitive,
           ArgBuffer = bindable->UseBindable(currentChunkId),
           AlignedByteOffsetForArgs](CommandChunk::context &ctx) {
            D3D11_ASSERT(ctx.current_index_buffer_ref);
            ctx.render_encoder->drawIndexedPrimitives(
                Primitive, IndexType, ctx.current_index_buffer_ref,
                IndexBufferOffset, ArgBuffer.buffer(),
                AlignedByteOffsetForArgs);
          });
    }
  }

  void DrawInstancedIndirect(ID3D11Buffer *pBufferForArgs,
                             UINT AlignedByteOffsetForArgs) override {
    IMPLEMENT_ME
  }

  void DrawAuto() override {
    ERR("DrawAuto: ignored");
    return;
  }

  void Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY,
                UINT ThreadGroupCountZ) override {
    if (!ctx.PreDispatch())
      return;
    ctx.EmitComputeCommand<true>(
        [ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ](
            MTL::ComputeCommandEncoder *encoder, MTL::Size &tg_size) {
          encoder->dispatchThreadgroups(MTL::Size::Make(ThreadGroupCountX,
                                                        ThreadGroupCountY,
                                                        ThreadGroupCountZ),
                                        tg_size);
        });
  }

  void DispatchIndirect(ID3D11Buffer *pBufferForArgs,
                        UINT AlignedByteOffsetForArgs) override {
    if (!ctx.PreDispatch())
      return;
    if (auto bindable = com_cast<IMTLBindable>(pBufferForArgs)) {
      ctx.EmitComputeCommand<true>(
          [AlignedByteOffsetForArgs,
           ArgBuffer = bindable->UseBindable(cmd_queue.CurrentSeqId())](
              MTL::ComputeCommandEncoder *encoder, MTL::Size &tg_size) {
            encoder->dispatchThreadgroups(ArgBuffer.buffer(),
                                          AlignedByteOffsetForArgs, tg_size);
          });
    }
  }
#pragma endregion

#pragma region State API

  void GetPredication(ID3D11Predicate **ppPredicate,
                      BOOL *pPredicateValue) override {

    if (ppPredicate) {
      *ppPredicate = state_.predicate.ref();
    }
    if (pPredicateValue) {
      *pPredicateValue = state_.predicate_value;
    }

    ERR_ONCE("Stub");
  }

  void SetPredication(ID3D11Predicate *pPredicate,
                      BOOL PredicateValue) override {

    state_.predicate = pPredicate;
    state_.predicate_value = PredicateValue;

    ERR_ONCE("Stub");
  }

  //-----------------------------------------------------------------------------
  // State Machine
  //-----------------------------------------------------------------------------

  void
  SwapDeviceContextState(ID3DDeviceContextState *pState,
                         ID3DDeviceContextState **ppPreviousState) override {
    IMPLEMENT_ME
  }

  void ClearState() override { state_ = {}; }

#pragma region InputAssembler

  void IASetInputLayout(ID3D11InputLayout *pInputLayout) override {
    if (auto expected = com_cast<IMTLD3D11InputLayout>(pInputLayout)) {
      state_.InputAssembler.InputLayout = std::move(expected);
    } else {
      state_.InputAssembler.InputLayout = nullptr;
    }
    ctx.InvalidateRenderPipeline();
  }
  void IAGetInputLayout(ID3D11InputLayout **ppInputLayout) override {
    if (ppInputLayout) {
      *ppInputLayout = state_.InputAssembler.InputLayout.ref();
    }
  }

  void IASetVertexBuffers(UINT StartSlot, UINT NumBuffers,
                          ID3D11Buffer *const *ppVertexBuffers,
                          const UINT *pStrides, const UINT *pOffsets) override {
    ctx.SetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides,
                         pOffsets);
  }

  void IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers,
                          ID3D11Buffer **ppVertexBuffers, UINT *pStrides,
                          UINT *pOffsets) override {
    ctx.GetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides,
                         pOffsets);
  }

  void IASetIndexBuffer(ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format,
                        UINT Offset) override {
    if (auto dynamic = com_cast<IMTLDynamicBindable>(pIndexBuffer)) {
      state_.InputAssembler.IndexBuffer = nullptr;
      dynamic->GetBindable(&state_.InputAssembler.IndexBuffer, [this](auto) {
        ctx.dirty_state.set(ContextInternal::DirtyState::IndexBuffer);
      });
    } else if (auto expected = com_cast<IMTLBindable>(pIndexBuffer)) {
      state_.InputAssembler.IndexBuffer = std::move(expected);
    } else {
      state_.InputAssembler.IndexBuffer = nullptr;
    }
    state_.InputAssembler.IndexBufferFormat = Format;
    state_.InputAssembler.IndexBufferOffset = Offset;
    ctx.dirty_state.set(ContextInternal::DirtyState::IndexBuffer);
  }
  void IAGetIndexBuffer(ID3D11Buffer **pIndexBuffer, DXGI_FORMAT *Format,
                        UINT *Offset) override {
    if (pIndexBuffer) {
      if (state_.InputAssembler.IndexBuffer) {
        state_.InputAssembler.IndexBuffer->GetLogicalResourceOrView(
            IID_PPV_ARGS(pIndexBuffer));
      } else {
        *pIndexBuffer = nullptr;
      }
    }
    if (Format != NULL)
      *Format = state_.InputAssembler.IndexBufferFormat;
    if (Offset != NULL)
      *Offset = state_.InputAssembler.IndexBufferOffset;
  }
  void IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) override {
    state_.InputAssembler.Topology = Topology;
  }
  void IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY *pTopology) override {
    if (pTopology) {
      *pTopology = state_.InputAssembler.Topology;
    }
  }
#pragma endregion

#pragma region VertexShader

  void VSSetShader(ID3D11VertexShader *pVertexShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) override {
    ctx.SetShader<ShaderType::Vertex, ID3D11VertexShader>(
        pVertexShader, ppClassInstances, NumClassInstances);
  }

  void VSGetShader(ID3D11VertexShader **ppVertexShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) override {
    ctx.GetShader<ShaderType::Vertex, ID3D11VertexShader>(
        ppVertexShader, ppClassInstances, pNumClassInstances);
  }

  void VSSetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    ctx.SetShaderResource<ShaderType::Vertex>(StartSlot, NumViews,
                                              ppShaderResourceViews);
  }

  void VSGetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView **ppShaderResourceViews) override {
    ctx.GetShaderResource<ShaderType::Vertex>(StartSlot, NumViews,
                                              ppShaderResourceViews);
  }

  void VSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) override {
    ctx.SetSamplers<ShaderType::Vertex>(StartSlot, NumSamplers, ppSamplers);
  }

  void VSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) override {
    ctx.GetSamplers<ShaderType::Vertex>(StartSlot, NumSamplers, ppSamplers);
  }

  void VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer *const *ppConstantBuffers) override {
    VSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) override {
    VSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void VSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer *const *ppConstantBuffers,
                             const UINT *pFirstConstant,
                             const UINT *pNumConstants) override {
    ctx.SetConstantBuffer<ShaderType::Vertex>(StartSlot, NumBuffers,
                                              ppConstantBuffers, pFirstConstant,
                                              pNumConstants);
  }

  void VSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant,
                             UINT *pNumConstants) override {
    ctx.GetConstantBuffer<ShaderType::Vertex>(StartSlot, NumBuffers,
                                              ppConstantBuffers, pFirstConstant,
                                              pNumConstants);
  }

#pragma endregion

#pragma region PixelShader

  void PSSetShader(ID3D11PixelShader *pPixelShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) override {
    ctx.SetShader<ShaderType::Pixel, ID3D11PixelShader>(
        pPixelShader, ppClassInstances, NumClassInstances);
  }

  void PSGetShader(ID3D11PixelShader **ppPixelShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) override {
    ctx.GetShader<ShaderType::Pixel, ID3D11PixelShader>(
        ppPixelShader, ppClassInstances, pNumClassInstances);
  }

  void PSSetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    ctx.SetShaderResource<ShaderType::Pixel>(StartSlot, NumViews,
                                             ppShaderResourceViews);
  }

  void PSGetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView **ppShaderResourceViews) override {
    ctx.GetShaderResource<ShaderType::Pixel>(StartSlot, NumViews,
                                             ppShaderResourceViews);
  }

  void PSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) override {
    ctx.SetSamplers<ShaderType::Pixel>(StartSlot, NumSamplers, ppSamplers);
  }

  void PSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) override {

    ctx.GetSamplers<ShaderType::Pixel>(StartSlot, NumSamplers, ppSamplers);
  }

  void PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer *const *ppConstantBuffers) override {
    PSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, 0, 0);
  }

  void PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) override {
    PSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, 0, 0);
  }

  void PSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer *const *ppConstantBuffers,
                             const UINT *pFirstConstant,
                             const UINT *pNumConstants) override {
    ctx.SetConstantBuffer<ShaderType::Pixel>(StartSlot, NumBuffers,
                                             ppConstantBuffers, pFirstConstant,
                                             pNumConstants);
  }

  void PSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant,
                             UINT *pNumConstants) override {
    ctx.GetConstantBuffer<ShaderType::Pixel>(StartSlot, NumBuffers,
                                             ppConstantBuffers, pFirstConstant,
                                             pNumConstants);
  }

#pragma endregion

#pragma region GeometryShader

  void GSSetShader(ID3D11GeometryShader *pShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) override {
    ctx.SetShader<ShaderType::Geometry>(pShader, ppClassInstances,
                                        NumClassInstances);
  }

  void GSGetShader(ID3D11GeometryShader **ppGeometryShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) override {
    ctx.GetShader<ShaderType::Geometry>(ppGeometryShader, ppClassInstances,
                                        pNumClassInstances);
  }

  void GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer *const *ppConstantBuffers) override {
    GSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void GSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer *const *ppConstantBuffers,
                             const UINT *pFirstConstant,
                             const UINT *pNumConstants) override {
    ctx.SetConstantBuffer<ShaderType::Geometry>(StartSlot, NumBuffers,
                                                ppConstantBuffers,
                                                pFirstConstant, pNumConstants);
  }

  void GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) override {
    GSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void GSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant,
                             UINT *pNumConstants) override {
    ctx.GetConstantBuffer<ShaderType::Geometry>(StartSlot, NumBuffers,
                                                ppConstantBuffers,
                                                pFirstConstant, pNumConstants);
  }

  void GSSetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    ctx.SetShaderResource<ShaderType::Geometry>(StartSlot, NumViews,
                                                ppShaderResourceViews);
  }

  void GSGetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView **ppShaderResourceViews) override {
    ctx.GetShaderResource<ShaderType::Geometry>(StartSlot, NumViews,
                                                ppShaderResourceViews);
  }

  void GSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) override {
    ctx.SetSamplers<ShaderType::Geometry>(StartSlot, NumSamplers, ppSamplers);
  }

  void GSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) override {
    ctx.GetSamplers<ShaderType::Geometry>(StartSlot, NumSamplers, ppSamplers);
  }

  void SOSetTargets(UINT NumBuffers, ID3D11Buffer *const *ppSOTargets,
                    const UINT *pOffsets) override {
    if (NumBuffers == 0) {
      NumBuffers = 4; // see msdn description of SOSetTargets
    }
    for (unsigned slot = 0; slot < NumBuffers; slot++) {
      auto pBuffer = ppSOTargets ? ppSOTargets[slot] : nullptr;
      if (pBuffer) {
        bool replaced = false;
        auto &entry =
            state_.StreamOutput.Targets.bind(slot, {pBuffer}, replaced);
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

  void SOGetTargets(UINT NumBuffers, ID3D11Buffer **ppSOTargets) override {
    IMPLEMENT_ME
  }

#pragma endregion

#pragma region HullShader

  void HSGetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView **ppShaderResourceViews) override {
    ctx.GetShaderResource<ShaderType::Hull>(StartSlot, NumViews,
                                            ppShaderResourceViews);
  }

  void HSGetShader(ID3D11HullShader **ppHullShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) override {
    ctx.GetShader<ShaderType::Hull>(ppHullShader, ppClassInstances,
                                    pNumClassInstances);
  }

  void HSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) override {
    ctx.GetSamplers<ShaderType::Hull>(StartSlot, NumSamplers, ppSamplers);
  }

  void HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) override {
    HSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void HSSetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    ctx.SetShaderResource<ShaderType::Hull>(StartSlot, NumViews,
                                            ppShaderResourceViews);
  }

  void HSSetShader(ID3D11HullShader *pHullShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) override {
    ctx.SetShader<ShaderType::Hull>(pHullShader, ppClassInstances,
                                    NumClassInstances);
  }

  void HSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) override {
    ctx.SetSamplers<ShaderType::Hull>(StartSlot, NumSamplers, ppSamplers);
  }

  void HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer *const *ppConstantBuffers) override {
    HSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void HSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer *const *ppConstantBuffers,
                             const UINT *pFirstConstant,
                             const UINT *pNumConstants) override {
    ctx.SetConstantBuffer<ShaderType::Hull>(StartSlot, NumBuffers,
                                            ppConstantBuffers, pFirstConstant,
                                            pNumConstants);
  }

  void HSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant,
                             UINT *pNumConstants) override {
    ctx.GetConstantBuffer<ShaderType::Hull>(StartSlot, NumBuffers,
                                            ppConstantBuffers, pFirstConstant,
                                            pNumConstants);
  }

#pragma endregion

#pragma region DomainShader

  void DSSetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    ctx.SetShaderResource<ShaderType::Domain>(StartSlot, NumViews,
                                              ppShaderResourceViews);
  }

  void DSGetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView **ppShaderResourceViews) override {
    ctx.GetShaderResource<ShaderType::Domain>(StartSlot, NumViews,
                                              ppShaderResourceViews);
  }

  void DSSetShader(ID3D11DomainShader *pDomainShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) override {
    ctx.SetShader<ShaderType::Domain, ID3D11DomainShader>(
        pDomainShader, ppClassInstances, NumClassInstances);
  }

  void DSGetShader(ID3D11DomainShader **ppDomainShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) override {
    ctx.GetShader<ShaderType::Domain, ID3D11DomainShader>(
        ppDomainShader, ppClassInstances, pNumClassInstances);
  }

  void DSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) override {
    ctx.GetSamplers<ShaderType::Domain>(StartSlot, NumSamplers, ppSamplers);
  }

  void DSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) override {
    ctx.SetSamplers<ShaderType::Domain>(StartSlot, NumSamplers, ppSamplers);
  }

  void DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer *const *ppConstantBuffers) override {
    return DSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL,
                                 NULL);
  }

  void DSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer *const *ppConstantBuffers,
                             const UINT *pFirstConstant,
                             const UINT *pNumConstants) override {
    ctx.SetConstantBuffer<ShaderType::Domain>(StartSlot, NumBuffers,
                                              ppConstantBuffers, pFirstConstant,
                                              pNumConstants);
  }

  void DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) override {
    UINT *pFirstConstant = 0, *pNumConstants = 0;
    return DSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers,
                                 pFirstConstant, pNumConstants);
  }

  void DSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant,
                             UINT *pNumConstants) override {
    ctx.GetConstantBuffer<ShaderType::Domain>(StartSlot, NumBuffers,
                                              ppConstantBuffers, pFirstConstant,
                                              pNumConstants);
  }

#pragma endregion

#pragma region ComputeShader

  void CSGetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView **ppShaderResourceViews) override {
    ctx.GetShaderResource<ShaderType::Compute>(StartSlot, NumViews,
                                               ppShaderResourceViews);
  }

  void CSSetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView *const *ppShaderResourceViews) override {
    ctx.SetShaderResource<ShaderType::Compute>(StartSlot, NumViews,
                                               ppShaderResourceViews);
  }

  void CSSetUnorderedAccessViews(
      UINT StartSlot, UINT NumUAVs,
      ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
      const UINT *pUAVInitialCounts) override {
    ctx.SetUnorderedAccessView<ShaderType::Compute>(
        StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
  }

  void CSGetUnorderedAccessViews(
      UINT StartSlot, UINT NumUAVs,
      ID3D11UnorderedAccessView **ppUnorderedAccessViews) override {
    for (auto i = 0u; i < NumUAVs; i++) {
      if (state_.ComputeStageUAV.UAVs.test_bound(StartSlot + i)) {
        state_.ComputeStageUAV.UAVs[StartSlot + i]
            .View->GetLogicalResourceOrView(
                IID_PPV_ARGS(&ppUnorderedAccessViews[i]));
      } else {
        ppUnorderedAccessViews[i] = nullptr;
      }
    }
  }

  void CSSetShader(ID3D11ComputeShader *pComputeShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) override {
    ctx.SetShader<ShaderType::Compute>(pComputeShader, ppClassInstances,
                                       NumClassInstances);
  }

  void CSGetShader(ID3D11ComputeShader **ppComputeShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) override {
    ctx.GetShader<ShaderType::Compute>(ppComputeShader, ppClassInstances,
                                       pNumClassInstances);
  }

  void CSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) override {
    ctx.SetSamplers<ShaderType::Compute>(StartSlot, NumSamplers, ppSamplers);
  }

  void CSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) override {
    ctx.GetSamplers<ShaderType::Compute>(StartSlot, NumSamplers, ppSamplers);
  }

  void CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer *const *ppConstantBuffers) override {
    CSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) override {
    CSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void CSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer *const *ppConstantBuffers,
                             const UINT *pFirstConstant,
                             const UINT *pNumConstants) override {
    ctx.SetConstantBuffer<ShaderType::Compute>(StartSlot, NumBuffers,
                                               ppConstantBuffers,
                                               pFirstConstant, pNumConstants);
  }

  void CSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant,
                             UINT *pNumConstants) override {
    ctx.GetConstantBuffer<ShaderType::Compute>(StartSlot, NumBuffers,
                                               ppConstantBuffers,
                                               pFirstConstant, pNumConstants);
  }

#pragma endregion

#pragma region OutputMerger

  void OMSetRenderTargets(UINT NumViews,
                          ID3D11RenderTargetView *const *ppRenderTargetViews,
                          ID3D11DepthStencilView *pDepthStencilView) override {
    OMSetRenderTargetsAndUnorderedAccessViews(
        NumViews, ppRenderTargetViews, pDepthStencilView, 0,
        D3D11_KEEP_UNORDERED_ACCESS_VIEWS, NULL, NULL);
  }

  void
  OMGetRenderTargets(UINT NumViews,
                     ID3D11RenderTargetView **ppRenderTargetViews,
                     ID3D11DepthStencilView **ppDepthStencilView) override {
    OMGetRenderTargetsAndUnorderedAccessViews(NumViews, ppRenderTargetViews,
                                              ppDepthStencilView, 0, 0, NULL);
  }

  void OMSetRenderTargetsAndUnorderedAccessViews(
      UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews,
      ID3D11DepthStencilView *pDepthStencilView, UINT UAVStartSlot,
      UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
      const UINT *pUAVInitialCounts) override {

    bool should_invalidate_pass = false;

    if (NumRTVs != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL) {
      auto &BoundRTVs = state_.OutputMerger.RTVs;
      constexpr unsigned RTVSlotCount = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
      for (unsigned rtv_index = 0; rtv_index < RTVSlotCount; rtv_index++) {
        if (rtv_index < NumRTVs && ppRenderTargetViews[rtv_index]) {
          if (auto expected = com_cast<IMTLD3D11RenderTargetView>(
                  ppRenderTargetViews[rtv_index])) {
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

      if (auto expected =
              com_cast<IMTLD3D11DepthStencilView>(pDepthStencilView)) {
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
      ctx.SetUnorderedAccessView<ShaderType::Pixel>(
          UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
    }

    if (should_invalidate_pass) {
      ctx.InvalidateCurrentPass();
    }
  }

  void OMGetRenderTargetsAndUnorderedAccessViews(
      UINT NumRTVs, ID3D11RenderTargetView **ppRenderTargetViews,
      ID3D11DepthStencilView **ppDepthStencilView, UINT UAVStartSlot,
      UINT NumUAVs,
      ID3D11UnorderedAccessView **ppUnorderedAccessViews) override {
    if (ppRenderTargetViews) {
      for (unsigned i = 0; i < NumRTVs; i++) {
        if (i < state_.OutputMerger.NumRTVs) {
          state_.OutputMerger.RTVs[i]->QueryInterface(
              IID_PPV_ARGS(&ppRenderTargetViews[i]));
        } else {
          ppRenderTargetViews[i] = nullptr;
        }
      }
    }
    if (ppDepthStencilView) {
      if (state_.OutputMerger.DSV) {
        state_.OutputMerger.DSV->QueryInterface(
            IID_PPV_ARGS(ppDepthStencilView));
      } else {
        ppDepthStencilView = nullptr;
      }
    }
    if (ppUnorderedAccessViews) {
      for (unsigned i = 0; i < NumUAVs; i++) {
        if (state_.OutputMerger.UAVs.test_bound(i + UAVStartSlot)) {
          state_.OutputMerger.UAVs[i + UAVStartSlot].View->QueryInterface(
              IID_PPV_ARGS(&ppUnorderedAccessViews[i]));
        } else {
          ppUnorderedAccessViews[i] = nullptr;
        }
      }
    }
  }

  void OMSetBlendState(ID3D11BlendState *pBlendState,
                       const FLOAT BlendFactor[4], UINT SampleMask) override {
    bool should_invalidate_pipeline = false;
    if (auto expected = com_cast<IMTLD3D11BlendState>(pBlendState)) {
      if (expected.ptr() != state_.OutputMerger.BlendState) {
        state_.OutputMerger.BlendState = expected.ptr();
        should_invalidate_pipeline = true;
      }
      if (BlendFactor) {
        memcpy(state_.OutputMerger.BlendFactor, BlendFactor, sizeof(float[4]));
      } else {
        state_.OutputMerger.BlendFactor[0] = 1.0f;
        state_.OutputMerger.BlendFactor[1] = 1.0f;
        state_.OutputMerger.BlendFactor[2] = 1.0f;
        state_.OutputMerger.BlendFactor[3] = 1.0f;
      }
    }
    if(state_.OutputMerger.SampleMask != SampleMask) {
      state_.OutputMerger.SampleMask = SampleMask;
      should_invalidate_pipeline = true;
    }
    if (should_invalidate_pipeline) {
      ctx.InvalidateRenderPipeline();
    }
    ctx.dirty_state.set(ContextInternal::DirtyState::BlendFactorAndStencilRef);
  }
  void OMGetBlendState(ID3D11BlendState **ppBlendState, FLOAT BlendFactor[4],
                       UINT *pSampleMask) override {
    if (ppBlendState) {
      if(state_.OutputMerger.BlendState) {
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

  void OMSetDepthStencilState(ID3D11DepthStencilState *pDepthStencilState,
                              UINT StencilRef) override {
    if (auto expected =
            com_cast<IMTLD3D11DepthStencilState>(pDepthStencilState)) {
      state_.OutputMerger.DepthStencilState = expected.ptr();
      state_.OutputMerger.StencilRef = StencilRef;
      ctx.dirty_state.set(ContextInternal::DirtyState::DepthStencilState);
    }
  }

  void OMGetDepthStencilState(ID3D11DepthStencilState **ppDepthStencilState,
                              UINT *pStencilRef) override {
    if (ppDepthStencilState) {
      if (state_.OutputMerger.DepthStencilState) {
        state_.OutputMerger.DepthStencilState->QueryInterface(
            IID_PPV_ARGS(ppDepthStencilState));
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

  void RSSetState(ID3D11RasterizerState *pRasterizerState) override {
    if (pRasterizerState) {
      if (auto expected =
              com_cast<IMTLD3D11RasterizerState>(pRasterizerState)) {
        auto current_rs = state_.Rasterizer.RasterizerState
                               ? state_.Rasterizer.RasterizerState
                               : ctx.default_rasterizer_state;
        if (current_rs == expected.ptr()) {
          return;
        }
        if (current_rs->IsScissorEnabled() != expected->IsScissorEnabled()) {
          ctx.dirty_state.set(ContextInternal::DirtyState::Scissors);
        }
        state_.Rasterizer.RasterizerState = expected.ptr();
        ctx.dirty_state.set(ContextInternal::DirtyState::RasterizerState);
      } else {
        ERR("RSSetState: invalid ID3D11RasterizerState object.");
      }
    } else {
      if (state_.Rasterizer.RasterizerState) {
        if (state_.Rasterizer.RasterizerState->IsScissorEnabled() !=
            ctx.default_rasterizer_state->IsScissorEnabled()) {
          ctx.dirty_state.set(ContextInternal::DirtyState::Scissors);
        }
      }
      state_.Rasterizer.RasterizerState = nullptr;
      ctx.dirty_state.set(ContextInternal::DirtyState::RasterizerState);
    }
  }

  void RSGetState(ID3D11RasterizerState **ppRasterizerState) override {
    if (ppRasterizerState) {
      if (state_.Rasterizer.RasterizerState) {
        state_.Rasterizer.RasterizerState->QueryInterface(
            IID_PPV_ARGS(ppRasterizerState));
      } else {
        *ppRasterizerState = NULL;
      }
    }
  }

  void RSSetViewports(UINT NumViewports,
                      const D3D11_VIEWPORT *pViewports) override {
    if (NumViewports > 16)
      return;
    if (state_.Rasterizer.NumViewports == NumViewports &&
        memcmp(state_.Rasterizer.viewports, pViewports,
               NumViewports * sizeof(D3D11_VIEWPORT)) == 0) {
      return;
    }
    state_.Rasterizer.NumViewports = NumViewports;
    for (auto i = 0u; i < NumViewports; i++) {
      state_.Rasterizer.viewports[i] = pViewports[i];
    }
    ctx.dirty_state.set(ContextInternal::DirtyState::Viewport);
    IMTLD3D11RasterizerState *current_rs =
        state_.Rasterizer.RasterizerState ? state_.Rasterizer.RasterizerState
                                          : ctx.default_rasterizer_state;
    if (!current_rs->IsScissorEnabled()) {
      ctx.dirty_state.set(ContextInternal::DirtyState::Scissors);
    }
  }

  void RSGetViewports(UINT *pNumViewports,
                      D3D11_VIEWPORT *pViewports) override {
    if (pNumViewports) {
      *pNumViewports = state_.Rasterizer.NumViewports;
    }
    if (pViewports) {
      for (auto i = 0u; i < state_.Rasterizer.NumViewports; i++) {
        pViewports[i] = state_.Rasterizer.viewports[i];
      }
    }
  }

  void RSSetScissorRects(UINT NumRects, const D3D11_RECT *pRects) override {
    if (NumRects > 16)
      return;
    if (state_.Rasterizer.NumScissorRects == NumRects &&
        memcmp(state_.Rasterizer.scissor_rects, pRects,
               NumRects * sizeof(D3D11_RECT)) == 0) {
      return;
    }
    state_.Rasterizer.NumScissorRects = NumRects;
    for (unsigned i = 0; i < NumRects; i++) {
      state_.Rasterizer.scissor_rects[i] = pRects[i];
    }
    IMTLD3D11RasterizerState *current_rs =
        state_.Rasterizer.RasterizerState
            ? state_.Rasterizer.RasterizerState
            : ctx.default_rasterizer_state;
    if (current_rs->IsScissorEnabled()) {
      ctx.dirty_state.set(ContextInternal::DirtyState::Scissors);
      // otherwise no need to update scissor because it's either already dirty
      // or duplicates viewports
    }
  }

  void RSGetScissorRects(UINT *pNumRects, D3D11_RECT *pRects) override {
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
      ID3D11Resource *resource, UINT region_count,
      const D3D11_TILED_RESOURCE_COORDINATE *region_start_coordinates,
      const D3D11_TILE_REGION_SIZE *region_sizes, ID3D11Buffer *pool,
      UINT range_count, const UINT *range_flags, const UINT *pool_start_offsets,
      const UINT *range_tile_counts, UINT flags) override {
    IMPLEMENT_ME
  };

  HRESULT STDMETHODCALLTYPE CopyTileMappings(
      ID3D11Resource *dst_resource,
      const D3D11_TILED_RESOURCE_COORDINATE *dst_start_coordinate,
      ID3D11Resource *src_resource,
      const D3D11_TILED_RESOURCE_COORDINATE *src_start_coordinate,
      const D3D11_TILE_REGION_SIZE *region_size, UINT flags) override {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE
  CopyTiles(ID3D11Resource *resource,
            const D3D11_TILED_RESOURCE_COORDINATE *start_coordinate,
            const D3D11_TILE_REGION_SIZE *size, ID3D11Buffer *buffer,
            UINT64 start_offset, UINT flags) override {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE
  UpdateTiles(ID3D11Resource *dst_resource,
              const D3D11_TILED_RESOURCE_COORDINATE *dst_start_coordinate,
              const D3D11_TILE_REGION_SIZE *dst_region_size,
              const void *src_data, UINT flags) override {
    IMPLEMENT_ME
  };

  HRESULT STDMETHODCALLTYPE ResizeTilePool(ID3D11Buffer *pool,
                                           UINT64 size) override {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE
  TiledResourceBarrier(ID3D11DeviceChild *before_barrier,
                       ID3D11DeviceChild *after_barrier) override {
    IMPLEMENT_ME
  };

  WINBOOL STDMETHODCALLTYPE IsAnnotationEnabled() override { IMPLEMENT_ME };

  void STDMETHODCALLTYPE SetMarkerInt(const WCHAR *label, int data) override {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE BeginEventInt(const WCHAR *label, int data) override {
    IMPLEMENT_ME
  };

  void STDMETHODCALLTYPE EndEvent() override { IMPLEMENT_ME };

#pragma endregion

#pragma region DynamicBufferPool

  void ExchangeFromPool(MTL::Buffer **pBuffer, uint64_t *gpuAddr,
                        void **cpuAddr, BufferPool *pool) final {
    D3D11_ASSERT(*pBuffer);
    D3D11_ASSERT(pool);
    pool->GetNext(cmd_queue.CurrentSeqId(), cmd_queue.CoherentSeqId(), pBuffer,
                  gpuAddr, cpuAddr);
  }

#pragma endregion

#pragma region Misc

  D3D11_DEVICE_CONTEXT_TYPE GetType() override {
    return D3D11_DEVICE_CONTEXT_IMMEDIATE;
  }

  UINT GetContextFlags() override { return 0; }

#pragma endregion

#pragma region 11.3

  void STDMETHODCALLTYPE Flush1(D3D11_CONTEXT_TYPE type,
                                HANDLE event) override {
    IMPLEMENT_ME;
  }

  void STDMETHODCALLTYPE SetHardwareProtectionState(WINBOOL enable) override {
    WARN("SetHardwareProtectionState: stub");
  }

  void STDMETHODCALLTYPE GetHardwareProtectionState(WINBOOL *enable) override {
    *enable = false;
    WARN("GetHardwareProtectionState: stub");
  }

  virtual HRESULT STDMETHODCALLTYPE Signal(ID3D11Fence *fence,
                                           UINT64 value) override {
    IMPLEMENT_ME
  }

  virtual HRESULT STDMETHODCALLTYPE Wait(ID3D11Fence *fence,
                                         UINT64 value) override {
    IMPLEMENT_ME
  }

#pragma endregion

  void FlushInternal(std::function<void(MTL::CommandBuffer *)> &&beforeCommit,
                     std::function<void(void)> &&onFinished,
                     bool present_) final {
    D3D11_ASSERT(!ctx.InvalidateCurrentPass(true));
    cmd_queue.CurrentChunk()->emit(
        [bc = std::move(beforeCommit),
         _ = DestructorWrapper([of = std::move(onFinished)]() { of(); },
                               nullptr)](CommandChunk::context &ctx) {
          bc(ctx.cmdbuf);
        });
    ctx.Commit();
    if (present_) {
      cmd_queue.PresentBoundary();
    }
  }

private:
  Obj<MTL::Device> metal_device_;
  D3D11ContextState state_;

  CommandQueue cmd_queue;
  ContextInternal ctx;

public:
  MTLD3D11DeviceContext(IMTLD3D11Device *pDevice)
      : MTLD3D11DeviceContextBase(pDevice),
        metal_device_(m_parent->GetMTLDevice()), state_(),
        cmd_queue(metal_device_), ctx(pDevice, state_, cmd_queue) {}

  ~MTLD3D11DeviceContext() {}

  virtual void WaitUntilGPUIdle() override {
    uint64_t seq = cmd_queue.CurrentSeqId();
    FlushInternal([](auto) {}, []() {}, false);
    cmd_queue.WaitCPUFence(seq);
  };
};

std::unique_ptr<MTLD3D11DeviceContextBase>
InitializeImmediateContext(IMTLD3D11Device *pDevice) {
  return std::make_unique<MTLD3D11DeviceContext>(pDevice);
}

} // namespace dxmt