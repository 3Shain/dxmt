

#include "Foundation/NSError.hpp"
#include "Foundation/NSString.hpp"
#include "Foundation/NSURL.hpp"
#include "Metal/MTLCaptureManager.hpp"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLStageInputOutputDescriptor.hpp"
#include "d3d11_buffer.hpp"
#include "d3d11_device_child.h"
#include "d3d11_context_state.hpp"

#include "Metal/MTLCommandBuffer.hpp"
#include "Metal/MTLCommandQueue.hpp"

#include "../dxmt/dxmt_command_stream.hpp"

#include "d3d11_state_object.hpp"
#include "d3d11_view.hpp"
#include "d3d11_context.hpp"
#include "d3d11_private.h"
#include "d3d11_device.hpp"
#include "d3d11_query.h"
#include "d3d11_shader.h"

#include "Metal/MTLLibrary.hpp"
#include "mtld11_resource.hpp"

// #include "objc-wrapper/block.hpp"
#include <vector>
#include "../dxmt/dxmt_pipeline.hpp"

#include "../airconv/airconv_public.h"

namespace dxmt {

class MTLD3D11DeviceContext
    : public MTLD3D11DeviceChild<IMTLD3D11DeviceContext> {
public:
  MTLD3D11DeviceContext(IMTLD3D11Device *device)
      : MTLD3D11DeviceChild<IMTLD3D11DeviceContext>(device),
        metal_device_(m_parent->GetMTLDevice()), state_() {
    m_queue = metal_device_->newCommandQueue();

    ring_[0] = transfer(
        metal_device_->newBuffer(4096, MTL::ResourceStorageModeShared));
    ring_[1] = transfer(
        metal_device_->newBuffer(4096, MTL::ResourceStorageModeShared));
    ring_[2] = transfer(
        metal_device_->newBuffer(4096, MTL::ResourceStorageModeShared));

    stream_ = new DXMTCommandStream(ring_[2].ptr());
  }

  ~MTLD3D11DeviceContext() { m_queue->release(); }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) {
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

    if (logQueryInterfaceError(__uuidof(ID3D11DeviceContext), riid)) {
      Logger::warn(
          "D3D11DeviceContext::QueryInterface: Unknown interface query");
      Logger::warn(str::format(riid));
    }
    return E_NOINTERFACE;
  }

  void STDMETHODCALLTYPE Begin(ID3D11Asynchronous *pAsync) {
    // in theory pAsync could be any of them: { Query, Predicate, Counter }.
    // However `Predicate` and `Counter` are not supported at all
    static_cast<MTLD3D11Query *>(pAsync)->Begin();
  }

  // See Begin()
  void STDMETHODCALLTYPE End(ID3D11Asynchronous *pAsync) {
    static_cast<MTLD3D11Query *>(pAsync)->End();
  }

  HRESULT STDMETHODCALLTYPE GetData(ID3D11Asynchronous *pAsync, void *pData,
                                    UINT DataSize, UINT GetDataFlags) {
    if (!pAsync || (DataSize && !pData))
      return E_INVALIDARG;

    // Allow dataSize to be zero
    if (DataSize && DataSize != pAsync->GetDataSize())
      return E_INVALIDARG;
    pData = DataSize ? pData : nullptr;

    auto query = static_cast<MTLD3D11Query *>(pAsync);
    return query->GetData(pData, GetDataFlags);
  }

  HRESULT STDMETHODCALLTYPE Map(ID3D11Resource *pResource, UINT Subresource,
                                D3D11_MAP MapType, UINT MapFlags,
                                D3D11_MAPPED_SUBRESOURCE *pMappedResource) {
    auto resource = Com<IDXMTResource>::queryFrom(pResource);
    if (resource == nullptr) {
      return E_INVALIDARG;
    }
    if (MapType == D3D11_MAP_WRITE_DISCARD) {
      return resource->MapDiscard(
          Subresource, (MappedResource *)pMappedResource, stream_.ptr());
    }
    IMPLEMENT_ME;
  }

  void STDMETHODCALLTYPE Unmap(ID3D11Resource *pResource, UINT Subresource) {
    auto resource = Com<IDXMTResource>::queryFrom(pResource);
    if (resource == nullptr) {
      return;
    }
    resource->Unmap(Subresource);
  }

  void STDMETHODCALLTYPE Flush() {
    Flush2([](auto) {});
    pipelineUpToDate = false;
  }

  void STDMETHODCALLTYPE ExecuteCommandList(ID3D11CommandList *pCommandList,
                                            BOOL RestoreContextState){
      IMPLEMENT_ME}

  HRESULT STDMETHODCALLTYPE
      FinishCommandList(BOOL RestoreDeferredContextState,
                        ID3D11CommandList **ppCommandList) {
    return DXGI_ERROR_INVALID_CALL;
  }

  void STDMETHODCALLTYPE SetResourceMinLOD(ID3D11Resource *pResource,
                                           FLOAT MinLOD) {
    // FIXME: `min_lod_clamp` can do this but it's in the shader
    ERR_ONCE("Not implemented");
  }

  FLOAT STDMETHODCALLTYPE GetResourceMinLOD(ID3D11Resource *pResource) {
    ERR_ONCE("Not implemented");
    return 0.0f;
  }

#pragma region Resource Manipulation

  void STDMETHODCALLTYPE ClearRenderTargetView(
      ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4]) {
    if (auto rtv = Com<IMTLD3D11RenderTargetView>::queryFrom(pRenderTargetView);
        rtv != nullptr) {
      stream_->Emit(MTLCommandClearRenderTargetView(rtv->Pin(), ColorRGBA));
    }
  }

  void STDMETHODCALLTYPE ClearUnorderedAccessViewUint(
      ID3D11UnorderedAccessView *pUnorderedAccessView, const UINT Values[4]) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE ClearUnorderedAccessViewFloat(
      ID3D11UnorderedAccessView *pUnorderedAccessView, const FLOAT Values[4]) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE
  ClearDepthStencilView(ID3D11DepthStencilView *pDepthStencilView,
                        UINT ClearFlags, FLOAT Depth, UINT8 Stencil) {
    if (auto dsv = Com<IMTLD3D11DepthStencilView>::queryFrom(pDepthStencilView);
        dsv != nullptr) {
      stream_->Emit(MTLCommandClearDepthStencilView(
          dsv->Pin(stream_.ptr()), Depth, Stencil,
          ClearFlags & D3D11_CLEAR_DEPTH, ClearFlags & D3D11_CLEAR_STENCIL));
    };
  }

  void STDMETHODCALLTYPE ClearView(ID3D11View *pView, const FLOAT Color[4],
                                   const D3D11_RECT *pRect, UINT NumRects) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE
  GenerateMips(ID3D11ShaderResourceView *pShaderResourceView) {
    D3D11_SHADER_RESOURCE_VIEW_DESC desc;
    pShaderResourceView->GetDesc(&desc);
    if (desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFER ||
        desc.ViewDimension == D3D11_SRV_DIMENSION_BUFFEREX) {
      return;
    }
    if (auto c =
            Com<IMTLD3D11ShaderResourceView>::queryFrom(pShaderResourceView);
        c != nullptr) {

      stream_->Emit(MTLBlitCommand([](MTL::BlitCommandEncoder *encoder) {
        encoder->generateMipmaps(nullptr);
      }));
    }
  }

  void STDMETHODCALLTYPE ResolveSubresource(ID3D11Resource *pDstResource,
                                            UINT DstSubresource,
                                            ID3D11Resource *pSrcResource,
                                            UINT SrcSubresource,
                                            DXGI_FORMAT Format) {
    // Metal does not provide methods for explicit resolve action.
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE CopyResource(ID3D11Resource *pDstResource,
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

  void STDMETHODCALLTYPE
  CopyStructureCount(ID3D11Buffer *pDstBuffer, UINT DstAlignedByteOffset,
                     ID3D11UnorderedAccessView *pSrcView) {

    // allows the hidden count in a Count/Append UAV to be copied to another
    // Buffer.
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE CopySubresourceRegion(ID3D11Resource *pDstResource,
                                               UINT DstSubresource, UINT DstX,
                                               UINT DstY, UINT DstZ,
                                               ID3D11Resource *pSrcResource,
                                               UINT SrcSubresource,
                                               const D3D11_BOX *pSrcBox) {
    CopySubresourceRegion1(pDstResource, DstSubresource, DstX, DstY, DstZ,
                           pSrcResource, SrcSubresource, pSrcBox, 0);
  }

  void STDMETHODCALLTYPE CopySubresourceRegion1(
      ID3D11Resource *pDstResource, UINT DstSubresource, UINT DstX, UINT DstY,
      UINT DstZ, ID3D11Resource *pSrcResource, UINT SrcSubresource,
      const D3D11_BOX *pSrcBox, UINT CopyFlags) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE UpdateSubresource(ID3D11Resource *pDstResource,
                                           UINT DstSubresource,
                                           const D3D11_BOX *pDstBox,
                                           const void *pSrcData,
                                           UINT SrcRowPitch,
                                           UINT SrcDepthPitch) {
    UpdateSubresource1(pDstResource, DstSubresource, pDstBox, pSrcData,
                       SrcRowPitch, SrcDepthPitch, 0);
  }

  void STDMETHODCALLTYPE
  UpdateSubresource1(ID3D11Resource *pDstResource, UINT DstSubresource,
                     const D3D11_BOX *pDstBox, const void *pSrcData,
                     UINT SrcRowPitch, UINT SrcDepthPitch, UINT CopyFlags) {

    if (pDstBox != NULL) {
      if (pDstBox->right <= pDstBox->left || pDstBox->bottom <= pDstBox->top ||
          pDstBox->back <= pDstBox->front) {
        return;
      }
    }

    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE DiscardResource(ID3D11Resource *pResource) {
    /*
    All the Discard* API is not implemented and that's probably fine (as it's
    more like a hint of optimization, and Metal manages resources on its own)
    FIXME: for render targets we can use this information: LoadActionDontCare
    FIXME: A Map with D3D11_MAP_WRITE type could become D3D11_MAP_WRITE_DISCARD?
    */
    ERR_ONCE("Not implemented");
  }

  void STDMETHODCALLTYPE DiscardView(ID3D11View *pResourceView) {
    DiscardView1(pResourceView, 0, 0);
  }

  void STDMETHODCALLTYPE DiscardView1(ID3D11View *pResourceView,
                                      const D3D11_RECT *pRects, UINT NumRects) {
    ERR_ONCE("Not implemented");
  }
#pragma endregion

#pragma region DrawCall

  void CheckGraphicsPipeline() {
    // if (state_.hull_stage.shader != nullptr &&
    //     state_.domain_stage.shader != nullptr) {
    //   // tessellation pipeline
    // } else {
    //   // classic pipeline

    //   // calculate pipeline has

    //   // search for prebuilt pipeline
    // }

    // TMP

    if (!pipelineUpToDate) {
      RenderPipelineCacheEntry key{
          .VertexShaderRef = state_.vertex_stage.shader.ptr(),
          .PixelShaderRef = state_.pixel_stage.shader.ptr()};
      // look up history
      Rc<RenderPipelineCache> cached = findCache(key);
      if (cached != nullptr) {
        stream_->Emit(MTLBindPipelineState(
            [PSO = cached->PSO]() { return PSO.ptr(); },
            cached->vertexFunctionArgumentEncoder.ptr(),
            cached->fragmentFunctionArgumentEncoder.ptr()));
      } else {
        auto pool = transfer(NS::AutoreleasePool::alloc()->init());
        WARN("cache miss");
        Obj<NS::Error> err;
        Obj<MTL::Function> vf;
        Obj<MTL::Function> ff;
        Obj<MTL::RenderPipelineDescriptor> ps;
        {
          auto options = transfer(MTL::CompileOptions::alloc()->init());
          {
            uint32_t size;
            void *ptr;
            ConvertDXBC(state_.vertex_stage.shader->m_bytecode,
                        state_.vertex_stage.shader->m_bytecodeLength, &ptr,
                        &size);
            assert(ptr);
            auto dp = dispatch_data_create(ptr, size, nullptr, nullptr);
            Obj<MTL::Library> vlib =
                transfer(metal_device_->newLibrary(dp, &err));
            assert(dp);

            dispatch_release(dp);
            free(ptr);

            vf = transfer(vlib->newFunction(
                NS::String::string("shader_main", NS::UTF8StringEncoding)));
          }

          {
            uint32_t size;
            void *ptr;
            ConvertDXBC(state_.pixel_stage.shader->m_bytecode,
                        state_.pixel_stage.shader->m_bytecodeLength, &ptr,
                        &size);
            assert(ptr);
            auto dp = dispatch_data_create(ptr, size, nullptr, nullptr);
            Obj<MTL::Library> vlib =
                transfer(metal_device_->newLibrary(dp, &err));
            assert(dp);

            dispatch_release(dp);
            free(ptr);

            ff = transfer(vlib->newFunction(
                NS::String::string("shader_main", NS::UTF8StringEncoding)));
          }

          ps = transfer(MTL::RenderPipelineDescriptor::alloc()->init());

          ps->setVertexFunction(vf.ptr());
          ps->setFragmentFunction(ff.ptr());
          ps->colorAttachments()->object(0)->setPixelFormat(
              state_.output_merger.render_target_views[0]->GetPixelFormat());

          if (state_.output_merger.depth_stencil_view != nullptr) {
            ps->setDepthAttachmentPixelFormat(
                state_.output_merger.depth_stencil_view->GetPixelFormat());
            // ps->setStencilAttachmentPixelFormat(
            //     state_.output_merger.depth_stencil_view->GetPixelFormat());
            // FIXME: pass validator
          }

          std::array<uint32_t, 16> strides;
          memcpy(&strides[0], state_.input_assembler.strides, 4 * 16);
          state_.input_assembler.input_layout->Bind(ps.ptr(), strides);
        }
        auto state =
            transfer(metal_device_->newRenderPipelineState(ps.ptr(), &err));

        if (state == nullptr) {
          ERR(err->localizedDescription()->utf8String());
          throw MTLD3DError("Failed to create PSO");
        }

        auto ve = transfer(vf->newArgumentEncoder(30));
        // auto fe = transfer(ff->newArgumentEncoder(30)); // FIXME:

        stream_->Emit(MTLBindPipelineState(
            [state = state]() { return state.ptr(); }, ve.ptr(), nullptr));

        setCache(key, new RenderPipelineCache(state.ptr(), ve.ptr(), nullptr));
      }
      pipelineUpToDate = true;
    }
  }

  void STDMETHODCALLTYPE Draw(UINT VertexCount, UINT StartVertexLocation) {
    CheckGraphicsPipeline();
    // FIXME: _ADJ _PATCHLIST not handled
    auto primitive = g_topology_map[state_.input_assembler.topology];
    stream_->Emit(MTLRenderCommand([=](MTL::RenderCommandEncoder *encoder) {
      encoder->drawPrimitives(primitive, StartVertexLocation, VertexCount);
    }));
  }

  void STDMETHODCALLTYPE DrawIndexed(UINT IndexCount, UINT StartIndexLocation,
                                     INT BaseVertexLocation) {
    CheckGraphicsPipeline();
    auto buffer = Com<IDXMTBufferResource>::queryFrom(
        state_.input_assembler.index_buffer.ptr());
    auto primitive = g_topology_map[state_.input_assembler.topology];
    auto type =
        (state_.input_assembler.index_buffer_format == DXGI_FORMAT_R32_UINT)
            ? MTL::IndexTypeUInt32
            : MTL::IndexTypeUInt16;
    Rc<IndexBufferBinding> pinned = buffer->BindAsIndexBuffer(stream_.ptr());
    auto offset = state_.input_assembler.index_buffer_offset;
    stream_->Emit(MTLRenderCommand([=](MTL::RenderCommandEncoder *encoder) {
      encoder->drawIndexedPrimitives(primitive, IndexCount, type, pinned->Get(),
                                     offset, 1, BaseVertexLocation, 0);
    }));
  }

  void STDMETHODCALLTYPE DrawInstanced(UINT VertexCountPerInstance,
                                       UINT InstanceCount,
                                       UINT StartVertexLocation,
                                       UINT StartInstanceLocation) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE DrawIndexedInstanced(UINT IndexCountPerInstance,
                                              UINT InstanceCount,
                                              UINT StartIndexLocation,
                                              INT BaseVertexLocation,
                                              UINT StartInstanceLocation) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE DrawIndexedInstancedIndirect(
      ID3D11Buffer *pBufferForArgs, UINT AlignedByteOffsetForArgs) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE DrawInstancedIndirect(ID3D11Buffer *pBufferForArgs,
                                               UINT AlignedByteOffsetForArgs) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE DrawAuto() { IMPLEMENT_ME }

  void STDMETHODCALLTYPE Dispatch(UINT ThreadGroupCountX,
                                  UINT ThreadGroupCountY,
                                  UINT ThreadGroupCountZ) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE DispatchIndirect(ID3D11Buffer *pBufferForArgs,
                                          UINT AlignedByteOffsetForArgs) {
    IMPLEMENT_ME
  }
#pragma endregion

#pragma region State API

  void STDMETHODCALLTYPE GetPredication(ID3D11Predicate **ppPredicate,
                                        BOOL *pPredicateValue) {

    if (ppPredicate) {
      *ppPredicate = state_.predicate.ref();
    }
    if (pPredicateValue) {
      *pPredicateValue = state_.predicate_value;
    }

    ERR_ONCE("Stub");
  }

  void STDMETHODCALLTYPE SetPredication(ID3D11Predicate *pPredicate,
                                        BOOL PredicateValue) {

    state_.predicate = pPredicate;
    state_.predicate_value = PredicateValue;

    ERR_ONCE("Stub");
  }

  //-----------------------------------------------------------------------------
  // State Machine
  //-----------------------------------------------------------------------------

  void STDMETHODCALLTYPE
  SwapDeviceContextState(ID3DDeviceContextState *pState,
                         ID3DDeviceContextState **ppPreviousState) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE ClearState() { state_ = {}; }

#pragma region InputAssembler

  void STDMETHODCALLTYPE IASetInputLayout(ID3D11InputLayout *pInputLayout) {
    state_.input_assembler.input_layout =
        static_cast<IMTLD3D11InputLayout *>(pInputLayout);
    pipelineUpToDate = false;
  }
  void STDMETHODCALLTYPE IAGetInputLayout(ID3D11InputLayout **ppInputLayout) {
    if (ppInputLayout) {
      *ppInputLayout = state_.input_assembler.input_layout.ref();
    }
  }

  void STDMETHODCALLTYPE IASetVertexBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppVertexBuffers,
      const UINT *pStrides, const UINT *pOffsets) {
    for (unsigned i = 0; i < NumBuffers; i++) {
      state_.input_assembler.vertex_buffers[i + StartSlot] = ppVertexBuffers[i];
      state_.input_assembler.strides[i + StartSlot] = pStrides[i];
      state_.input_assembler.offsets[i + StartSlot] = pOffsets[i];
      if (ppVertexBuffers[i] != NULL) {

        auto vbo = Com<IDXMTBufferResource>::queryFrom(ppVertexBuffers[i]);

        stream_->Emit(
            MTLSetVertexBuffer(i + StartSlot, pOffsets[i],
                               vbo->BindAsVertexBuffer(stream_.ptr())));
      } else {
        stream_->Emit(MTLSetVertexBuffer(i + StartSlot, 0, nullptr));
      }
    }
  }
  void STDMETHODCALLTYPE IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers,
                                            ID3D11Buffer **ppVertexBuffers,
                                            UINT *pStrides, UINT *pOffsets) {
    for (unsigned i = 0; i < NumBuffers; i++) {
      if (ppVertexBuffers != NULL)
        (ppVertexBuffers)[i] =
            state_.input_assembler.vertex_buffers[i + StartSlot].ptr();
      if (pStrides != NULL)
        pStrides[i] = state_.input_assembler.strides[i + StartSlot];
      if (pOffsets != NULL)
        pOffsets[i] = state_.input_assembler.offsets[i + StartSlot];
    }
  }

  void STDMETHODCALLTYPE IASetIndexBuffer(ID3D11Buffer *pIndexBuffer,
                                          DXGI_FORMAT Format, UINT Offset) {
    state_.input_assembler.index_buffer = pIndexBuffer;
    state_.input_assembler.index_buffer_format = Format;
    state_.input_assembler.index_buffer_offset = Offset;

    // if (pIndexBuffer[i] != NULL) {

    //     auto vbo = Com<IDXMTBufferResource>::queryFrom(pIndexBuffer);

    //     stream_->Emit(
    //         MTLSetVertexBuffer(Format, Offset,
    //                            vbo->BindAsVertexBuffer(stream_.ptr())));
    //   } else {
    //     stream_->Emit(MTLSetVertexBuffer(i + StartSlot, 0, nullptr));
    //   }
  }
  void STDMETHODCALLTYPE IAGetIndexBuffer(ID3D11Buffer **pIndexBuffer,
                                          DXGI_FORMAT *Format, UINT *Offset) {
    if (pIndexBuffer != NULL) {
      pIndexBuffer[0] = state_.input_assembler.index_buffer.ptr();
    }
    if (Format != NULL)
      *Format = state_.input_assembler.index_buffer_format;
    if (Offset != NULL)
      *Offset = state_.input_assembler.index_buffer_offset;
  }
  void STDMETHODCALLTYPE
  IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) {
    state_.input_assembler.topology = Topology;
  }
  void STDMETHODCALLTYPE
  IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY *pTopology) {
    if (pTopology) {
      *pTopology = state_.input_assembler.topology;
    }
  }
#pragma endregion

#pragma region VertexShader

  void STDMETHODCALLTYPE VSSetShader(
      ID3D11VertexShader *pVertexShader,
      ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) {

    if (NumClassInstances)
      ERR("Class instances not supported");

    if (state_.vertex_stage.shader !=
        static_cast<MTLD3D11VertexShader *>(pVertexShader)) {
      state_.vertex_stage.shader =
          static_cast<MTLD3D11VertexShader *>(pVertexShader);
      pipelineUpToDate = false;
    }
  }

  void STDMETHODCALLTYPE VSGetShader(ID3D11VertexShader **ppVertexShader,
                                     ID3D11ClassInstance **ppClassInstances,
                                     UINT *pNumClassInstances) {
    if (ppVertexShader) {
      *ppVertexShader = state_.vertex_stage.shader.ref();
    }
    if (pNumClassInstances) {
      *pNumClassInstances = 0;
    }
  }

  void STDMETHODCALLTYPE
  VSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    for (unsigned i = 0; i < NumViews; i++) {
      state_.vertex_stage.shader_resource_views[i + StartSlot] =
          static_cast<IMTLD3D11ShaderResourceView *>(ppShaderResourceViews[i]);
    }
  }

  void STDMETHODCALLTYPE
  VSGetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView **ppShaderResourceViews) {
    for (unsigned i = 0; i < NumViews; i++) {
      ppShaderResourceViews[i] =
          state_.vertex_stage.shader_resource_views[i + StartSlot].ref();
    }
  }

  void STDMETHODCALLTYPE VSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState *const *ppSamplers) {
    for (unsigned i = 0; i < NumSamplers; i++) {
      state_.vertex_stage.sampler_states[i + StartSlot] =
          static_cast<MTLD3D11SamplerState *>(ppSamplers[i]);
    }
  }

  void STDMETHODCALLTYPE VSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState **ppSamplers) {
    for (unsigned i = 0; i < NumSamplers; i++) {
      ppSamplers[i] = state_.vertex_stage.sampler_states[i + StartSlot].ref();
    }
  }

  void STDMETHODCALLTYPE VSSetConstantBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) {
    VSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void STDMETHODCALLTYPE VSGetConstantBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) {
    VSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL, NULL);
  }

  void STDMETHODCALLTYPE VSSetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers,
      const UINT *pFirstConstant, const UINT *pNumConstants) {
    if ((pFirstConstant == NULL) != (pNumConstants == NULL)) {
      return; // INVALID
    }
    for (unsigned i = 0; i < NumBuffers; i++) {
      state_.vertex_stage.constant_buffers[i + StartSlot] =
          static_cast<ID3D11Buffer *>(ppConstantBuffers[i]);

      auto vbo = Com<IDXMTBufferResource>::queryFrom(ppConstantBuffers[i]);

      stream_->Emit(MTLSetConstantBuffer<Vertex>(
          i + StartSlot, 0, 0, vbo->BindAsConstantBuffer(stream_.ptr())));
    }
  }

  void STDMETHODCALLTYPE VSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                               ID3D11Buffer **ppConstantBuffers,
                                               UINT *pFirstConstant,
                                               UINT *pNumConstants) {
    IMPLEMENT_ME
  }

#pragma endregion

#pragma region PixelShader

  void STDMETHODCALLTYPE PSSetShader(
      ID3D11PixelShader *pPixelShader,
      ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) {

    if (NumClassInstances)
      ERR("Class instances not supported");

    if (state_.pixel_stage.shader !=
        static_cast<MTLD3D11PixelShader *>(pPixelShader)) {
      state_.pixel_stage.shader =
          static_cast<MTLD3D11PixelShader *>(pPixelShader);
      pipelineUpToDate = false;
    }
  }

  void STDMETHODCALLTYPE PSGetShader(ID3D11PixelShader **ppPixelShader,
                                     ID3D11ClassInstance **ppClassInstances,
                                     UINT *pNumClassInstances) {
    if (ppPixelShader) {
      *ppPixelShader = state_.pixel_stage.shader.ref();
    }
    if (pNumClassInstances) {
      *pNumClassInstances = 0;
    }
  }

  void STDMETHODCALLTYPE
  PSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    for (unsigned i = 0; i < NumViews; i++) {
      if (ppShaderResourceViews[i] != nullptr) {
        state_.pixel_stage.shader_resource_views[i + StartSlot] =
            static_cast<IMTLD3D11ShaderResourceView *>(
                ppShaderResourceViews[i]);
        if (auto res = Com<IMTLD3D11ShaderResourceView>::queryFrom(
                ppShaderResourceViews[i]);
            res != nullptr) {

          stream_->Emit(MTLSetShaderResource<Pixel>(res->Pin(stream_.ptr()),
                                                    i + StartSlot));
          continue;
        }
      }
      stream_->Emit(MTLSetShaderResource<Pixel>(nullptr, i + StartSlot));
    }
  }

  void STDMETHODCALLTYPE
  PSGetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView **ppShaderResourceViews) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE PSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState *const *ppSamplers) {
    for (unsigned i = 0; i < NumSamplers; i++) {
      if (ppSamplers[i] != nullptr) {

        auto c = state_.pixel_stage.sampler_states[i + StartSlot] =
            static_cast<MTLD3D11SamplerState *>(ppSamplers[i]);
        stream_->Emit(
            MTLSetSampler<Pixel>(c->metal_sampler_state_.ptr(), StartSlot + i));
      } else {

        auto c = state_.pixel_stage.sampler_states[i + StartSlot] =
            static_cast<MTLD3D11SamplerState *>(ppSamplers[i]);
        stream_->Emit(MTLSetSampler<Pixel>(nullptr, StartSlot + i));
      }
    }
  }

  void STDMETHODCALLTYPE PSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState **ppSamplers) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE PSSetConstantBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) {
    PSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, 0, 0);
  }

  void STDMETHODCALLTYPE PSGetConstantBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) {
    PSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, 0, 0);
  }

  void STDMETHODCALLTYPE PSSetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers,
      const UINT *pFirstConstant, const UINT *pNumConstants) {
    if ((pFirstConstant == NULL) != (pNumConstants == NULL)) {
      return; // INVALID
    }
    for (unsigned i = 0; i < NumBuffers; i++) {
      if (ppConstantBuffers[i] != nullptr) {
        state_.vertex_stage.constant_buffers[i + StartSlot] =
            static_cast<ID3D11Buffer *>(ppConstantBuffers[i]);

        auto vbo = Com<IDXMTBufferResource>::queryFrom(ppConstantBuffers[i]);

        stream_->Emit(MTLSetConstantBuffer<Pixel>(
            i + StartSlot, 0, 0, vbo->BindAsConstantBuffer(stream_.ptr())));
      } else {

        stream_->Emit(
            MTLSetConstantBuffer<Pixel>(i + StartSlot, 0, 0, nullptr));
      }
    }
  }

  void STDMETHODCALLTYPE PSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                               ID3D11Buffer **ppConstantBuffers,
                                               UINT *pFirstConstant,
                                               UINT *pNumConstants) {
    IMPLEMENT_ME
  }

#pragma endregion

#pragma region GeometryShader

  void STDMETHODCALLTYPE GSSetShader(
      ID3D11GeometryShader *pShader,
      ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) {

    if (NumClassInstances)
      Logger::err("D3D11: Class instances not supported");

    // if (m_state.gs != shader) {
    //   m_state.gs = shader;

    //   BindShader<DxbcProgramType::GeometryShader>(GetCommonShader(shader));
    // }
  }

  void STDMETHODCALLTYPE GSGetShader(ID3D11GeometryShader **ppGeometryShader,
                                     ID3D11ClassInstance **ppClassInstances,
                                     UINT *pNumClassInstances) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE GSSetConstantBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) {
    GSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, 0,
                          0); // really zero?
  }

  void STDMETHODCALLTYPE GSSetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers,
      const UINT *pFirstConstant, const UINT *pNumConstants) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE GSGetConstantBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) {
    GSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, 0,
                          0); // TODO: really zero?
  }

  void STDMETHODCALLTYPE GSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                               ID3D11Buffer **ppConstantBuffers,
                                               UINT *pFirstConstant,
                                               UINT *pNumConstants) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE
  GSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE
  GSGetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView **ppShaderResourceViews) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE GSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState *const *ppSamplers) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE GSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState **ppSamplers) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE SOSetTargets(UINT NumBuffers,
                                      ID3D11Buffer *const *ppSOTargets,
                                      const UINT *pOffsets) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE SOGetTargets(UINT NumBuffers,
                                      ID3D11Buffer **ppSOTargets) {
    IMPLEMENT_ME
  }

#pragma endregion

#pragma region HullShader

  void STDMETHODCALLTYPE
  HSGetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView **ppShaderResourceViews) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE HSGetShader(ID3D11HullShader **ppHullShader,
                                     ID3D11ClassInstance **ppClassInstances,
                                     UINT *pNumClassInstances) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE HSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState **ppSamplers) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE HSGetConstantBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE
  HSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE HSSetShader(
      ID3D11HullShader *pHullShader,
      ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) {

    if (NumClassInstances)
      Logger::err("D3D11: Class instances not supported");

    // if (m_state.hs != shader) {
    //   m_state.hs = shader;

    //   BindShader<DxbcProgramType::HullShader>(GetCommonShader(shader));
    // }
  }

  void STDMETHODCALLTYPE HSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState *const *ppSamplers) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE HSSetConstantBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE HSSetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers,
      const UINT *pFirstConstant, const UINT *pNumConstants) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE HSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                               ID3D11Buffer **ppConstantBuffers,
                                               UINT *pFirstConstant,
                                               UINT *pNumConstants) {
    IMPLEMENT_ME
  }

#pragma endregion

#pragma region DomainShader

  void STDMETHODCALLTYPE
  DSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE
  DSGetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView **ppShaderResourceViews) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE DSSetShader(
      ID3D11DomainShader *pDomainShader,
      ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) {

    if (NumClassInstances)
      ERR("Class instances not supported");

    // if (m_state.ds != shader) {
    //   m_state.ds = shader;

    //   BindShader<DxbcProgramType::DomainShader>(GetCommonShader(shader));
    // }
  }

  void STDMETHODCALLTYPE DSGetShader(ID3D11DomainShader **ppDomainShader,
                                     ID3D11ClassInstance **ppClassInstances,
                                     UINT *pNumClassInstances) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE DSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState **ppSamplers) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE DSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState *const *ppSamplers) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE DSSetConstantBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) {
    return DSSetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers, NULL,
                                 NULL);
  }

  void STDMETHODCALLTYPE DSSetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers,
      const UINT *pFirstConstant, const UINT *pNumConstants) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE DSGetConstantBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) {
    UINT *pFirstConstant = 0, *pNumConstants = 0;
    return DSGetConstantBuffers1(StartSlot, NumBuffers, ppConstantBuffers,
                                 pFirstConstant, pNumConstants);
  }

  void STDMETHODCALLTYPE DSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                               ID3D11Buffer **ppConstantBuffers,
                                               UINT *pFirstConstant,
                                               UINT *pNumConstants) {
    IMPLEMENT_ME
  }

#pragma endregion

#pragma region ComputeShader

  void STDMETHODCALLTYPE
  CSGetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView **ppShaderResourceViews) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE CSGetUnorderedAccessViews(
      UINT StartSlot, UINT NumUAVs,
      ID3D11UnorderedAccessView **ppUnorderedAccessViews) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE CSGetShader(ID3D11ComputeShader **ppComputeShader,
                                     ID3D11ClassInstance **ppClassInstances,
                                     UINT *pNumClassInstances) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE CSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState **ppSamplers) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE CSGetConstantBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer **ppConstantBuffers) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE
  CSSetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView *const *ppShaderResourceViews) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE CSSetUnorderedAccessViews(
      UINT StartSlot, UINT NumUAVs,
      ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
      const UINT *pUAVInitialCounts) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE CSSetShader(
      ID3D11ComputeShader *pComputeShader,
      ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE CSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState *const *ppSamplers) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE CSSetConstantBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE CSSetConstantBuffers1(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppConstantBuffers,
      const UINT *pFirstConstant, const UINT *pNumConstants) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE CSGetConstantBuffers1(UINT StartSlot, UINT NumBuffers,
                                               ID3D11Buffer **ppConstantBuffers,
                                               UINT *pFirstConstant,
                                               UINT *pNumConstants) {
    IMPLEMENT_ME
  }

#pragma endregion

#pragma region OutputMerger

  void STDMETHODCALLTYPE OMSetRenderTargets(
      UINT NumViews, ID3D11RenderTargetView *const *ppRenderTargetViews,
      ID3D11DepthStencilView *pDepthStencilView) {
    OMSetRenderTargetsAndUnorderedAccessViews(
        NumViews, ppRenderTargetViews, pDepthStencilView, 0, 0, NULL, NULL);
  }

  void STDMETHODCALLTYPE OMGetRenderTargets(
      UINT NumViews, ID3D11RenderTargetView **ppRenderTargetViews,
      ID3D11DepthStencilView **ppDepthStencilView) {
    OMGetRenderTargetsAndUnorderedAccessViews(NumViews, ppRenderTargetViews,
                                              ppDepthStencilView, 0, 0, NULL);
  }

  void STDMETHODCALLTYPE OMSetRenderTargetsAndUnorderedAccessViews(
      UINT NumRTVs, ID3D11RenderTargetView *const *ppRenderTargetViews,
      ID3D11DepthStencilView *pDepthStencilView, UINT UAVStartSlot,
      UINT NumUAVs, ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
      const UINT *pUAVInitialCounts) {

    // TODO: check data hazards
    // TODO: handle UAV
    std::vector<Rc<RenderTargetBinding>> views(NumRTVs);
    for (unsigned i = 0; i < 8; i++) {
      if (i < NumRTVs) {
        state_.output_merger.render_target_views[i] =
            static_cast<IMTLD3D11RenderTargetView *>(ppRenderTargetViews[i]);
        views[i] = state_.output_merger.render_target_views[i]->Pin();
      } else {
        state_.output_merger.render_target_views[i] = nullptr;
      }
    }
    state_.output_merger.depth_stencil_view =
        static_cast<IMTLD3D11DepthStencilView *>(pDepthStencilView);
    Rc<DepthStencilBinding> dsv =
        pDepthStencilView
            ? state_.output_merger.depth_stencil_view->Pin(stream_.ptr())
            : nullptr;

    // Validation
    // 1. check if all rtvs and dsv have the same array size

    stream_->Emit(MTLBindRenderTarget([views = std::move(views),
                                       dsv = std::move(dsv)](
                                          MTL::RenderPassDescriptor *desc) {
      unsigned slot = 0;
      for (auto &view : views) {
        view->Bind(desc, slot++, MTL::LoadActionLoad, MTL::StoreActionStore);
      }
      if (dsv != nullptr) {
        dsv->Bind(desc);
      }
      // desc->setRenderTargetArrayLength(1); // TODO: if RTVs are created from
      // texture array
    }));
  }
  void STDMETHODCALLTYPE OMGetRenderTargetsAndUnorderedAccessViews(
      UINT NumRTVs, ID3D11RenderTargetView **ppRenderTargetViews,
      ID3D11DepthStencilView **ppDepthStencilView, UINT UAVStartSlot,
      UINT NumUAVs, ID3D11UnorderedAccessView **ppUnorderedAccessViews) {
    IMPLEMENT_ME;
  }

  void STDMETHODCALLTYPE OMSetBlendState(ID3D11BlendState *pBlendState,
                                         const FLOAT BlendFactor[4],
                                         UINT SampleMask) {
    IMPLEMENT_ME
  }
  void STDMETHODCALLTYPE OMGetBlendState(ID3D11BlendState **ppBlendState,
                                         FLOAT BlendFactor[4],
                                         UINT *pSampleMask) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE OMSetDepthStencilState(
      ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef) {
    state_.output_merger.depth_stencil_state =
        static_cast<MTLD3D11DepthStencilState *>(pDepthStencilState);
    state_.output_merger.stencil_ref = StencilRef;
    if (pDepthStencilState != NULL) {
      stream_->Emit(MTLBindDepthStencilState(
          [StencilRef = StencilRef,
           state = (state_.output_merger.depth_stencil_state)](
              MTL::RenderCommandEncoder *enc) {
            state->SetupDepthStencilState(enc);
            enc->setStencilReferenceValue(StencilRef);
          }));
    } else {
      stream_->Emit(MTLBindDepthStencilState());
    }
  }

  void STDMETHODCALLTYPE OMGetDepthStencilState(
      ID3D11DepthStencilState **ppDepthStencilState, UINT *pStencilRef) {}

#pragma endregion

#pragma region Rasterizer

  void STDMETHODCALLTYPE RSSetState(ID3D11RasterizerState *pRasterizerState) {

    state_.rasterizer.rasterizer_state =
        static_cast<MTLD3D11RasterizerState *>(pRasterizerState);
  }

  void STDMETHODCALLTYPE RSGetState(ID3D11RasterizerState **ppRasterizerState) {
    InitReturnPtr(ppRasterizerState);

    *ppRasterizerState = state_.rasterizer.rasterizer_state.ref();
  }

  void STDMETHODCALLTYPE RSSetViewports(UINT NumViewports,
                                        const D3D11_VIEWPORT *pViewports) {
    state_.rasterizer.numViewports = NumViewports;
    memcpy(state_.rasterizer.viewports, pViewports,
           sizeof(D3D11_VIEWPORT) * NumViewports);
    std::vector<MTL::Viewport> viewports(NumViewports);
    //
    for (unsigned i = 0; i < NumViewports; i++) {
      auto &viewport = pViewports[i];
      viewports[i].height = viewport.Height;
      viewports[i].width = viewport.Width;
      viewports[i].originX = viewport.TopLeftX;
      viewports[i].originY = viewport.TopLeftY;
      viewports[i].zfar = viewport.MaxDepth;
      viewports[i].znear = viewport.MinDepth;
    };
    stream_->Emit(MTLSetViewports(std::move(viewports)));
  }

  void STDMETHODCALLTYPE RSGetViewports(UINT *pNumViewports,
                                        D3D11_VIEWPORT *pViewports) {
    *pNumViewports = state_.rasterizer.numViewports;
    memcpy(pViewports, state_.rasterizer.viewports,
           sizeof(D3D11_VIEWPORT) * state_.rasterizer.numViewports);
  }

  void STDMETHODCALLTYPE RSSetScissorRects(UINT NumRects,
                                           const D3D11_RECT *pRects) {
    state_.rasterizer.numScissorRects = NumRects;
    memcpy(state_.rasterizer.scissor_rects, pRects,
           sizeof(D3D11_RECT) * NumRects);
    std::vector<MTL::ScissorRect> rects(NumRects);
    //
    for (unsigned i = 0; i < NumRects; i++) {
      auto &scissorRect = pRects[i];
      rects[i].x = scissorRect.left;
      rects[i].y = scissorRect.top;
      rects[i].width = state_.rasterizer.viewports[i].Width - scissorRect.left -
                       scissorRect.right;
      rects[i].height = state_.rasterizer.viewports[i].Height -
                        scissorRect.top - scissorRect.bottom;
    } // FIXME: undesired dependency: viewports. need to recalculate if viewport
      // changes
    stream_->Emit(MTLSetScissorRects(std::move(rects)));
  }

  void STDMETHODCALLTYPE RSGetScissorRects(UINT *pNumRects,
                                           D3D11_RECT *pRects) {
    *pNumRects = state_.rasterizer.numScissorRects;
    memcpy(pRects, state_.rasterizer.scissor_rects,
           sizeof(D3D11_RECT) * state_.rasterizer.numScissorRects);
  }
#pragma endregion

#pragma endregion

#pragma region Misc

  D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType() {
    return D3D11_DEVICE_CONTEXT_IMMEDIATE;
  }

  UINT STDMETHODCALLTYPE GetContextFlags() { return 0; }

#pragma endregion

  void STDMETHODCALLTYPE
  Flush2(std::function<void(MTL::CommandBuffer *)> &&beforeCommit) final {

    auto c = MTL::CaptureManager::sharedCaptureManager();
    auto d = transfer(MTL::CaptureDescriptor::alloc()->init());
    d->setCaptureObject(m_queue->device());
    d->setDestination(MTL::CaptureDestinationGPUTraceDocument);
    char filename[1024];
    std::time_t now;
    std::time(&now);
    std::strftime(filename, 1024,
                  "/Users/sanshain/capture-%H-%M-%S_%m-%d-%y.gputrace",
                  std::localtime(&now));

    auto _pTraceSaveFilePath =
        (NS::String::string(filename, NS::UTF8StringEncoding));
    NS::URL *pURL = NS::URL::alloc()->initFileURLWithPath(_pTraceSaveFilePath);

    NS::Error *pError = nullptr;
    d->setOutputURL(pURL);

    c->startCapture(d.ptr(), &pError);
    {
      auto pool = transfer(NS::AutoreleasePool::alloc()->init());
      auto cbuffer = m_queue->commandBufferWithUnretainedReferences();
      stream_->Encode(pipeline_command_state, cbuffer, metal_device_.ptr());

      beforeCommit(cbuffer);

      // stream_->incRef();
      // cbuffer->addCompletedHandler([stream_ = std::move(stream_)](auto s) {

      // });
      cbuffer->commit();
      cbuffer->waitUntilCompleted();
    }

    // throw 0;

    stream_ = new DXMTCommandStream(ring_[ring_offset].ptr());

    ring_offset++;
    ring_offset %= 3;

    c->stopCapture();
  }

private:
  Obj<MTL::Device> metal_device_;
  MTL::CommandQueue *m_queue;
  D3D11ContextState state_;

  Rc<DXMTCommandStream> stream_;

  PipelineCommandState pipeline_command_state;

  bool pipelineUpToDate = false;

  Obj<MTL::Buffer> ring_[3];
  int ring_offset = 0;
};

IMTLD3D11DeviceContext *NewD3D11DeviceContext(IMTLD3D11Device *device) {
  return new MTLD3D11DeviceContext(device);
}

} // namespace dxmt