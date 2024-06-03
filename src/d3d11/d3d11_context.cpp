

#include "d3d11_context.hpp"
#include "d3d11_query.hpp"
#include "dxmt_command_queue.hpp"

#include "./d3d11_context_internal.cpp"

namespace dxmt {

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

using MTLD3D11DeviceContextBase =
    MTLD3D11DeviceChild<IMTLD3D11DeviceContext, IMTLDynamicBufferPool>;

class MTLD3D11DeviceContext : public MTLD3D11DeviceContextBase {
public:
  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11DeviceContext) ||
        riid == __uuidof(ID3D11DeviceContext1) ||
        riid == __uuidof(IMTLD3D11DeviceContext)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(IMTLD3D11DeviceContext), riid)) {
      WARN("D3D11DeviceContext: Unknown interface query ", str::format(riid));
    }
    return E_NOINTERFACE;
  }

  void Begin(ID3D11Asynchronous *pAsync) {
    // in theory pAsync could be any of them: { Query, Predicate, Counter }.
    // However `Predicate` and `Counter` are not supported at all
    static_cast<MTLD3D11Query *>(pAsync)->Begin();
  }

  // See Begin()
  void End(ID3D11Asynchronous *pAsync) {
    static_cast<MTLD3D11Query *>(pAsync)->End();
  }

  HRESULT GetData(ID3D11Asynchronous *pAsync, void *pData, UINT DataSize,
                  UINT GetDataFlags) {
    if (!pAsync || (DataSize && !pData))
      return E_INVALIDARG;

    // Allow dataSize to be zero
    if (DataSize && DataSize != pAsync->GetDataSize())
      return E_INVALIDARG;
    pData = DataSize ? pData : nullptr;

    auto query = static_cast<MTLD3D11Query *>(pAsync);
    return query->GetData(pData, GetDataFlags);
  }

  HRESULT Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType,
              UINT MapFlags, D3D11_MAPPED_SUBRESOURCE *pMappedResource) {
    D3D11_MAPPED_SUBRESOURCE Out;
    if (auto dynamic = com_cast<IMTLDynamicBuffer>(pResource)) {
      switch (MapType) {
      case D3D11_MAP_READ:
      case D3D11_MAP_WRITE:
      case D3D11_MAP_READ_WRITE:
        return E_INVALIDARG;
      case D3D11_MAP_WRITE_DISCARD: {
        dynamic->RotateBuffer(this);
        Out.pData =
            dynamic->GetCurrentBuffer(&pMappedResource->RowPitch)->contents();
        break;
      }
      case D3D11_MAP_WRITE_NO_OVERWRITE: {
        Out.pData =
            dynamic->GetCurrentBuffer(&pMappedResource->RowPitch)->contents();
        break;
      }
      }
      if (pMappedResource) {
        *pMappedResource = Out;
      }
      return S_OK;
    }

    IMPLEMENT_ME;
  }

  void Unmap(ID3D11Resource *pResource, UINT Subresource) {
    if (auto dynamic = com_cast<IMTLDynamicBuffer>(pResource)) {
      return;
    }
    IMPLEMENT_ME;
  }

  void Flush() {
    Flush2([](auto) {});
  }

  void ExecuteCommandList(ID3D11CommandList *pCommandList,
                          BOOL RestoreContextState){IMPLEMENT_ME}

  HRESULT FinishCommandList(BOOL RestoreDeferredContextState,
                            ID3D11CommandList **ppCommandList) {
    return DXGI_ERROR_INVALID_CALL;
  }

  void SetResourceMinLOD(ID3D11Resource *pResource, FLOAT MinLOD) {
    // FIXME: `min_lod_clamp` can do this but it's in the shader
    ERR_ONCE("Not implemented");
  }

  FLOAT GetResourceMinLOD(ID3D11Resource *pResource) {
    ERR_ONCE("Not implemented");
    return 0.0f;
  }

#pragma region Resource Manipulation

  void ClearRenderTargetView(ID3D11RenderTargetView *pRenderTargetView,
                             const FLOAT ColorRGBA[4]) {
    if (auto expected =
            com_cast<IMTLD3D11RenderTargetView>(pRenderTargetView)) {
      ctx.InvalidateCurrentPass();
      CommandChunk *chk = cmd_queue.CurrentChunk();
      // GetCurrentTexture() is executed outside of command body
      // because of swapchain logic implemented at the moment
      // ideally it should be inside the command
      // so autorelease will work properly
      chk->emit([texture = expected->GetCurrentTexture(), r = ColorRGBA[0],
                 g = ColorRGBA[1], b = ColorRGBA[2],
                 a = ColorRGBA[3]](CommandChunk::context &ctx) {
        auto enc_descriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
        auto attachmentz = enc_descriptor->colorAttachments()->object(0);
        attachmentz->setClearColor({r, g, b, a});
        attachmentz->setTexture(texture);
        attachmentz->setLoadAction(MTL::LoadActionClear);
        attachmentz->setStoreAction(MTL::StoreActionStore);

        auto enc = ctx.cmdbuf->renderCommandEncoder(enc_descriptor);
        enc->endEncoding();
      });
    }
  }

  void
  ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView *pUnorderedAccessView,
                               const UINT Values[4]) {
    IMPLEMENT_ME
  }

  void
  ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView *pUnorderedAccessView,
                                const FLOAT Values[4]) {
    IMPLEMENT_ME
  }

  void ClearDepthStencilView(ID3D11DepthStencilView *pDepthStencilView,
                             UINT ClearFlags, FLOAT Depth, UINT8 Stencil) {
    if (auto expected =
            com_cast<IMTLD3D11DepthStencilView>(pDepthStencilView)) {

      ctx.InvalidateCurrentPass();
      CommandChunk *chk = cmd_queue.CurrentChunk();
      chk->emit([texture = expected->GetCurrentTexture(), Depth, Stencil,
                 ClearDepth = (ClearFlags & D3D11_CLEAR_DEPTH),
                 ClearStencil = (ClearFlags & D3D11_CLEAR_STENCIL)](
                    CommandChunk::context &ctx) {
        auto enc_descriptor = MTL::RenderPassDescriptor::renderPassDescriptor();
        if (ClearDepth) {
          auto attachmentz = enc_descriptor->depthAttachment();
          attachmentz->setClearDepth(Depth);
          attachmentz->setTexture(texture);
          attachmentz->setLoadAction(MTL::LoadActionClear);
          attachmentz->setStoreAction(MTL::StoreActionStore);
        }
        if (ClearStencil) {
          auto attachmentz = enc_descriptor->stencilAttachment();
          attachmentz->setClearStencil(Stencil);
          attachmentz->setTexture(texture);
          attachmentz->setLoadAction(MTL::LoadActionClear);
          attachmentz->setStoreAction(MTL::StoreActionStore);
        }

        auto enc = ctx.cmdbuf->renderCommandEncoder(enc_descriptor);
        enc->endEncoding();
      });
    }
  }

  void ClearView(ID3D11View *pView, const FLOAT Color[4],
                 const D3D11_RECT *pRect, UINT NumRects) {
    IMPLEMENT_ME
  }

  void GenerateMips(ID3D11ShaderResourceView *pShaderResourceView) {
    D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    pShaderResourceView->GetDesc(&desc);
    if (desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER ||
        desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX) {
      return;
    }
    IMPLEMENT_ME
  }

  void ResolveSubresource(ID3D11Resource *pDstResource, UINT DstSubresource,
                          ID3D11Resource *pSrcResource, UINT SrcSubresource,
                          DXGI_FORMAT Format) {
    // Metal does not provide methods for explicit resolve action.
    IMPLEMENT_ME
  }

  void CopyResource(ID3D11Resource *pDstResource,
                    ID3D11Resource *pSrcResource) {
    D3D11_RESOURCE_DIMENSION dst_dim, src_dim;
    pDstResource->GetType(&dst_dim);
    pSrcResource->GetType(&src_dim);
    if (dst_dim != src_dim)
      return;
    ;
    switch (dst_dim) {
    case D3D11_RESOURCE_DIMENSION_UNKNOWN:
      return; // wut
    case D3D11_RESOURCE_DIMENSION_BUFFER: {

      IMPLEMENT_ME
      break;
    }
    case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
    case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
    case D3D11_RESOURCE_DIMENSION_TEXTURE3D: {
      IMPLEMENT_ME
      break;
    }
    }
  }

  void CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset,
                          ID3D11UnorderedAccessView *pSrcView) {
    IMPLEMENT_ME
  }

  void CopySubresourceRegion(ID3D11Resource *pDstResource, UINT DstSubresource,
                             UINT DstX, UINT DstY, UINT DstZ,
                             ID3D11Resource *pSrcResource, UINT SrcSubresource,
                             const D3D11_BOX *pSrcBox) {
    CopySubresourceRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ,
                           pSrcResource, SrcSubresource, pSrcBox, 0);
  }

  void CopySubresourceRegion1(ID3D11Resource *pDstResource, UINT DstSubresource,
                              UINT DstX, UINT DstY, UINT DstZ,
                              ID3D11Resource *pSrcResource, UINT SrcSubresource,
                              const D3D11_BOX *pSrcBox, UINT CopyFlags) {
    IMPLEMENT_ME
  }

  void UpdateSubresource(ID3D11Resource *pDstResource, UINT DstSubresource,
                         const D3D11_BOX *pDstBox, const void *pSrcData,
                         UINT SrcRowPitch, UINT SrcDepthPitch) {
    UpdateSubresource1(pDstResource, DstSubresource, pDstBox, pSrcData,
                       SrcRowPitch, SrcDepthPitch, 0);
  }

  void UpdateSubresource1(ID3D11Resource *pDstResource, UINT DstSubresource,
                          const D3D11_BOX *pDstBox, const void *pSrcData,
                          UINT SrcRowPitch, UINT SrcDepthPitch,
                          UINT CopyFlags) {

    if (pDstBox != NULL) {
      if (pDstBox->right <= pDstBox->left || pDstBox->bottom <= pDstBox->top ||
          pDstBox->back <= pDstBox->front) {
        return;
      }
    }

    IMPLEMENT_ME
  }

  void DiscardResource(ID3D11Resource *pResource) {
    /*
    All the Discard* API is not implemented and that's probably fine (as it's
    more like a hint of optimization, and Metal manages resources on its own)
    FIXME: for render targets we can use this information: LoadActionDontCare
    FIXME: A Map with D3D11_MAP_WRITE type could become D3D11_MAP_WRITE_DISCARD?
    */
    ERR_ONCE("Not implemented");
  }

  void DiscardView(ID3D11View *pResourceView) {
    DiscardView1(pResourceView, 0, 0);
  }

  void DiscardView1(ID3D11View *pResourceView, const D3D11_RECT *pRects,
                    UINT NumRects) {
    ERR_ONCE("Not implemented");
  }
#pragma endregion

#pragma region DrawCall

  void Draw(UINT VertexCount, UINT StartVertexLocation) {
    ctx.PreDraw();
    MTL::PrimitiveType Primitive =
        to_metal_topology(state_.InputAssembler.Topology);
    // TODO: skip invalid topology
    ctx.EmitRenderCommand([Primitive, StartVertexLocation,
                           VertexCount](MTL::RenderCommandEncoder *encoder) {
      encoder->drawPrimitives(Primitive, StartVertexLocation, VertexCount);
    });
  }

  void DrawIndexed(UINT IndexCount, UINT StartIndexLocation,
                   INT BaseVertexLocation) {
    ctx.PreDraw();
    MTL::PrimitiveType Primitive =
        to_metal_topology(state_.InputAssembler.Topology);
    MTL_BIND_RESOURCE IndexBuffer;
    assert(state_.InputAssembler.IndexBuffer);
    state_.InputAssembler.IndexBuffer->GetBoundResource(&IndexBuffer);
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
    ctx.EmitRenderCommand<>(
        [IndexBuffer = Obj(IndexBuffer.Buffer), IndexType, IndexBufferOffset,
         Primitive, IndexCount,
         BaseVertexLocation](MTL::RenderCommandEncoder *encoder) {
          encoder->drawIndexedPrimitives(Primitive, IndexCount, IndexType,
                                         IndexBuffer, IndexBufferOffset, 1,
                                         BaseVertexLocation, 0);
        });
  }

  void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount,
                     UINT StartVertexLocation, UINT StartInstanceLocation) {
    ctx.PreDraw();
    MTL::PrimitiveType Primitive =
        to_metal_topology(state_.InputAssembler.Topology);
    // TODO: skip invalid topology
    ctx.EmitRenderCommand<>(
        [Primitive, StartVertexLocation, VertexCountPerInstance, InstanceCount,
         StartInstanceLocation](MTL::RenderCommandEncoder *encoder) {
          encoder->drawPrimitives(Primitive, StartVertexLocation,
                                  VertexCountPerInstance, InstanceCount,
                                  StartInstanceLocation);
        });
  }

  void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount,
                            UINT StartIndexLocation, INT BaseVertexLocation,
                            UINT StartInstanceLocation) {
    ctx.PreDraw();
    MTL::PrimitiveType Primitive =
        to_metal_topology(state_.InputAssembler.Topology);
    // TODO: skip invalid topology
    MTL_BIND_RESOURCE IndexBuffer;
    assert(state_.InputAssembler.IndexBuffer);
    state_.InputAssembler.IndexBuffer->GetBoundResource(&IndexBuffer);
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
    ctx.EmitRenderCommand<>(
        [IndexBuffer = Obj(IndexBuffer.Buffer), IndexType, IndexBufferOffset,
         Primitive, InstanceCount, BaseVertexLocation, StartInstanceLocation,
         IndexCountPerInstance](MTL::RenderCommandEncoder *encoder) {
          encoder->drawIndexedPrimitives(
              Primitive, IndexCountPerInstance, IndexType, IndexBuffer,
              IndexBufferOffset, InstanceCount, BaseVertexLocation,
              StartInstanceLocation);
        });
  }

  void DrawIndexedInstancedIndirect(ID3D11Buffer *pBufferForArgs,
                                    UINT AlignedByteOffsetForArgs) {
    IMPLEMENT_ME
  }

  void DrawInstancedIndirect(ID3D11Buffer *pBufferForArgs,
                             UINT AlignedByteOffsetForArgs) {
    IMPLEMENT_ME
  }

  void DrawAuto() { IMPLEMENT_ME }

  void Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY,
                UINT ThreadGroupCountZ) {
    if (!ctx.PreDispatch())
      return;
    ctx.EmitComputeCommand(
        [ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ](
            MTL::ComputeCommandEncoder *encoder, MTL::Size &tg_size) {
          encoder->dispatchThreadgroups(MTL::Size::Make(ThreadGroupCountX,
                                                        ThreadGroupCountY,
                                                        ThreadGroupCountZ),
                                        tg_size);
        });
  }

  void DispatchIndirect(ID3D11Buffer *pBufferForArgs,
                        UINT AlignedByteOffsetForArgs) {
    if (!ctx.PreDispatch())
      return;
    ctx.EmitComputeCommand([AlignedByteOffsetForArgs](
                               MTL::ComputeCommandEncoder *encoder,
                               MTL::Size &tg_size) {
      encoder->dispatchThreadgroups(nullptr, AlignedByteOffsetForArgs, tg_size);
    });
  }
#pragma endregion

#pragma region State API

  void GetPredication(ID3D11Predicate **ppPredicate, BOOL *pPredicateValue) {

    if (ppPredicate) {
      *ppPredicate = state_.predicate.ref();
    }
    if (pPredicateValue) {
      *pPredicateValue = state_.predicate_value;
    }

    ERR_ONCE("Stub");
  }

  void SetPredication(ID3D11Predicate *pPredicate, BOOL PredicateValue) {

    state_.predicate = pPredicate;
    state_.predicate_value = PredicateValue;

    ERR_ONCE("Stub");
  }

  //-----------------------------------------------------------------------------
  // State Machine
  //-----------------------------------------------------------------------------

  void SwapDeviceContextState(ID3DDeviceContextState *pState,
                              ID3DDeviceContextState **ppPreviousState) {
    IMPLEMENT_ME
  }

  void ClearState() { state_ = {}; }

#pragma region InputAssembler

  void IASetInputLayout(ID3D11InputLayout *pInputLayout) {
    if (auto expected = com_cast<IMTLD3D11InputLayout>(pInputLayout)) {
      state_.InputAssembler.InputLayout = std::move(expected);
    }
    ctx.InvalidateGraphicPipeline();
  }
  void IAGetInputLayout(ID3D11InputLayout **ppInputLayout) {
    if (ppInputLayout) {
      *ppInputLayout = state_.InputAssembler.InputLayout.ref();
    }
  }

  void IASetVertexBuffers(UINT StartSlot, UINT NumBuffers,
                          ID3D11Buffer *const *ppVertexBuffers,
                          const UINT *pStrides, const UINT *pOffsets) {
    auto &BoundVBs = state_.InputAssembler.VertexBuffers;
    for (unsigned Slot = StartSlot; Slot < StartSlot + NumBuffers; Slot++) {
      auto &VB = BoundVBs[Slot];
      VB.Buffer = nullptr;
      assert(ppVertexBuffers);
      assert(pStrides);
      assert(pOffsets);
      if (auto dynamic = com_cast<IMTLDynamicBindable>(
              ppVertexBuffers[Slot - StartSlot])) {
        dynamic->GetBindable(&VB.Buffer, [this]() { ctx.EmitSetIAState(); });
      } else if (auto expected = com_cast<IMTLBindable>(
                     ppVertexBuffers[Slot - StartSlot])) {
        VB.Buffer = std::move(expected);
      } else if (ppVertexBuffers[Slot - StartSlot]) {
        ERR("IASetVertexBuffers: wrong vertex buffer binding");
      } else {
        BoundVBs.erase(Slot);
      }
      VB.Stride = pStrides[Slot - StartSlot];
      VB.Offset = pOffsets[Slot - StartSlot];
    }
  }

  void IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers,
                          ID3D11Buffer **ppVertexBuffers, UINT *pStrides,
                          UINT *pOffsets) {
    for (unsigned i = 0; i < NumBuffers; i++) {
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

  void IASetIndexBuffer(ID3D11Buffer *pIndexBuffer, DXGI_FORMAT Format,
                        UINT Offset) {
    state_.InputAssembler.IndexBuffer = nullptr;
    if (auto dynamic = com_cast<IMTLDynamicBindable>(pIndexBuffer)) {
      dynamic->GetBindable(&state_.InputAssembler.IndexBuffer, []() {
        assert(0 && "FIXME: dynamic index buffer updated");
      });
    } else if (auto expected = com_cast<IMTLBindable>(pIndexBuffer)) {
      state_.InputAssembler.IndexBuffer = std::move(expected);
    }
    state_.InputAssembler.IndexBufferFormat = Format;
    state_.InputAssembler.IndexBufferOffset = Offset;
  }
  void IAGetIndexBuffer(ID3D11Buffer **pIndexBuffer, DXGI_FORMAT *Format,
                        UINT *Offset) {
    if (pIndexBuffer != NULL) {
      state_.InputAssembler.IndexBuffer->GetLogicalResourceOrView(
          IID_PPV_ARGS(pIndexBuffer));
    }
    if (Format != NULL)
      *Format = state_.InputAssembler.IndexBufferFormat;
    if (Offset != NULL)
      *Offset = state_.InputAssembler.IndexBufferOffset;
  }
  void IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) {
    state_.InputAssembler.Topology = Topology;
  }
  void IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY *pTopology) {
    if (pTopology) {
      *pTopology = state_.InputAssembler.Topology;
    }
  }
#pragma endregion

#pragma region VertexShader

  void VSSetShader(ID3D11VertexShader *pVertexShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) {
    ctx.SetShader<ShaderType::Vertex, ID3D11VertexShader>(
        pVertexShader, ppClassInstances, NumClassInstances);
  }

  void VSGetShader(ID3D11VertexShader **ppVertexShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) {
    ctx.GetShader<ShaderType::Vertex, ID3D11VertexShader>(
        ppVertexShader, ppClassInstances, pNumClassInstances);
  }

  void
  VSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    ctx.SetShaderResource<ShaderType::Vertex>(StartSlot, NumViews,
                                              ppShaderResourceViews);
  }

  void VSGetShaderResources(UINT StartSlot, UINT NumViews,
                            ID3D11ShaderResourceView **ppShaderResourceViews) {
    ctx.GetShaderResource<ShaderType::Vertex>(StartSlot, NumViews,
                                              ppShaderResourceViews);
  }

  void VSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) {
    ctx.SetSamplers<ShaderType::Vertex>(StartSlot, NumSamplers, ppSamplers);
  }

  void VSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) {
    ctx.GetSamplers<ShaderType::Vertex>(StartSlot, NumSamplers, ppSamplers);
  }

  void VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer *const *ppConstantBuffers) {
    VSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) {
    VSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void VSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer *const *ppConstantBuffers,
                             const UINT *pFirstConstant,
                             const UINT *pNumConstants) {
    ctx.SetConstantBuffer<ShaderType::Vertex>(StartSlot, NumBuffers,
                                              ppConstantBuffers, pFirstConstant,
                                              pNumConstants);
  }

  void VSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant, UINT *pNumConstants) {
    ctx.GetConstantBuffer<ShaderType::Vertex>(StartSlot, NumBuffers,
                                              ppConstantBuffers, pFirstConstant,
                                              pNumConstants);
  }

#pragma endregion

#pragma region PixelShader

  void PSSetShader(ID3D11PixelShader *pPixelShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) {
    ctx.SetShader<ShaderType::Pixel, ID3D11PixelShader>(
        pPixelShader, ppClassInstances, NumClassInstances);
  }

  void PSGetShader(ID3D11PixelShader **ppPixelShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) {
    ctx.GetShader<ShaderType::Pixel, ID3D11PixelShader>(
        ppPixelShader, ppClassInstances, pNumClassInstances);
  }

  void
  PSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    ctx.SetShaderResource<ShaderType::Pixel>(StartSlot, NumViews,
                                             ppShaderResourceViews);
  }

  void PSGetShaderResources(UINT StartSlot, UINT NumViews,
                            ID3D11ShaderResourceView **ppShaderResourceViews) {
    ctx.GetShaderResource<ShaderType::Pixel>(StartSlot, NumViews,
                                             ppShaderResourceViews);
  }

  void PSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) {
    ctx.SetSamplers<ShaderType::Pixel>(StartSlot, NumSamplers, ppSamplers);
  }

  void PSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) {

    ctx.GetSamplers<ShaderType::Pixel>(StartSlot, NumSamplers, ppSamplers);
  }

  void PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer *const *ppConstantBuffers) {
    PSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, 0, 0);
  }

  void PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) {
    PSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, 0, 0);
  }

  void PSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer *const *ppConstantBuffers,
                             const UINT *pFirstConstant,
                             const UINT *pNumConstants) {
    ctx.SetConstantBuffer<ShaderType::Pixel>(StartSlot, NumBuffers,
                                             ppConstantBuffers, pFirstConstant,
                                             pNumConstants);
  }

  void PSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant, UINT *pNumConstants) {
    ctx.GetConstantBuffer<ShaderType::Pixel>(StartSlot, NumBuffers,
                                             ppConstantBuffers, pFirstConstant,
                                             pNumConstants);
  }

#pragma endregion

#pragma region GeometryShader

  void GSSetShader(ID3D11GeometryShader *pShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) {
    ctx.SetShader<ShaderType::Geometry>(pShader, ppClassInstances,
                                        NumClassInstances);
  }

  void GSGetShader(ID3D11GeometryShader **ppGeometryShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) {
    ctx.GetShader<ShaderType::Geometry>(ppGeometryShader, ppClassInstances,
                                        pNumClassInstances);
  }

  void GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer *const *ppConstantBuffers) {
    GSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void GSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer *const *ppConstantBuffers,
                             const UINT *pFirstConstant,
                             const UINT *pNumConstants) {
    ctx.SetConstantBuffer<ShaderType::Geometry>(StartSlot, NumBuffers,
                                                ppConstantBuffers,
                                                pFirstConstant, pNumConstants);
  }

  void GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) {
    GSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void GSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant, UINT *pNumConstants) {
    ctx.GetConstantBuffer<ShaderType::Geometry>(StartSlot, NumBuffers,
                                                ppConstantBuffers,
                                                pFirstConstant, pNumConstants);
  }

  void
  GSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    ctx.SetShaderResource<ShaderType::Geometry>(StartSlot, NumViews,
                                                ppShaderResourceViews);
  }

  void GSGetShaderResources(UINT StartSlot, UINT NumViews,
                            ID3D11ShaderResourceView **ppShaderResourceViews) {
    ctx.GetShaderResource<ShaderType::Geometry>(StartSlot, NumViews,
                                                ppShaderResourceViews);
  }

  void GSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) {
    ctx.SetSamplers<ShaderType::Geometry>(StartSlot, NumSamplers, ppSamplers);
  }

  void GSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) {
    ctx.GetSamplers<ShaderType::Geometry>(StartSlot, NumSamplers, ppSamplers);
  }

  void SOSetTargets(UINT NumBuffers, ID3D11Buffer *const *ppSOTargets,
                    const UINT *pOffsets) {
    IMPLEMENT_ME
  }

  void SOGetTargets(UINT NumBuffers, ID3D11Buffer **ppSOTargets) {
    IMPLEMENT_ME
  }

#pragma endregion

#pragma region HullShader

  void HSGetShaderResources(UINT StartSlot, UINT NumViews,
                            ID3D11ShaderResourceView **ppShaderResourceViews) {
    ctx.GetShaderResource<ShaderType::Hull>(StartSlot, NumViews,
                                            ppShaderResourceViews);
  }

  void HSGetShader(ID3D11HullShader **ppHullShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) {
    ctx.GetShader<ShaderType::Hull>(ppHullShader, ppClassInstances,
                                    pNumClassInstances);
  }

  void HSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) {
    ctx.GetSamplers<ShaderType::Hull>(StartSlot, NumSamplers, ppSamplers);
  }

  void HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) {
    HSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void
  HSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    ctx.SetShaderResource<ShaderType::Hull>(StartSlot, NumViews,
                                            ppShaderResourceViews);
  }

  void HSSetShader(ID3D11HullShader *pHullShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) {
    ctx.SetShader<ShaderType::Hull>(pHullShader, ppClassInstances,
                                    NumClassInstances);
  }

  void HSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) {
    ctx.SetSamplers<ShaderType::Hull>(StartSlot, NumSamplers, ppSamplers);
  }

  void HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer *const *ppConstantBuffers) {
    HSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void HSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer *const *ppConstantBuffers,
                             const UINT *pFirstConstant,
                             const UINT *pNumConstants) {
    ctx.SetConstantBuffer<ShaderType::Hull>(StartSlot, NumBuffers,
                                            ppConstantBuffers, pFirstConstant,
                                            pNumConstants);
  }

  void HSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant, UINT *pNumConstants) {
    ctx.GetConstantBuffer<ShaderType::Hull>(StartSlot, NumBuffers,
                                            ppConstantBuffers, pFirstConstant,
                                            pNumConstants);
  }

#pragma endregion

#pragma region DomainShader

  void
  DSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    ctx.SetShaderResource<ShaderType::Domain>(StartSlot, NumViews,
                                              ppShaderResourceViews);
  }

  void DSGetShaderResources(UINT StartSlot, UINT NumViews,
                            ID3D11ShaderResourceView **ppShaderResourceViews) {
    ctx.GetShaderResource<ShaderType::Domain>(StartSlot, NumViews,
                                              ppShaderResourceViews);
  }

  void DSSetShader(ID3D11DomainShader *pDomainShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) {
    ctx.SetShader<ShaderType::Domain, ID3D11DomainShader>(
        pDomainShader, ppClassInstances, NumClassInstances);
  }

  void DSGetShader(ID3D11DomainShader **ppDomainShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) {
    ctx.GetShader<ShaderType::Domain, ID3D11DomainShader>(
        ppDomainShader, ppClassInstances, pNumClassInstances);
  }

  void DSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) {
    ctx.GetSamplers<ShaderType::Domain>(StartSlot, NumSamplers, ppSamplers);
  }

  void DSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) {
    ctx.SetSamplers<ShaderType::Domain>(StartSlot, NumSamplers, ppSamplers);
  }

  void DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer *const *ppConstantBuffers) {
    return DSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL,
                                 NULL);
  }

  void DSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer *const *ppConstantBuffers,
                             const UINT *pFirstConstant,
                             const UINT *pNumConstants) {
    ctx.SetConstantBuffer<ShaderType::Domain>(StartSlot, NumBuffers,
                                              ppConstantBuffers, pFirstConstant,
                                              pNumConstants);
  }

  void DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) {
    UINT *pFirstConstant = 0, *pNumConstants = 0;
    return DSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers,
                                 pFirstConstant, pNumConstants);
  }

  void DSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant, UINT *pNumConstants) {
    ctx.GetConstantBuffer<ShaderType::Domain>(StartSlot, NumBuffers,
                                              ppConstantBuffers, pFirstConstant,
                                              pNumConstants);
  }

#pragma endregion

#pragma region ComputeShader

  void CSGetShaderResources(UINT StartSlot, UINT NumViews,
                            ID3D11ShaderResourceView **ppShaderResourceViews) {
    ctx.GetShaderResource<ShaderType::Compute>(StartSlot, NumViews,
                                               ppShaderResourceViews);
  }

  void
  CSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    ctx.SetShaderResource<ShaderType::Compute>(StartSlot, NumViews,
                                               ppShaderResourceViews);
  }

  void CSSetUnorderedAccessViews(
      UINT StartSlot, UINT NumUAVs,
      ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
      const UINT *pUAVInitialCounts) {

    std::erase_if(state_.ComputeStageUAV.UAVs, [&](const auto &item) -> bool {
      auto &[slot, bound_uav] = item;
      if (slot < StartSlot || slot >= (StartSlot + NumUAVs))
        return false;
      for (auto i = 0u; i < NumUAVs; i++) {
        if (auto uav = static_cast<IMTLD3D11UnorderedAccessView *>(
                ppUnorderedAccessViews[i])) {
          // FIXME! GetViewRange is not defined on IMTLBindable
          // if (bound_uav.View->GetViewRange().CheckOverlap(
          //         uav->GetViewRange())) {
          //   return true;
          // }
        }
      }
      return false;
    });

    for (auto i = 0u; i < NumUAVs; i++) {
      if (auto uav = com_cast<IMTLBindable>(ppUnorderedAccessViews[i])) {
        // bind
        UAV_B to_bind = {std::move(uav),
                         pUAVInitialCounts ? pUAVInitialCounts[i] : ~0u};
        state_.ComputeStageUAV.UAVs.insert_or_assign(StartSlot + i,
                                                     std::move(to_bind));
        // resolve srv hazard: unbind any cs srv that share the resource
        std::erase_if(state_.ShaderStages[5].SRVs,
                      [&](const auto &item) -> bool {
                        // auto &[slot, bound_srv] = item;
                        // if srv conflict with uav, return true
                        return false;
                      });
      } else {
        // unbind
        state_.ComputeStageUAV.UAVs.erase(StartSlot + i);
      }
    }
  }

  void CSGetUnorderedAccessViews(
      UINT StartSlot, UINT NumUAVs,
      ID3D11UnorderedAccessView **ppUnorderedAccessViews) {
    for (auto i = 0u; i < NumUAVs; i++) {
      if (state_.ComputeStageUAV.UAVs.contains(StartSlot + i)) {
        state_.ComputeStageUAV.UAVs[StartSlot + i].View->QueryInterface(
            IID_PPV_ARGS(&ppUnorderedAccessViews[i]));
      } else {
        ppUnorderedAccessViews[i] = nullptr;
      }
    }
  }

  void CSSetShader(ID3D11ComputeShader *pComputeShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) {
    ctx.SetShader<ShaderType::Compute>(pComputeShader, ppClassInstances,
                                       NumClassInstances);
  }

  void CSGetShader(ID3D11ComputeShader **ppComputeShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) {
    ctx.GetShader<ShaderType::Compute>(ppComputeShader, ppClassInstances,
                                       pNumClassInstances);
  }

  void CSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) {
    ctx.SetSamplers<ShaderType::Compute>(StartSlot, NumSamplers, ppSamplers);
  }

  void CSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) {
    ctx.GetSamplers<ShaderType::Compute>(StartSlot, NumSamplers, ppSamplers);
  }

  void CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer *const *ppConstantBuffers) {
    CSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) {
    CSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void CSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer *const *ppConstantBuffers,
                             const UINT *pFirstConstant,
                             const UINT *pNumConstants) {
    ctx.SetConstantBuffer<ShaderType::Compute>(StartSlot, NumBuffers,
                                               ppConstantBuffers,
                                               pFirstConstant, pNumConstants);
  }

  void CSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant, UINT *pNumConstants) {
    ctx.GetConstantBuffer<ShaderType::Compute>(StartSlot, NumBuffers,
                                               ppConstantBuffers,
                                               pFirstConstant, pNumConstants);
  }

#pragma endregion

#pragma region OutputMerger

  void OMSetRenderTargets(UINT NumViews,
                          ID3D11RenderTargetView *const *ppRenderTargetViews,
                          ID3D11DepthStencilView *pDepthStencilView) {
    OMSetRenderTargetsAndUnorderedAccessViews(
        NumViews, ppRenderTargetViews, pDepthStencilView, 0, 0, NULL, NULL);
  }

  void OMGetRenderTargets(UINT NumViews,
                          ID3D11RenderTargetView **ppRenderTargetViews,
                          ID3D11DepthStencilView **ppDepthStencilView) {
    OMGetRenderTargetsAndUnorderedAccessViews(NumViews, ppRenderTargetViews,
                                              ppDepthStencilView, 0, 0, NULL);
  }

  void OMSetRenderTargetsAndUnorderedAccessViews(
      UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews,
      ID3D11DepthStencilView *pDepthStencilView, UINT UAVStartSlot,
      UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
      const UINT *pUAVInitialCounts) {

    auto &BoundRTVs = state_.OutputMerger.RTVs;
    constexpr unsigned RTVSlotCount = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
    for (unsigned rtv_index = 0; rtv_index < RTVSlotCount; rtv_index++) {
      if (rtv_index < NumRTVs && ppRenderTargetViews[rtv_index]) {
        if (auto expected = com_cast<IMTLD3D11RenderTargetView>(
                ppRenderTargetViews[rtv_index])) {
          BoundRTVs[rtv_index] = std::move(expected);
        } else {
          assert(0 && "wtf");
        }
      } else {
        BoundRTVs[rtv_index] = nullptr;
      }
    }
    state_.OutputMerger.NumRTVs = NumRTVs;

    if (auto expected =
            com_cast<IMTLD3D11DepthStencilView>(pDepthStencilView)) {
      state_.OutputMerger.DSV = std::move(expected);
    }

    if (NumUAVs) {
      IMPLEMENT_ME
    }

    ctx.InvalidateCurrentPass();
  }

  void OMGetRenderTargetsAndUnorderedAccessViews(
      UINT NumRTVs, ID3D11RenderTargetView **ppRenderTargetViews,
      ID3D11DepthStencilView **ppDepthStencilView, UINT UAVStartSlot,
      UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews) {
    IMPLEMENT_ME;
  }

  void OMSetBlendState(ID3D11BlendState *pBlendState,
                       const FLOAT BlendFactor[4], UINT SampleMask) {
    if (auto expected = com_cast<IMTLD3D11BlendState>(pBlendState)) {
      state_.OutputMerger.BlendState = std::move(expected);
      memcpy(state_.OutputMerger.BlendFactor, BlendFactor, sizeof(float[4]));
      state_.OutputMerger.SampleMask = SampleMask;
    }

    ctx.InvalidateGraphicPipeline();
  }
  void OMGetBlendState(ID3D11BlendState **ppBlendState, FLOAT BlendFactor[4],
                       UINT *pSampleMask) {
    if (ppBlendState) {
      *ppBlendState = state_.OutputMerger.BlendState.ref();
    }
    if (BlendFactor) {
      memcpy(BlendFactor, state_.OutputMerger.BlendFactor, sizeof(float[4]));
    }
    if (pSampleMask) {
      *pSampleMask = state_.OutputMerger.SampleMask;
    }
  }

  void OMSetDepthStencilState(ID3D11DepthStencilState *pDepthStencilState,
                              UINT StencilRef) {
    if (auto expected =
            com_cast<IMTLD3D11DepthStencilState>(pDepthStencilState)) {
      state_.OutputMerger.DepthStencilState = std::move(expected);
      state_.OutputMerger.StencilRef = StencilRef;
      ctx.EmitSetDepthStencilState();
    }
  }

  void OMGetDepthStencilState(ID3D11DepthStencilState **ppDepthStencilState,
                              UINT *pStencilRef) {
    if (ppDepthStencilState) {
      *ppDepthStencilState = state_.OutputMerger.DepthStencilState.ref();
    }
    if (pStencilRef) {
      *pStencilRef = state_.OutputMerger.StencilRef;
    }
  }

#pragma endregion

#pragma region Rasterizer

  void RSSetState(ID3D11RasterizerState *pRasterizerState) {
    if (pRasterizerState) {
      if (auto expected =
              com_cast<IMTLD3D11RasterizerState>(pRasterizerState)) {
        state_.Rasterizer.RasterizerState = expected;
      } else {
        ERR("RSSetState: invalid ID3D11RasterizerState object.");
      }
    } else {
      state_.Rasterizer.RasterizerState = nullptr;
    }
    ctx.EmitSetRasterizerState();
  }

  void RSGetState(ID3D11RasterizerState **ppRasterizerState) {
    if (ppRasterizerState) {
      if (state_.Rasterizer.RasterizerState) {
        state_.Rasterizer.RasterizerState->QueryInterface(
            IID_PPV_ARGS(ppRasterizerState));
      } else {
        *ppRasterizerState = NULL;
      }
    }
  }

  void RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT *pViewports) {
    state_.Rasterizer.NumViewports = NumViewports;
    for (auto i = 0u; i < NumViewports; i++) {
      state_.Rasterizer.viewports[i] = pViewports[i];
    }
    ctx.EmitSetViewport();
  }

  void RSGetViewports(UINT *pNumViewports, D3D11_VIEWPORT *pViewports) {
    if (pNumViewports) {
      *pNumViewports = state_.Rasterizer.NumViewports;
    }
    if (pViewports) {
      for (auto i = 0u; i < state_.Rasterizer.NumViewports; i++) {
        pViewports[i] = state_.Rasterizer.viewports[i];
      }
    }
  }

  void RSSetScissorRects(UINT NumRects, const D3D11_RECT *pRects) {
    state_.Rasterizer.NumScissorRects = NumRects;
    for (unsigned i = 0; i < NumRects; i++) {
      state_.Rasterizer.scissor_rects[i] = pRects[i];
    }
    ctx.EmitSetScissor();
  }

  void RSGetScissorRects(UINT *pNumRects, D3D11_RECT *pRects) { IMPLEMENT_ME; }
#pragma endregion

#pragma region DynamicBufferPool

  void ExchangeFromPool(MTL::Buffer **pBuffer) final {
    /**
    TODO: there is no pool at all!
    */
    assert(*pBuffer);
    auto original = transfer(*pBuffer);
    *pBuffer = metal_device_->newBuffer(original->length(),
                                        original->resourceOptions());
    cmd_queue.CurrentChunk()->emit(
        [_ = std::move(original)](CommandChunk::context &ctx) {
          /**
          abusing lambda capture
          the original buffer will be released once the chunk has completed
          */
        });
  }

#pragma endregion

#pragma region Misc

  D3D11_DEVICE_CONTEXT_TYPE GetType() { return D3D11_DEVICE_CONTEXT_IMMEDIATE; }

  UINT GetContextFlags() { return 0; }

#pragma endregion

  void Flush2(std::function<void(MTL::CommandBuffer *)> &&beforeCommit) final {
    ctx.InvalidateCurrentPass();
    cmd_queue.CurrentChunk()->emit(
        [bc = std::move(beforeCommit)](CommandChunk::context &ctx) {
          bc(ctx.cmdbuf);
        });
    cmd_queue.CommitCurrentChunk();
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

  virtual void WaitUntilGPUIdle() { cmd_queue.WaitCPUFence(0); };
};

Com<IMTLD3D11DeviceContext> CreateD3D11DeviceContext(IMTLD3D11Device *pDevice) {
  return new MTLD3D11DeviceContext(pDevice);
}

} // namespace dxmt