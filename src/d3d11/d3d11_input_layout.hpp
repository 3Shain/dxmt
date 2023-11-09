#pragma once

#include "d3d11_device.hpp"

#include "Metal/MTLRenderPipeline.hpp"
#include "Metal/MTLComputePipeline.hpp"

MIDL_INTERFACE("b56c6a99-80cf-4c7f-a756-9e9ceb38730f")
IMTLD3D11InputLayout : public ID3D11InputLayout {
  virtual void STDMETHODCALLTYPE Bind(MTL::RenderPipelineDescriptor * desc,
                                      const std::array<UINT, 16> &strides) = 0;
  virtual void STDMETHODCALLTYPE Bind(MTL::ComputePipelineDescriptor * desc,
                                      const std::array<UINT, 16> &strides) = 0;
};
__CRT_UUID_DECL(IMTLD3D11InputLayout, 0xb56c6a99, 0x80cf, 0x4c7f, 0xa7, 0x56,
                0x9e, 0x9c, 0xeb, 0x38, 0x73, 0x0f);

namespace dxmt {

HRESULT CreateInputLayout(IMTLD3D11Device *device,
                          const void *pShaderBytecodeWithInputSignature,
                          const D3D11_INPUT_ELEMENT_DESC *input_element_descs,
                          UINT num_elements, ID3D11InputLayout **ppInputLayout);
} // namespace dxmt