#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLCommandEncoder.hpp"
#include "Metal/MTLPixelFormat.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLRenderPass.hpp"
#include "Metal/MTLStageInputOutputDescriptor.hpp"
#include "Metal/MTLTexture.hpp"
#include "com/com_guid.hpp"
#include "com/com_pointer.hpp"
#include "d3d11_device_child.hpp"
#include "d3d11_context_state.hpp"

#include "Metal/MTLCommandBuffer.hpp"

#include "d3d11_input_layout.hpp"
#include "d3d11_pipeline.hpp"
#include "d3d11_private.h"
#include "d3d11_query.hpp"
#include "d3d11_context.hpp"
#include "d3d11_device.hpp"
#include "d3d11_shader.hpp"
#include "d3d11_state_object.hpp"
#include "d3d11_view.hpp"
#include "log/log.hpp"
#include "mtld11_interfaces.hpp"
#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"
#include "util_flags.hpp"

#include "dxmt_command_queue.hpp"
#include <cassert>
#include <cstddef>
#include <cstring>

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

enum class BINDING_TYPE { Buffer, Texture };

struct BINDING_TEXTURE {
  MTL::Texture *Ptr;
  // TODO: minlod
};

struct BINDING_BUFFER {
  MTL::Buffer *Ptr;
  UINT Offset;
  // TODO: length
};

struct BINDING_INFO {
  UINT ArgumentIndex;
  BINDING_TYPE Type; // 0 buffer, 1 texture, 2 sampler
  union {
    BINDING_TEXTURE Texture;
    BINDING_BUFFER Buffer;
  } U;
};

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
        Out.pData = dynamic->GetCurrentBuffer()->contents();
        break;
      }
      case D3D11_MAP_WRITE_NO_OVERWRITE: {
        Out.pData = dynamic->GetCurrentBuffer()->contents();
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
      InvalidateCurrentPass();
      CommandChunk *chk = cmd_queue.CurrentChunk();
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

      InvalidateCurrentPass();
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
    PreDraw();
    MTL::PrimitiveType Primitive =
        to_metal_topology(state_.InputAssembler.Topology);
    // TODO: skip invalid topology
    EmitRenderCommand([Primitive, StartVertexLocation,
                       VertexCount](MTL::RenderCommandEncoder *encoder) {
      encoder->drawPrimitives(Primitive, StartVertexLocation, VertexCount);
    });
  }

  void DrawIndexed(UINT IndexCount, UINT StartIndexLocation,
                   INT BaseVertexLocation) {
    PreDraw();
    MTL::PrimitiveType Primitive =
        to_metal_topology(state_.InputAssembler.Topology);
    MTL_BIND_RESOURCE IndexBuffer;
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
    EmitRenderCommand<>(
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
    PreDraw();
    MTL::PrimitiveType Primitive =
        to_metal_topology(state_.InputAssembler.Topology);
    // TODO: skip invalid topology
    EmitRenderCommand<>(
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
    IMPLEMENT_ME
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
    IMPLEMENT_ME
  }

  void DispatchIndirect(ID3D11Buffer *pBufferForArgs,
                        UINT AlignedByteOffsetForArgs) {
    IMPLEMENT_ME
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
    InvalidateGraphicPipeline();
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
      if (auto dynamic =
              com_cast<IMTLDynamicBuffer>(ppVertexBuffers[Slot - StartSlot])) {
        dynamic->GetBindable(&VB.Buffer, [this]() { EmitSetIAState(); });
      } else if (auto expected = com_cast<IMTLBindable>(
                     ppVertexBuffers[Slot - StartSlot])) {
        VB.Buffer = std::move(expected);
      } else if (ppVertexBuffers[Slot - StartSlot]) {
        ERR("IASetVertexBuffers: wrong vertex buffer binding");
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
    if (auto dynamic = com_cast<IMTLDynamicBuffer>(pIndexBuffer)) {
      dynamic->GetBindable(&state_.InputAssembler.IndexBuffer, []() {});
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

#pragma region ShaderCommon

  template <ShaderType Type, typename IShader>
  void SetShader(IShader *pShader, ID3D11ClassInstance *const *ppClassInstances,
                 UINT NumClassInstances) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    if (auto expected = com_cast<IMTLD3D11Shader>(pShader)) {
      ShaderStage.Shader = std::move(expected);
    } else {
      assert(0 && "wtf");
    }

    if (NumClassInstances)
      ERR("Class instances not supported");

    if (Type == ShaderType::Compute) {
      InvalidateComputePipeline();
    } else {
      InvalidateGraphicPipeline();
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

    for (unsigned i = StartSlot; i < StartSlot + NumBuffers; i++) {
      auto pConstantBuffer = ppConstantBuffers[i - StartSlot];
      if (pConstantBuffer) {
        auto maybeExist = ShaderStage.ConstantBuffers.insert({i, {}});
        // either old value or inserted empty value
        auto &entry = maybeExist.first->second;
        entry.Buffer = nullptr; // dereference old
        entry.FirstConstant =
            pFirstConstant ? pFirstConstant[i - StartSlot] : 0;
        entry.NumConstants = pNumConstants ? pNumConstants[i - StartSlot]
                                           : 4096; // FIXME: too much
        if (auto dynamic = com_cast<IMTLDynamicBuffer>(pConstantBuffer)) {
          dynamic->GetBindable(&entry.Buffer,
                               [this]() { binding_ready.clr(Type); });
        } else if (auto expected = com_cast<IMTLBindable>(pConstantBuffer)) {
          entry.Buffer = std::move(expected);
        } else {
          assert(0 && "wtf");
        }
      } else {
        // BIND NULL
        ShaderStage.ConstantBuffers.erase(i);
      }
    }

    binding_ready.clr(Type);
  }

  template <ShaderType Type>
  void GetConstantBuffer(UINT StartSlot, UINT NumBuffers,
                         ID3D11Buffer *const *ppConstantBuffers,
                         const UINT *pFirstConstant,
                         const UINT *pNumConstants) {

    // auto &ShaderStage = state_.ShaderStages[(UINT)Type];
  }

  template <ShaderType Type>
  void
  SetShaderResource(UINT StartSlot, UINT NumViews,
                    ID3D11ShaderResourceView *const *ppShaderResourceViews) {

    // auto &ShaderStage = state_.ShaderStages[(UINT)Type];

    binding_ready.clr(Type);
  }

  template <ShaderType Type>
  void GetShaderResource(UINT StartSlot, UINT NumViews,
                         ID3D11ShaderResourceView **ppShaderResourceViews) {
    // auto &ShaderStage = state_.ShaderStages[(UINT)Type];
  }

  template <ShaderType Type>
  void SetSamplers(UINT StartSlot, UINT NumSamplers,
                   ID3D11SamplerState *const *ppSamplers) {
    // auto &ShaderStage = state_.ShaderStages[(UINT)Type];
    for (unsigned Slot = StartSlot; Slot < StartSlot + NumSamplers; Slot++) {
    }

    binding_ready.clr(Type);
  }

  template <ShaderType Type>
  void GetSamplers(UINT StartSlot, UINT NumSamplers,
                   ID3D11SamplerState **ppSamplers) {
    auto &ShaderStage = state_.ShaderStages[(UINT)Type];
    if (ppSamplers) {
      for (unsigned Slot = StartSlot; Slot < StartSlot + NumSamplers; Slot++) {
        if (ShaderStage.Samplers.contains(Slot)) {
          ppSamplers[Slot - StartSlot] = ShaderStage.Samplers[Slot].ref();
        }
      }
    }
  }

#pragma endregion

#pragma region VertexShader

  void VSSetShader(ID3D11VertexShader *pVertexShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) {
    SetShader<ShaderType::Vertex, ID3D11VertexShader>(
        pVertexShader, ppClassInstances, NumClassInstances);
  }

  void VSGetShader(ID3D11VertexShader **ppVertexShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) {
    GetShader<ShaderType::Vertex, ID3D11VertexShader>(
        ppVertexShader, ppClassInstances, pNumClassInstances);
  }

  void
  VSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    SetShaderResource<ShaderType::Vertex>(StartSlot, NumViews,
                                          ppShaderResourceViews);
  }

  void VSGetShaderResources(UINT StartSlot, UINT NumViews,
                            ID3D11ShaderResourceView **ppShaderResourceViews) {
    GetShaderResource<ShaderType::Vertex>(StartSlot, NumViews,
                                          ppShaderResourceViews);
  }

  void VSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) {
    SetSamplers<ShaderType::Vertex>(StartSlot, NumSamplers, ppSamplers);
  }

  void VSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) {
    GetSamplers<ShaderType::Vertex>(StartSlot, NumSamplers, ppSamplers);
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
    SetConstantBuffer<ShaderType::Vertex>(StartSlot, NumBuffers,
                                          ppConstantBuffers, pFirstConstant,
                                          pNumConstants);
  }

  void VSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant, UINT *pNumConstants) {
    GetConstantBuffer<ShaderType::Vertex>(StartSlot, NumBuffers,
                                          ppConstantBuffers, pFirstConstant,
                                          pNumConstants);
  }

#pragma endregion

#pragma region PixelShader

  void PSSetShader(ID3D11PixelShader *pPixelShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) {
    SetShader<ShaderType::Pixel, ID3D11PixelShader>(
        pPixelShader, ppClassInstances, NumClassInstances);
  }

  void PSGetShader(ID3D11PixelShader **ppPixelShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) {
    GetShader<ShaderType::Pixel, ID3D11PixelShader>(
        ppPixelShader, ppClassInstances, pNumClassInstances);
  }

  void
  PSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    SetShaderResource<ShaderType::Pixel>(StartSlot, NumViews,
                                         ppShaderResourceViews);
  }

  void PSGetShaderResources(UINT StartSlot, UINT NumViews,
                            ID3D11ShaderResourceView **ppShaderResourceViews) {
    GetShaderResource<ShaderType::Pixel>(StartSlot, NumViews,
                                         ppShaderResourceViews);
  }

  void PSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) {
    SetSamplers<ShaderType::Pixel>(StartSlot, NumSamplers, ppSamplers);
  }

  void PSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) {

    GetSamplers<ShaderType::Pixel>(StartSlot, NumSamplers, ppSamplers);
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
    SetConstantBuffer<ShaderType::Pixel>(StartSlot, NumBuffers,
                                         ppConstantBuffers, pFirstConstant,
                                         pNumConstants);
  }

  void PSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant, UINT *pNumConstants) {
    GetConstantBuffer<ShaderType::Pixel>(StartSlot, NumBuffers,
                                         ppConstantBuffers, pFirstConstant,
                                         pNumConstants);
  }

#pragma endregion

#pragma region GeometryShader

  void GSSetShader(ID3D11GeometryShader *pShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) {
    SetShader<ShaderType::Geometry>(pShader, ppClassInstances,
                                    NumClassInstances);
  }

  void GSGetShader(ID3D11GeometryShader **ppGeometryShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) {
    GetShader<ShaderType::Geometry>(ppGeometryShader, ppClassInstances,
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
    SetConstantBuffer<ShaderType::Geometry>(StartSlot, NumBuffers,
                                            ppConstantBuffers, pFirstConstant,
                                            pNumConstants);
  }

  void GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) {
    GSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void GSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant, UINT *pNumConstants) {
    GetConstantBuffer<ShaderType::Geometry>(StartSlot, NumBuffers,
                                            ppConstantBuffers, pFirstConstant,
                                            pNumConstants);
  }

  void
  GSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    SetShaderResource<ShaderType::Geometry>(StartSlot, NumViews,
                                            ppShaderResourceViews);
  }

  void GSGetShaderResources(UINT StartSlot, UINT NumViews,
                            ID3D11ShaderResourceView **ppShaderResourceViews) {
    GetShaderResource<ShaderType::Geometry>(StartSlot, NumViews,
                                            ppShaderResourceViews);
  }

  void GSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) {
    SetSamplers<ShaderType::Geometry>(StartSlot, NumSamplers, ppSamplers);
  }

  void GSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) {
    GetSamplers<ShaderType::Geometry>(StartSlot, NumSamplers, ppSamplers);
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
    GetShaderResource<ShaderType::Hull>(StartSlot, NumViews,
                                        ppShaderResourceViews);
  }

  void HSGetShader(ID3D11HullShader **ppHullShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) {
    GetShader<ShaderType::Hull>(ppHullShader, ppClassInstances,
                                pNumClassInstances);
  }

  void HSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) {
    GetSamplers<ShaderType::Hull>(StartSlot, NumSamplers, ppSamplers);
  }

  void HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer **ppConstantBuffers) {
    HSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void
  HSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    SetShaderResource<ShaderType::Hull>(StartSlot, NumViews,
                                        ppShaderResourceViews);
  }

  void HSSetShader(ID3D11HullShader *pHullShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) {
    SetShader<ShaderType::Hull>(pHullShader, ppClassInstances,
                                NumClassInstances);
  }

  void HSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) {
    SetSamplers<ShaderType::Hull>(StartSlot, NumSamplers, ppSamplers);
  }

  void HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers,
                            ID3D11Buffer *const *ppConstantBuffers) {
    HSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void HSSetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer *const *ppConstantBuffers,
                             const UINT *pFirstConstant,
                             const UINT *pNumConstants) {
    SetConstantBuffer<ShaderType::Hull>(StartSlot, NumBuffers,
                                        ppConstantBuffers, pFirstConstant,
                                        pNumConstants);
  }

  void HSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant, UINT *pNumConstants) {
    GetConstantBuffer<ShaderType::Hull>(StartSlot, NumBuffers,
                                        ppConstantBuffers, pFirstConstant,
                                        pNumConstants);
  }

#pragma endregion

#pragma region DomainShader

  void
  DSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    SetShaderResource<ShaderType::Domain>(StartSlot, NumViews,
                                          ppShaderResourceViews);
  }

  void DSGetShaderResources(UINT StartSlot, UINT NumViews,
                            ID3D11ShaderResourceView **ppShaderResourceViews) {
    GetShaderResource<ShaderType::Domain>(StartSlot, NumViews,
                                          ppShaderResourceViews);
  }

  void DSSetShader(ID3D11DomainShader *pDomainShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) {
    SetShader<ShaderType::Domain, ID3D11DomainShader>(
        pDomainShader, ppClassInstances, NumClassInstances);
  }

  void DSGetShader(ID3D11DomainShader **ppDomainShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) {
    GetShader<ShaderType::Domain, ID3D11DomainShader>(
        ppDomainShader, ppClassInstances, pNumClassInstances);
  }

  void DSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) {
    GetSamplers<ShaderType::Domain>(StartSlot, NumSamplers, ppSamplers);
  }

  void DSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) {
    SetSamplers<ShaderType::Domain>(StartSlot, NumSamplers, ppSamplers);
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
    SetConstantBuffer<ShaderType::Domain>(StartSlot, NumBuffers,
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
    GetConstantBuffer<ShaderType::Domain>(StartSlot, NumBuffers,
                                          ppConstantBuffers, pFirstConstant,
                                          pNumConstants);
  }

#pragma endregion

#pragma region ComputeShader

  void CSGetShaderResources(UINT StartSlot, UINT NumViews,
                            ID3D11ShaderResourceView **ppShaderResourceViews) {
    GetShaderResource<ShaderType::Compute>(StartSlot, NumViews,
                                           ppShaderResourceViews);
  }

  void
  CSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    SetShaderResource<ShaderType::Compute>(StartSlot, NumViews,
                                           ppShaderResourceViews);
  }

  void CSSetUnorderedAccessViews(
      UINT StartSlot, UINT NumUAVs,
      ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
      const UINT *pUAVInitialCounts) {
    IMPLEMENT_ME
  }

  void CSGetUnorderedAccessViews(
      UINT StartSlot, UINT NumUAVs,
      ID3D11UnorderedAccessView **ppUnorderedAccessViews) {
    IMPLEMENT_ME
  }

  void CSSetShader(ID3D11ComputeShader *pComputeShader,
                   ID3D11ClassInstance *const *ppClassInstances,
                   UINT NumClassInstances) {
    SetShader<ShaderType::Compute>(pComputeShader, ppClassInstances,
                                   NumClassInstances);
  }

  void CSGetShader(ID3D11ComputeShader **ppComputeShader,
                   ID3D11ClassInstance **ppClassInstances,
                   UINT *pNumClassInstances) {
    GetShader<ShaderType::Compute>(ppComputeShader, ppClassInstances,
                                   pNumClassInstances);
  }

  void CSSetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState *const *ppSamplers) {
    SetSamplers<ShaderType::Compute>(StartSlot, NumSamplers, ppSamplers);
  }

  void CSGetSamplers(UINT StartSlot, UINT NumSamplers,
                     ID3D11SamplerState **ppSamplers) {
    GetSamplers<ShaderType::Compute>(StartSlot, NumSamplers, ppSamplers);
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
    SetConstantBuffer<ShaderType::Compute>(StartSlot, NumBuffers,
                                           ppConstantBuffers, pFirstConstant,
                                           pNumConstants);
  }

  void CSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                             ID3D11Buffer **ppConstantBuffers,
                             UINT *pFirstConstant, UINT *pNumConstants) {
    GetConstantBuffer<ShaderType::Compute>(StartSlot, NumBuffers,
                                           ppConstantBuffers, pFirstConstant,
                                           pNumConstants);
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

    InvalidateCurrentPass();
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

    InvalidateGraphicPipeline();
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
      EmitSetDepthStencilState();
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
    EmitSetRasterizerState();
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
    for (unsigned i = 0; i < NumViewports; i++) {
      state_.Rasterizer.viewports[i] = pViewports[i];
    }
    EmitSetViewport();
  }

  void RSGetViewports(UINT *pNumViewports, D3D11_VIEWPORT *pViewports) {
    IMPLEMENT_ME;
  }

  void RSSetScissorRects(UINT NumRects, const D3D11_RECT *pRects) {
    state_.Rasterizer.NumScissorRects = NumRects;
    for (unsigned i = 0; i < NumRects; i++) {
      state_.Rasterizer.scissor_rects[i] = pRects[i];
    }
    EmitSetScissor();
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
    InvalidateCurrentPass();
    cmd_queue.CurrentChunk()->emit(
        [bc = std::move(beforeCommit)](CommandChunk::context &ctx) {
          bc(ctx.cmdbuf);
        });
    cmd_queue.CommitCurrentChunk();
  }

#pragma region CommandEncoder Maintain State

  /**
  Render pass can be invalidated by reasons:
  - render target changes (including depth stencil)
  All pass can be invalidated by reasons:
  - a encoder type switch
  - flush/present
  */
  void InvalidateCurrentPass() {
    CommandChunk *chk = cmd_queue.CurrentChunk();
    switch (cmdbuf_state) {
    case CommandBufferState::Idle:
      break;
    case CommandBufferState::RenderEncoderActive:
    case CommandBufferState::RenderPipelineReady:
      chk->emit([](CommandChunk::context &ctx) {
        ctx.render_encoder->endEncoding();
        ctx.vs_binding_encoder = nullptr;
        ctx.ps_binding_encoder = nullptr;
        ctx.render_encoder = nullptr;
      });
      break;
    case CommandBufferState::ComputeEncoderActive:
    case CommandBufferState::ComputePipelineReady:
      chk->emit([](CommandChunk::context &ctx) {
        ctx.compute_encoder->endEncoding();
        ctx.compute_encoder = nullptr;
      });
      break;
    case CommandBufferState::BlitEncoderActive:
      chk->emit([](CommandChunk::context &ctx) {
        ctx.blit_encoder->endEncoding();
        ctx.blit_encoder = nullptr;
      });
      break;
    }

    cmdbuf_state = CommandBufferState::Idle;
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
  void InvalidateGraphicPipeline() {
    if (cmdbuf_state != CommandBufferState::RenderPipelineReady)
      return;
    cmdbuf_state = CommandBufferState::RenderEncoderActive;

    CommandChunk *chk = cmd_queue.CurrentChunk();
    chk->emit([](CommandChunk::context &ctx) {
      ctx.vs_binding_encoder = nullptr;
      ctx.ps_binding_encoder = nullptr;
    });
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

  /**
  Switch to render encoder and set all states (expect for pipeline state)
  */
  void SwitchToRenderEncoder() {
    if (cmdbuf_state == CommandBufferState::RenderPipelineReady)
      return;
    if (cmdbuf_state == CommandBufferState::RenderEncoderActive)
      return;
    InvalidateCurrentPass();

    // should assume: render target is properly set
    {
      /* Setup RenderCommandEncoder */
      CommandChunk *chk = cmd_queue.CurrentChunk();

      struct RENDER_TARGET_STATE {
        Obj<MTL::Texture> Texture;
        UINT RenderTargetIndex;
        UINT MipSlice;
        UINT ArrayIndex;
        MTL::PixelFormat PixelFormat;
      };
      auto rtvs =
          chk->reserve_vector<RENDER_TARGET_STATE>(state_.OutputMerger.NumRTVs);
      for (unsigned i = 0; i < state_.OutputMerger.NumRTVs; i++) {
        auto &rtv = state_.OutputMerger.RTVs[i];
        rtvs.push_back(
            {rtv->GetCurrentTexture(), i, 0, 0, rtv->GetPixelFormat()});
      }
      struct DEPTH_STENCIL_STATE {
        Obj<MTL::Texture> Texture;
        UINT MipSlice;
        UINT ArrayIndex;
        MTL::PixelFormat PixelFormat;
      };
      auto &dsv = state_.OutputMerger.DSV;
      chk->emit(
          [rtvs = std::move(rtvs),
           dsv = DEPTH_STENCIL_STATE{
               dsv != nullptr ? dsv->GetCurrentTexture() : nullptr, 0, 0,
               dsv != nullptr
                   ? dsv->GetPixelFormat()
                   : MTL::PixelFormatInvalid}](CommandChunk::context &ctx) {
            auto renderPassDescriptor =
                MTL::RenderPassDescriptor::renderPassDescriptor();
            for (auto &rtv : rtvs) {
              auto colorAttachment =
                  renderPassDescriptor->colorAttachments()->object(
                      rtv.RenderTargetIndex);
              colorAttachment->setTexture(rtv.Texture);
              colorAttachment->setLevel(rtv.MipSlice);
              colorAttachment->setSlice(rtv.ArrayIndex);
              colorAttachment->setLoadAction(MTL::LoadActionLoad);
              colorAttachment->setStoreAction(MTL::StoreActionStore);
            };
            if (dsv.Texture) {
              // TODO: ...should know more about load/store behavior
              auto depthAttachment = renderPassDescriptor->depthAttachment();
              depthAttachment->setTexture(dsv.Texture);
              depthAttachment->setLevel(dsv.MipSlice);
              depthAttachment->setSlice(dsv.ArrayIndex);
              depthAttachment->setLoadAction(MTL::LoadActionLoad);
              depthAttachment->setStoreAction(MTL::StoreActionStore);

              // TODO: should know if depth buffer has stencil bits
              // auto stencilAttachment =
              // renderPassDescriptor->stencilAttachment();
              // stencilAttachment->setTexture(dsv.Texture);
              // stencilAttachment->setLevel(dsv.MipSlice);
              // stencilAttachment->setSlice(dsv.ArrayIndex);
              // stencilAttachment->setLoadAction(MTL::LoadActionLoad);
              // stencilAttachment->setStoreAction(MTL::StoreActionStore);
            }
            ctx.render_encoder =
                ctx.cmdbuf->renderCommandEncoder(renderPassDescriptor);
          });
    }

    cmdbuf_state = CommandBufferState::RenderEncoderActive;

    EmitSetDepthStencilState<true>();
    EmitSetRasterizerState<true>();
    EmitSetScissor<true>();
    EmitSetViewport<true>();
    EmitSetIAState<true>();
  }

  /**
  Switch to blit encoder
  */
  void SwitchToBlitEncoder() {
    if (cmdbuf_state == CommandBufferState::BlitEncoderActive)
      return;
    InvalidateCurrentPass();

    // TODO

    cmdbuf_state = CommandBufferState::BlitEncoderActive;
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

    // TODO

    cmdbuf_state = CommandBufferState::ComputeEncoderActive;
  }

  Com<IMTLCompiledGraphicsPipeline> temp;
  /**
  Assume we have all things needed to build PSO
  If the current encoder is not a render encoder, switch to it.
  */
  void FinalizeCurrentRenderPipeline() {
    if (cmdbuf_state == CommandBufferState::RenderPipelineReady)
      return;
    assert(state_.InputAssembler.InputLayout && "");

    SwitchToRenderEncoder();

    // TODO: Find PSO from cache?
    CommandChunk *chk = cmd_queue.CurrentChunk();

    if (!temp) {
      Com<IMTLCompiledShader> vs, ps;
      state_.ShaderStages[(UINT)ShaderType::Vertex]
          .Shader //
          ->GetCompiledShader(NULL, &vs);
      state_.ShaderStages[(UINT)ShaderType::Pixel]
          .Shader //
          ->GetCompiledShader(NULL, &ps);
      MTL::PixelFormat s[1] = {state_.OutputMerger.RTVs[0]->GetPixelFormat()};
      temp = CreateGraphicsPipeline(
          m_parent, vs.ptr(), ps.ptr(), state_.InputAssembler.InputLayout.ptr(),
          state_.OutputMerger.BlendState ? state_.OutputMerger.BlendState.ptr()
                                         : default_blend_state.ptr(),
          1, s,
          state_.OutputMerger.DSV ? state_.OutputMerger.DSV->GetPixelFormat()
                                  : MTL::PixelFormatInvalid,
          MTL::PixelFormatInvalid);
    }
    chk->emit([pso = temp](CommandChunk::context &ctx) {
      MTL_COMPILED_GRAPHICS_PIPELINE GraphicsPipeline;
      pso->GetPipeline(&GraphicsPipeline); // may block
      ctx.render_encoder->setRenderPipelineState(
          GraphicsPipeline.PipelineState);
      ctx.ps_binding_encoder = GraphicsPipeline.PSArgumentEncoder;
      ctx.vs_binding_encoder = GraphicsPipeline.VSArgumentEncoder;
    });

    cmdbuf_state = CommandBufferState::RenderPipelineReady;
  }

  void PreDraw() {
    FinalizeCurrentRenderPipeline();
    UploadShaderStageResourceBinding<ShaderType::Vertex>();
    UploadShaderStageResourceBinding<ShaderType::Pixel>();
  }

  /**
  Assume we have all things needed to build PSO
  If the current encoder is not a compute encoder, switch to it.
  */
  void FinalizeCurrentComputePipeline() {
    if (cmdbuf_state == CommandBufferState::ComputePipelineReady)
      return;
    SwitchToComputeEncoder();

    // TODO

    cmdbuf_state = CommandBufferState::ComputePipelineReady;
  }

  void PreDispatch() {
    FinalizeCurrentComputePipeline();
    UploadShaderStageResourceBinding<ShaderType::Compute>();
  }

  template <ShaderType stage, bool Force = false>
  void UploadShaderStageResourceBinding() {
    if (!Force && binding_ready.test(stage))
      return;

    auto &ShaderStage = state_.ShaderStages[(UINT)stage];

    auto BindingCount = ShaderStage.ConstantBuffers.size() +
                        ShaderStage.Samplers.size() + ShaderStage.SRVs.size();
    // TODO: + UAV count

    if (BindingCount) {
      CommandChunk *chk = cmd_queue.CurrentChunk();
      auto bindings = chk->reserve_vector<BINDING_INFO>(BindingCount);

      for (auto &[slot, cbuf] : ShaderStage.ConstantBuffers) {
        MTL_BIND_RESOURCE m;
        cbuf.Buffer->GetBoundResource(&m);
        bindings.push_back(BINDING_INFO{.ArgumentIndex = slot,
                                        .Type = BINDING_TYPE::Buffer,
                                        .U = {.Buffer = {m.Buffer, 0}}});
      }
      // TODO: sampler

      chk->emit([bindings = std::move(bindings)](CommandChunk::context &ctx) {
        // FIXME: compute shader?
        auto encoder = stage == ShaderType::Vertex ? ctx.vs_binding_encoder
                                                   : ctx.ps_binding_encoder;
        auto sstage = stage == ShaderType::Vertex ? MTL::RenderStageVertex
                                                  : MTL::RenderStageFragment;
        if (!encoder)
          return;

        auto [heap, offset] = ctx.chk->allocate_gpu_heap(
            encoder->encodedLength(), encoder->alignment());
        encoder->setArgumentBuffer(heap, offset);

        for (auto &binding : bindings) {
          switch (binding.Type) {
          case BINDING_TYPE::Buffer:
            encoder->setBuffer(binding.U.Buffer.Ptr, binding.U.Buffer.Offset,
                               binding.ArgumentIndex);
            ctx.render_encoder->useResource(binding.U.Buffer.Ptr,
                                            MTL::ResourceUsageRead, sstage);
            break;
          case BINDING_TYPE::Texture:
            encoder->setTexture(binding.U.Texture.Ptr, binding.ArgumentIndex);
            ctx.render_encoder->useResource(binding.U.Texture.Ptr,
                                            MTL::ResourceUsageRead, sstage);
            break;
          }
        }

        if constexpr (stage == ShaderType::Vertex) {
          ctx.render_encoder->setVertexBuffer(heap, offset,
                                              30 /* kArgumentBufferBinding */);
        } else {
          ctx.render_encoder->setFragmentBuffer(
              heap, offset, 30 /* kArgumentBufferBinding */);
        }
      });
      // TRACE("should encode element: ", BindingCount);
    }
    if (stage == ShaderType::Compute) {
      /* naive implementation... */
    } else {
    }
    binding_ready.set(stage);
  }

  template <bool Force = false, typename Fn> void EmitRenderCommand(Fn &&fn) {
    if (Force) {
      SwitchToRenderEncoder();
    }
    if (cmdbuf_state == CommandBufferState::RenderEncoderActive ||
        cmdbuf_state == CommandBufferState::RenderPipelineReady) {
      CommandChunk *chk = cmd_queue.CurrentChunk();
      chk->emit([fn = std::forward<Fn>(fn)](CommandChunk::context &ctx) {
        fn(ctx.render_encoder.ptr());
      });
    }
  };

  template <bool Force = false, typename Fn> void EmitComputeCommand(Fn &&fn) {
    if (Force) {
      SwitchToComputeEncoder();
    }
    if (cmdbuf_state == CommandBufferState::ComputeEncoderActive ||
        cmdbuf_state == CommandBufferState::ComputePipelineReady) {
      // insert
    }
  };

  template <bool Force = false, typename Fn> void EmitBlitCommand(Fn &&fn) {
    if (Force) {
      SwitchToBlitEncoder();
    }
    if (cmdbuf_state == CommandBufferState::BlitEncoderActive) {
      // insert
    }
  };

  template <bool Force = false> void EmitSetDepthStencilState() {
    auto state = state_.OutputMerger.DepthStencilState
                     ? state_.OutputMerger.DepthStencilState
                     : default_depth_stencil_state;
    EmitRenderCommand<Force>(
        [state, stencil_ref = state_.OutputMerger.StencilRef](
            MTL::RenderCommandEncoder *encoder) {
          encoder->setDepthStencilState(state->GetDepthStencilState());
          encoder->setStencilReferenceValue(stencil_ref);
        });
  }

  template <bool Force = false> void EmitSetRasterizerState() {
    auto state = state_.Rasterizer.RasterizerState
                     ? state_.Rasterizer.RasterizerState
                     : default_rasterizer_state;
    EmitRenderCommand<Force>([state](MTL::RenderCommandEncoder *encoder) {
      state->SetupRasterizerState(encoder);
    });
  }

  template <bool Force = false> void EmitSetScissor() {
    CommandChunk *chk = cmd_queue.CurrentChunk();
    auto scissors = chk->reserve_vector<MTL::ScissorRect>(
        state_.Rasterizer.NumScissorRects);
    for (unsigned i = 0; i < state_.Rasterizer.NumScissorRects; i++) {
      auto &d3dRect = state_.Rasterizer.scissor_rects[i];
      scissors.push_back({(UINT)d3dRect.left, (UINT)d3dRect.top,
                          (UINT)d3dRect.right - d3dRect.left,
                          (UINT)d3dRect.bottom - d3dRect.top});
    }
    EmitRenderCommand<Force>(
        [scissors = std::move(scissors)](MTL::RenderCommandEncoder *encoder) {
          if (scissors.size() == 0)
            return;
          encoder->setScissorRects(scissors.data(), scissors.size());
        });
  }

  template <bool Force = false> void EmitSetViewport() {

    CommandChunk *chk = cmd_queue.CurrentChunk();
    auto viewports =
        chk->reserve_vector<MTL::Viewport>(state_.Rasterizer.NumViewports);
    for (unsigned i = 0; i < state_.Rasterizer.NumViewports; i++) {
      auto &d3dViewport = state_.Rasterizer.viewports[i];
      viewports.push_back({d3dViewport.TopLeftX, d3dViewport.TopLeftY,
                           d3dViewport.Width, d3dViewport.Height,
                           d3dViewport.MinDepth, d3dViewport.MaxDepth});
    }
    EmitRenderCommand<Force>(
        [viewports = std::move(viewports)](MTL::RenderCommandEncoder *encoder) {
          encoder->setViewports(viewports.data(), viewports.size());
        });
  }

  /**
  it's just about vertex buffer
  - index buffer and input topology are provided in draw commands
  */
  template <bool Force = false> void EmitSetIAState() {
    CommandChunk *chk = cmd_queue.CurrentChunk();
    struct VERTEX_BUFFER_STATE {
      Obj<MTL::Buffer> buffer;
      UINT offset;
      UINT stride;
    };
    auto vbs = chk->reserve_vector<std::pair<UINT, VERTEX_BUFFER_STATE>>(
        state_.InputAssembler.VertexBuffers.size());
    for (auto &[index, state] : state_.InputAssembler.VertexBuffers) {
      MTL_BIND_RESOURCE r;
      assert(!r.IsTexture);
      state.Buffer->GetBoundResource(&r);
      vbs.push_back({index, {r.Buffer, state.Offset, state.Stride}});
    };
    EmitRenderCommand<Force>(
        [vbs = std::move(vbs)](MTL::RenderCommandEncoder *encoder) {
          for (auto &[index, state] : vbs) {
            encoder->setVertexBuffer(state.buffer.ptr(), state.offset,
                                     state.stride, index);
          };
          // TODO: deal with old redunant binding (I guess it doesn't hurt
        });
  }

  template <bool Force = false> void EmitBlendFactorAndStencilRef() {
    EmitRenderCommand<Force>([r = state_.OutputMerger.BlendFactor[0],
                              g = state_.OutputMerger.BlendFactor[1],
                              b = state_.OutputMerger.BlendFactor[2],
                              a = state_.OutputMerger.BlendFactor[3],
                              stencil_ref = state_.OutputMerger.StencilRef](
                                 MTL::RenderCommandEncoder *encoder) {
      encoder->setBlendColor(r, g, b, a);
      encoder->setStencilReferenceValue(stencil_ref);
    });
  }

  /**
  Valid transition:
  * -> Idle
  Idle -> RenderEncoderActive
  Idle -> ComputeEncoderActive
  Idle -> BlitEncoderActive
  RenderEncoderActive <-> RenderPipelineReady
  ComputeEncoderActive <-> CoputePipelineReady
  */
  enum class CommandBufferState {
    Idle,
    RenderEncoderActive,
    RenderPipelineReady,
    ComputeEncoderActive,
    ComputePipelineReady,
    BlitEncoderActive
  };

#pragma endregion

#pragma region Default State

private:
  /**
  Don't bind them to state or provide to API consumer
  (use COM just in case it's referenced by encoding thread)
  ...tricky
  */
  Com<IMTLD3D11RasterizerState> default_rasterizer_state;
  Com<IMTLD3D11DepthStencilState> default_depth_stencil_state;
  Com<IMTLD3D11BlendState> default_blend_state;

#pragma endregion

private:
  Obj<MTL::Device> metal_device_;
  D3D11ContextState state_;

  CommandBufferState cmdbuf_state;
  Flags<ShaderType> binding_ready;
  CommandQueue cmd_queue;

public:
  MTLD3D11DeviceContext(IMTLD3D11Device *pDevice)
      : MTLD3D11DeviceContextBase(pDevice),
        metal_device_(m_parent->GetMTLDevice()), state_(),
        cmd_queue(metal_device_) {
    default_rasterizer_state = CreateDefaultRasterizerState(pDevice);
    default_depth_stencil_state = CreateDefaultDepthStencilState(pDevice);
    default_blend_state = CreateDefaultBlendState(pDevice);
  }

  ~MTLD3D11DeviceContext() {}

  virtual void WaitUntilGPUIdle() { cmd_queue.WaitCPUFence(0); };
};

Com<IMTLD3D11DeviceContext> CreateD3D11DeviceContext(IMTLD3D11Device *pDevice) {
  return new MTLD3D11DeviceContext(pDevice);
}

} // namespace dxmt