#pragma once
#include "d3d11_input_layout.hpp"
#include "d3d11_state_object.hpp"

namespace dxmt {

class MTLD3D11PipelineCacheBase {
public:
  virtual ~MTLD3D11PipelineCacheBase() {}
  virtual HRESULT AddVertexShader(const void *pBytecode, uint32_t BytecodeLength, ID3D11VertexShader **ppShader) = 0;
  virtual HRESULT AddPixelShader(const void *pBytecode, uint32_t BytecodeLength, ID3D11PixelShader **ppShader) = 0;
  virtual HRESULT AddHullShader(const void *pBytecode, uint32_t BytecodeLength, ID3D11HullShader **ppShader) = 0;
  virtual HRESULT AddDomainShader(const void *pBytecode, uint32_t BytecodeLength, ID3D11DomainShader **ppShader) = 0;
  virtual HRESULT
  AddGeometryShader(const void *pBytecode, uint32_t BytecodeLength, ID3D11GeometryShader **ppShader) = 0;
  virtual HRESULT AddComputeShader(const void *pBytecode, uint32_t BytecodeLength, ID3D11ComputeShader **ppShader) = 0;
  virtual HRESULT AddInputLayout(
      const void *pShaderBytecodeWithInputSignature, const D3D11_INPUT_ELEMENT_DESC *pInputElementDesc,
      UINT NumElements, IMTLD3D11InputLayout **ppInputLayout
  ) = 0;
  virtual HRESULT AddStreamOutputLayout(
      const void *pShaderBytecode, UINT NumEntries, const D3D11_SO_DECLARATION_ENTRY *pEntries, UINT NumStrides,
      const UINT *pStrides, UINT RasterizedStream, IMTLD3D11StreamOutputLayout **ppSOLayout
  ) = 0;
  virtual HRESULT AddBlendState(const D3D11_BLEND_DESC1 *pBlendDesc, IMTLD3D11BlendState **ppBlendState) = 0;
  virtual void
  GetGraphicsPipeline(MTL_GRAPHICS_PIPELINE_DESC *pPipelineDesc, MTLCompiledGraphicsPipeline **ppPipeline) = 0;
  virtual void GetGeometryPipeline(MTL_GRAPHICS_PIPELINE_DESC *pDesc, MTLCompiledGeometryPipeline **ppPipeline) = 0;
  virtual void
  GetTessellationPipeline(MTL_GRAPHICS_PIPELINE_DESC *pDesc, MTLCompiledTessellationMeshPipeline **ppPipeline) = 0;
  virtual void GetComputePipeline(MTL_COMPUTE_PIPELINE_DESC *pDesc, MTLCompiledComputePipeline **ppPipeline) = 0;
};

std::unique_ptr<MTLD3D11PipelineCacheBase>
InitializePipelineCache(MTLD3D11Device *device);

} // namespace dxmt
