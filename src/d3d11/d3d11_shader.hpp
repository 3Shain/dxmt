#pragma once

#include "airconv_public.h"
#include "com/com_guid.hpp"
#include "d3d11_device.hpp"
#include "objc-wrapper/dispatch.h"
#include "sha1/sha1_util.hpp"

struct MTL_COMPILED_SHADER {
  /**
  NOTE: it's not retained by design
  */
  MTL::Function *Function;
  dxmt::Sha1Hash *MetallibHash;
  MTL_SHADER_REFLECTION *Reflection;
};

DEFINE_COM_INTERFACE("a8bfeef7-a453-4bce-90c1-912b02cf5cdf", IMTLCompiledShader)
    : public IMTLThreadpoolWork {
  virtual void SubmitWork() = 0;
  virtual bool IsReady() = 0;
  /**
  NOTE: the current thread is blocked if it's not ready
   */
  virtual void GetShader(MTL_COMPILED_SHADER * pShaderData) = 0;

  virtual void Dump() = 0;

  virtual uint64_t GetId() = 0;
};

DEFINE_COM_INTERFACE("e95ba1c7-e43f-49c3-a907-4ac669c9fb42", IMTLD3D11Shader)
    : public IUnknown {
  virtual void GetCompiledShader(IMTLCompiledShader * *ppShader) = 0;
  virtual void GetCompiledShaderWithInputLayerFixup(
      uint64_t sign_mask, IMTLCompiledShader * *ppShader) = 0;
  virtual void GetCompiledPixelShaderWithSampleMask(
      uint32_t sample_mask, IMTLCompiledShader * *ppShader) = 0;
  virtual void GetReflection(MTL_SHADER_REFLECTION * *pRefl) = 0;
};

namespace dxmt {

enum class ShaderType {
  Vertex = 0,
  Pixel = 1,
  Geometry = 2,
  Hull = 3,
  Domain = 4,
  Compute = 5
};

HRESULT CreateVertexShader(IMTLD3D11Device *pDevice,
                           const void *pShaderBytecode, SIZE_T BytecodeLength,
                           ID3D11VertexShader **ppShader);

HRESULT CreatePixelShader(IMTLD3D11Device *pDevice, const void *pShaderBytecode,
                          SIZE_T BytecodeLength, ID3D11PixelShader **ppShader);

HRESULT CreateDummyHullShader(IMTLD3D11Device *pDevice,
                              const void *pShaderBytecode,
                              SIZE_T BytecodeLength,
                              ID3D11HullShader **ppShader);

HRESULT CreateDummyDomainShader(IMTLD3D11Device *pDevice,
                                const void *pShaderBytecode,
                                SIZE_T BytecodeLength,
                                ID3D11DomainShader **ppShader);

HRESULT CreateDummyGeometryShader(IMTLD3D11Device *pDevice,
                                  const void *pShaderBytecode,
                                  SIZE_T BytecodeLength,
                                  ID3D11GeometryShader **ppShader);

HRESULT CreateEmulatedVertexStreamOutputShader(
    IMTLD3D11Device *pDevice, const void *pShaderBytecode,
    SIZE_T BytecodeLength, ID3D11GeometryShader **ppShader, UINT NumEntries,
    const D3D11_SO_DECLARATION_ENTRY *pEntries, UINT NumStrides,
    const UINT *pStrides);

HRESULT CreateComputeShader(IMTLD3D11Device *pDevice,
                            const void *pShaderBytecode, SIZE_T BytecodeLength,
                            ID3D11ComputeShader **ppShader);

} // namespace dxmt