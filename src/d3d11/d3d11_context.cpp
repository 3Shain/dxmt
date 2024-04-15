#include "Metal/MTLCaptureManager.hpp"
#include "com/com_guid.hpp"
#include "d3d11_device_child.hpp"
#include "d3d11_context_state.hpp"

#include "Metal/MTLCommandBuffer.hpp"
#include "Metal/MTLCommandQueue.hpp"

#include "../dxmt/dxmt_command_stream.hpp"

#include "d3d11_query.hpp"
#include "d3d11_context.hpp"
#include "d3d11_device.hpp"
#include "d3d11_shader.hpp"

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

    IMPLEMENT_ME;
  }

  void STDMETHODCALLTYPE Unmap(ID3D11Resource *pResource, UINT Subresource) {

    IMPLEMENT_ME;
  }

  void STDMETHODCALLTYPE Flush() {
    Flush2([](auto) {});
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

    IMPLEMENT_ME;
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
                        UINT ClearFlags, FLOAT Depth, UINT8 Stencil) {}

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

  void STDMETHODCALLTYPE Draw(UINT VertexCount, UINT StartVertexLocation) {}

  void STDMETHODCALLTYPE DrawIndexed(UINT IndexCount, UINT StartIndexLocation,
                                     INT BaseVertexLocation) {}

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
  }
  void STDMETHODCALLTYPE IAGetInputLayout(ID3D11InputLayout **ppInputLayout) {}

  void STDMETHODCALLTYPE IASetVertexBuffers(
      UINT StartSlot, UINT NumBuffers, ID3D11Buffer *const *ppVertexBuffers,
      const UINT *pStrides, const UINT *pOffsets) {}
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

#pragma region ShaderCommon

template<int shader_type>
void SetConstantBuffer() {

}

template<int shader_type>
void SetShaderResource() {

}

template<int shader_type>
void SetSampler() {

}

#pragma endregion

#pragma region VertexShader

  void STDMETHODCALLTYPE VSSetShader(
      ID3D11VertexShader *pVertexShader,
      ID3D11ClassInstance *const *ppClassInstances, UINT NumClassInstances) {

    if (NumClassInstances)
      ERR("Class instances not supported");
  }

  void STDMETHODCALLTYPE VSGetShader(ID3D11VertexShader **ppVertexShader,
                                     ID3D11ClassInstance **ppClassInstances,
                                     UINT *pNumClassInstances) {}

  void STDMETHODCALLTYPE VSSetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView *const *ppShaderResourceViews) {}

  void STDMETHODCALLTYPE
  VSGetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView **ppShaderResourceViews) {}

  void STDMETHODCALLTYPE VSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState *const *ppSamplers) {}

  void STDMETHODCALLTYPE VSGetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState **ppSamplers) {}

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
      const UINT *pFirstConstant, const UINT *pNumConstants) {}

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
  }

  void STDMETHODCALLTYPE PSGetShader(ID3D11PixelShader **ppPixelShader,
                                     ID3D11ClassInstance **ppClassInstances,
                                     UINT *pNumClassInstances) {}

  void STDMETHODCALLTYPE PSSetShaderResources(
      UINT StartSlot, UINT NumViews,
      ID3D11ShaderResourceView *const *ppShaderResourceViews) {}

  void STDMETHODCALLTYPE
  PSGetShaderResources(UINT StartSlot, UINT NumViews,
                       ID3D11ShaderResourceView **ppShaderResourceViews) {
    IMPLEMENT_ME
  }

  void STDMETHODCALLTYPE PSSetSamplers(UINT StartSlot, UINT NumSamplers,
                                       ID3D11SamplerState *const *ppSamplers) {}

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
      const UINT *pFirstConstant, const UINT *pNumConstants) {}

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
      const UINT *pUAVInitialCounts) {}
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
      ID3D11DepthStencilState *pDepthStencilState, UINT StencilRef) {}

  void STDMETHODCALLTYPE OMGetDepthStencilState(
      ID3D11DepthStencilState **ppDepthStencilState, UINT *pStencilRef) {}

#pragma endregion

#pragma region Rasterizer

  void STDMETHODCALLTYPE RSSetState(ID3D11RasterizerState *pRasterizerState) {}

  void STDMETHODCALLTYPE RSGetState(ID3D11RasterizerState **ppRasterizerState) {
    InitReturnPtr(ppRasterizerState);
  }

  void STDMETHODCALLTYPE RSSetViewports(UINT NumViewports,
                                        const D3D11_VIEWPORT *pViewports) {}

  void STDMETHODCALLTYPE RSGetViewports(UINT *pNumViewports,
                                        D3D11_VIEWPORT *pViewports) {}

  void STDMETHODCALLTYPE RSSetScissorRects(UINT NumRects,
                                           const D3D11_RECT *pRects) {}

  void STDMETHODCALLTYPE RSGetScissorRects(UINT *pNumRects,
                                           D3D11_RECT *pRects) {}
#pragma endregion

#pragma region Misc

  D3D11_DEVICE_CONTEXT_TYPE STDMETHODCALLTYPE GetType() {
    return D3D11_DEVICE_CONTEXT_IMMEDIATE;
  }

  UINT STDMETHODCALLTYPE GetContextFlags() { return 0; }

#pragma endregion

  void STDMETHODCALLTYPE
  Flush2(std::function<void(MTL::CommandBuffer *)> &&beforeCommit) final {}

private:
  Obj<MTL::Device> metal_device_;
  MTL::CommandQueue *m_queue;
  D3D11ContextState state_;

  Rc<DXMTCommandStream> stream_;

  PipelineCommandState pipeline_command_state;

  Obj<MTL::Buffer> ring_[3];
  int ring_offset = 0;
};

IMTLD3D11DeviceContext *NewD3D11DeviceContext(IMTLD3D11Device *device) {
  return new MTLD3D11DeviceContext(device);
}

} // namespace dxmt