#pragma once

#include "com/com_guid.hpp"
#include "d3d11_device.hpp"

#include "Metal/MTLRenderPipeline.hpp"
#include "Metal/MTLComputePipeline.hpp"

struct MTL_SHADER_INPUT_LAYOUT_FIXUP {
  uint64_t sign_mask;
};

DEFINE_COM_INTERFACE("b56c6a99-80cf-4c7f-a756-9e9ceb38730f",
                     IMTLD3D11InputLayout)
    : public ID3D11InputLayout {
  virtual void STDMETHODCALLTYPE Bind(MTL::RenderPipelineDescriptor * desc) = 0;
  virtual void STDMETHODCALLTYPE Bind(MTL::ComputePipelineDescriptor * desc) = 0;
  virtual bool STDMETHODCALLTYPE NeedsFixup() = 0;
  virtual void STDMETHODCALLTYPE GetShaderFixupInfo(
      MTL_SHADER_INPUT_LAYOUT_FIXUP * pFixup) = 0;
  virtual uint32_t STDMETHODCALLTYPE GetInputSlotMask() = 0;
};

namespace dxmt {

HRESULT CreateInputLayout(IMTLD3D11Device *device,
                          const void *pShaderBytecodeWithInputSignature,
                          const D3D11_INPUT_ELEMENT_DESC *input_element_descs,
                          UINT num_elements, ID3D11InputLayout **ppInputLayout);
} // namespace dxmt