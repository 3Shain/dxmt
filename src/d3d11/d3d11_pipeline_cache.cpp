#include "d3d11_pipeline_cache.hpp"
#include "d3d11_shader.hpp"
#include "d3d11_pipeline.hpp"

namespace dxmt {

class PipelineCache : public MTLD3D11PipelineCacheBase {

  IMTLD3D11Device *device;
  StateObjectCache<D3D11_BLEND_DESC1, IMTLD3D11BlendState> blend_states;

  StateObjectCache<MTL_INPUT_LAYOUT_DESC, IMTLD3D11InputLayout> input_layouts;

  std::unordered_map<MTL_GRAPHICS_PIPELINE_DESC,
                     Com<IMTLCompiledGraphicsPipeline>>
      pipelines_;
  dxmt::mutex mutex_;

  std::unordered_map<MTL_GRAPHICS_PIPELINE_DESC,
                     Com<IMTLCompiledTessellationPipeline>>
      pipelines_ts_;
  dxmt::mutex mutex_ts_;

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(IMTLD3D11PipeineCache)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  virtual HRESULT AddVertexShader(const void *pBytecode,
                                  uint32_t BytecodeLength,
                                  ID3D11VertexShader **ppShader) override {
    return CreateVertexShader(device, pBytecode, BytecodeLength, ppShader);
  }

  virtual HRESULT AddPixelShader(const void *pBytecode, uint32_t BytecodeLength,
                                 ID3D11PixelShader **ppShader) override {
    return CreatePixelShader(device, pBytecode, BytecodeLength, ppShader);
  }

  virtual HRESULT AddHullShader(const void *pBytecode, uint32_t BytecodeLength,
                                ID3D11HullShader **ppShader) override {
    return CreateHullShader(device, pBytecode, BytecodeLength, ppShader);
  }

  virtual HRESULT AddDomainShader(const void *pBytecode,
                                  uint32_t BytecodeLength,
                                  ID3D11DomainShader **ppShader) override {
    return CreateDomainShader(device, pBytecode, BytecodeLength, ppShader);
  }

  virtual HRESULT AddGeometryShader(const void *pBytecode,
                                    uint32_t BytecodeLength,
                                    ID3D11GeometryShader **ppShader) override {
    return CreateDummyGeometryShader(device, pBytecode, BytecodeLength,
                                     ppShader);
  }

  HRESULT AddInputLayout(const void *pShaderBytecodeWithInputSignature,
                         const D3D11_INPUT_ELEMENT_DESC *pInputElementDesc,
                         UINT NumElements,
                         IMTLD3D11InputLayout **ppInputLayout) override {
    std::vector<MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC> buffer(NumElements);
    uint32_t num_metal_ia_elements;
    if (FAILED(ExtractMTLInputLayoutElements(
            device, pShaderBytecodeWithInputSignature, pInputElementDesc,
            NumElements, buffer.data(), &num_metal_ia_elements))) {
      return E_FAIL;
    }
    buffer.resize(num_metal_ia_elements);
    return input_layouts.CreateStateObject(&buffer, ppInputLayout);
  }

  HRESULT AddBlendState(const D3D11_BLEND_DESC1 *pBlendDesc,
                        IMTLD3D11BlendState **ppBlendState) override {
    return blend_states.CreateStateObject(pBlendDesc, ppBlendState);
  }

  void GetGraphicsPipeline(MTL_GRAPHICS_PIPELINE_DESC *pDesc,
                           IMTLCompiledGraphicsPipeline **ppPipeline) override {
    std::lock_guard<dxmt::mutex> lock(mutex_);

    auto iter = pipelines_.find(*pDesc);
    if (iter != pipelines_.end()) {
      *ppPipeline = iter->second.ref();
      return;
    }
    auto temp = dxmt::CreateGraphicsPipeline(device, pDesc);
    D3D11_ASSERT(pipelines_.insert({*pDesc, temp}).second); // copy
    *ppPipeline = std::move(temp);                          // move
  }

  void GetTessellationPipeline(
      MTL_GRAPHICS_PIPELINE_DESC *pDesc,
      IMTLCompiledTessellationPipeline **ppPipeline) override {
    std::lock_guard<dxmt::mutex> lock(mutex_ts_);

    auto iter = pipelines_ts_.find(*pDesc);
    if (iter != pipelines_ts_.end()) {
      *ppPipeline = iter->second.ref();
      return;
    }
    auto temp = dxmt::CreateTessellationPipeline(device, pDesc);
    D3D11_ASSERT(pipelines_ts_.insert({*pDesc, temp}).second); // copy
    *ppPipeline = std::move(temp);
  }

public:
  PipelineCache(IMTLD3D11Device *pDevice)
      : MTLD3D11PipelineCacheBase(pDevice), device(pDevice),
        blend_states(pDevice), input_layouts(pDevice) {

        };
};

std::unique_ptr<MTLD3D11PipelineCacheBase>
InitializePipelineCache(IMTLD3D11Device *device) {
  return std::make_unique<PipelineCache>(device);
}

}; // namespace dxmt