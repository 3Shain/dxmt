/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "Metal.hpp"
#include "com/com_pointer.hpp"
#include "d3d12_device.hpp"
#include "d3d12_pageable.hpp"
#include "log/log.hpp"
#include "airconv_public.h"

namespace dxmt {

class MTLD3D12ComputePipelineStateImpl : public MTLD3D12Pageable<MTLD3D12ComputePipelineState> {

  sm50_shader_t shader_cs;
  MTL_SHADER_REFLECTION ref_cs;

public:
  MTLD3D12ComputePipelineStateImpl(MTLD3D12Device *pDevice) : MTLD3D12Pageable<MTLD3D12ComputePipelineState>(pDevice) {
    IsComputePipelineState = 1;
  }

  HRESULT
  Initialize(const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc) {

    sm50_error_t sm50_err;

    SM50_SHADER_ROOT_SIGNATURE_DATA rootsig;
    rootsig.type = SM50_SHADER_ROOT_SIGNATURE;
    if (pDesc->pRootSignature) {
      rootsig.bytecode_length = static_cast<MTLD3D12RootSignature *>(pDesc->pRootSignature)->GetBlob(&rootsig.bytecode);
    } else {
      rootsig.bytecode = pDesc->CS.pShaderBytecode;
      rootsig.bytecode_length = pDesc->CS.BytecodeLength;
    }
    rootsig.next = nullptr;

    SM50_SHADER_COMMON_DATA common;
    common.flags = {};
    common.type = SM50_SHADER_COMMON;
    common.metal_version = SM50_SHADER_METAL_310;
    common.next = &rootsig;

    if (SM50Initialize(pDesc->CS.pShaderBytecode, pDesc->CS.BytecodeLength, &shader_cs, &ref_cs, &sm50_err)) {
      ERR("Failed to parse cs shader");
      return E_FAIL;
    }

    threadgroup_size = {ref_cs.ThreadgroupSize[0], ref_cs.ThreadgroupSize[1], ref_cs.ThreadgroupSize[2]};

    sm50_bitcode_t cs_bitcode;

    if (SM50Compile(shader_cs, (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&common, "cs_main", &cs_bitcode, &sm50_err)) {
      ERR("Failed to compile cs shader");
      return E_FAIL;
    }

    SM50_COMPILED_BITCODE cs_bitcode_compiled;

    SM50GetCompiledBitcode(cs_bitcode, &cs_bitcode_compiled);

    auto cs_data = WMT::MakeDispatchData(cs_bitcode_compiled.Data, cs_bitcode_compiled.Size);

    auto metal = device_->GetMTLDevice();

    WMT::Reference<WMT::Error> err;

    auto cs_lib = metal.newLibrary(cs_data, err);

    auto cs_func = cs_lib.newFunction("cs_main");

    // PSO
    {
      WMTComputePipelineInfo info;
      WMT::InitializeComputePipelineInfo(info);
      info.compute_function = cs_func;

      pso = metal.newComputePipelineState(info, err);
      if (!pso) {
        ERR("Failed to create compute PSO: ", err.description().getUTF8String());
        return E_FAIL;
      }
    }

    return S_OK;
  }

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) || riid == __uuidof(ID3D12DeviceChild) ||
        riid == __uuidof(ID3D12Pageable) || riid == __uuidof(ID3D12PipelineState)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D12PipelineState), riid)) {
      WARN("D3D12ComputePipelineState: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  virtual HRESULT STDMETHODCALLTYPE
  GetCachedBlob(ID3DBlob **blob) {
    IMPLEMENT_ME
    return E_NOTIMPL;
  }
};

HRESULT
CreateComputePipelineState(
    MTLD3D12Device *pDevice, const D3D12_COMPUTE_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState
) {
  auto pso = Com(new MTLD3D12ComputePipelineStateImpl(pDevice));
  HRESULT hr = pso->Initialize(pDesc);
  if (FAILED(hr))
    return hr;
  return pso->QueryInterface(riid, ppPipelineState);
};

} // namespace dxmt