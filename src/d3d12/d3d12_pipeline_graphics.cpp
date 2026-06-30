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

#include "d3d12_device.hpp"
#include "d3d12_pageable.hpp"
#include "dxmt_format.hpp"
#include "com/com_object.hpp"
#include "com/com_pointer.hpp"
#include "sha1/sha1_util.hpp"
#include "airconv_public.h"
#include "DXBCParser/DXBCUtils.h"

namespace dxmt {

constexpr WMTCompareFunction kCompareFunctionMap[] = {
    WMTCompareFunctionNever, // padding 0
    WMTCompareFunctionNever, // 1 - 1
    WMTCompareFunctionLess,    WMTCompareFunctionEqual,    WMTCompareFunctionLessEqual,
    WMTCompareFunctionGreater, WMTCompareFunctionNotEqual, WMTCompareFunctionGreaterEqual,
    WMTCompareFunctionAlways // 8 - 1
};

constexpr WMTStencilOperation kStencilOperationMap[] = {
    WMTStencilOperationZero, // invalid
    WMTStencilOperationKeep,
    WMTStencilOperationZero,
    WMTStencilOperationReplace,
    // D3D11_STENCIL_OP_INCR_SAT: Increment the stencil value by 1, and clamp
    // the result.
    WMTStencilOperationIncrementClamp,
    WMTStencilOperationDecrementClamp,
    WMTStencilOperationInvert,
    // D3D11_STENCIL_OP_INCR:Increment the stencil value by 1, and wrap the
    // result if necessary.

    WMTStencilOperationIncrementWrap,
    WMTStencilOperationDecrementWrap,

};

constexpr WMTBlendOperation kBlendOpMap[] = {
    WMTBlendOperationAdd, // padding 0
    WMTBlendOperationAdd, WMTBlendOperationSubtract, WMTBlendOperationReverseSubtract,
    WMTBlendOperationMin, WMTBlendOperationMax,
};

constexpr WMTLogicOperation kLogicOpMap[] = {
    WMTLogicOperationClear,        WMTLogicOperationSet,         WMTLogicOperationCopy,
    WMTLogicOperationCopyInverted, WMTLogicOperationNoOp,        WMTLogicOperationInvert,
    WMTLogicOperationAnd,          WMTLogicOperationNand,        WMTLogicOperationOr,
    WMTLogicOperationNor,          WMTLogicOperationXor,         WMTLogicOperationEquiv,
    WMTLogicOperationAndReverse,   WMTLogicOperationAndInverted, WMTLogicOperationOrReverse,
    WMTLogicOperationOrInverted,
};

constexpr WMTBlendFactor kBlendFactorMap[] = {
    WMTBlendFactorZero, // padding 0
    WMTBlendFactorZero,
    WMTBlendFactorOne,
    WMTBlendFactorSourceColor,
    WMTBlendFactorOneMinusSourceColor,
    WMTBlendFactorSourceAlpha,
    WMTBlendFactorOneMinusSourceAlpha,
    WMTBlendFactorDestinationAlpha,
    WMTBlendFactorOneMinusDestinationAlpha,
    WMTBlendFactorDestinationColor,
    WMTBlendFactorOneMinusDestinationColor,
    WMTBlendFactorSourceAlphaSaturated,
    WMTBlendFactorZero,       // invalid,12
    WMTBlendFactorZero,       // invalid,13
    WMTBlendFactorBlendColor, // BLEND_FACTOR
    WMTBlendFactorOneMinusBlendColor,
    WMTBlendFactorSource1Color,
    WMTBlendFactorOneMinusSource1Color,
    WMTBlendFactorSource1Alpha,
    WMTBlendFactorOneMinusSource1Alpha,
    WMTBlendFactorBlendAlpha,
    WMTBlendFactorOneMinusBlendAlpha,
};

constexpr WMTBlendFactor kBlendAlphaFactorMap[] = {
    WMTBlendFactorZero, // padding 0
    WMTBlendFactorZero,
    WMTBlendFactorOne,
    WMTBlendFactorSourceColor,
    WMTBlendFactorOneMinusSourceColor,
    WMTBlendFactorSourceAlpha,
    WMTBlendFactorOneMinusSourceAlpha,
    WMTBlendFactorDestinationAlpha,
    WMTBlendFactorOneMinusDestinationAlpha,
    WMTBlendFactorDestinationColor,
    WMTBlendFactorOneMinusDestinationColor,
    WMTBlendFactorSourceAlphaSaturated,
    WMTBlendFactorZero,       // invalid,12
    WMTBlendFactorZero,       // invalid,13
    WMTBlendFactorBlendAlpha, // BLEND_FACTOR
    WMTBlendFactorOneMinusBlendAlpha,
    WMTBlendFactorSource1Color,
    WMTBlendFactorOneMinusSource1Color,
    WMTBlendFactorSource1Alpha,
    WMTBlendFactorOneMinusSource1Alpha,
};

constexpr WMTColorWriteMask kColorWriteMaskMap[] = {
    // 0000
    WMTColorWriteMaskNone,
    // 0001
    WMTColorWriteMaskRed,
    // 0010
    WMTColorWriteMaskGreen,
    // 0011,
    WMTColorWriteMaskRed | WMTColorWriteMaskGreen,
    // 0100
    WMTColorWriteMaskBlue,
    // 0101
    WMTColorWriteMaskBlue | WMTColorWriteMaskRed,
    // 0110
    WMTColorWriteMaskBlue | WMTColorWriteMaskGreen,
    // 0111
    WMTColorWriteMaskBlue | WMTColorWriteMaskRed | WMTColorWriteMaskGreen,

    // 1000
    WMTColorWriteMaskAlpha,
    // 1001
    WMTColorWriteMaskAlpha | WMTColorWriteMaskRed,
    // 1010
    WMTColorWriteMaskAlpha | WMTColorWriteMaskGreen,
    // 1011,
    WMTColorWriteMaskAlpha | WMTColorWriteMaskRed | WMTColorWriteMaskGreen,
    // 1100
    WMTColorWriteMaskAlpha | WMTColorWriteMaskBlue,
    // 0101
    WMTColorWriteMaskAlpha | WMTColorWriteMaskBlue | WMTColorWriteMaskRed,
    // 1110
    WMTColorWriteMaskAlpha | WMTColorWriteMaskBlue | WMTColorWriteMaskGreen,
    // 1111
    WMTColorWriteMaskAlpha | WMTColorWriteMaskBlue | WMTColorWriteMaskRed | WMTColorWriteMaskGreen,
};

HRESULT
ExtractMTLInputLayoutElements(
    MTLD3D12Device *device, const void *pShaderBytecodeWithInputSignature,
    const D3D12_INPUT_ELEMENT_DESC *pInputElementDescs, uint32_t NumElements, SM50_IA_INPUT_ELEMENT *pInputLayout,
    uint32_t *pNumElementsOut
) {

  using namespace microsoft;
  uint16_t append_offset[32] = {0};
  uint32_t register_mask = 0;

  CSignatureParser parser;
  HRESULT hr = DXBCGetInputSignature(pShaderBytecodeWithInputSignature, &parser);
  if (FAILED(hr)) {
    return hr;
  }
  const D3D11_SIGNATURE_PARAMETER *pParameters;
  auto num_parameters = parser.GetParameters(&pParameters);

  UINT attribute_count = 0;
  for (UINT j = 0; j < NumElements; j++) {
    auto &desc = pInputElementDescs[j];

    MTL_DXGI_FORMAT_DESC metal_format;
    if (FAILED(MTLQueryDXGIFormat(device->GetMTLDevice(), desc.Format, metal_format))) {
      ERR("CreateInputLayout: Unsupported vertex format: ", desc.Format);
      return E_FAIL;
    }

    if (!metal_format.AttributeFormat) {
      ERR("CreateInputLayout: Unsupported vertex format: ", desc.Format);
      return E_INVALIDARG;
    }
    if (!metal_format.BytesPerTexel) {
      ERR("CreateInputLayout: not an ordinary or packed format: ", desc.Format);
      return E_INVALIDARG;
    }
    auto aligned_byte_offset = desc.AlignedByteOffset == D3D11_APPEND_ALIGNED_ELEMENT
                                   ? align(append_offset[desc.InputSlot], std::min(4u, metal_format.BytesPerTexel))
                                   : desc.AlignedByteOffset;
    append_offset[desc.InputSlot] = aligned_byte_offset + metal_format.BytesPerTexel;

    auto pSig = std::find_if(pParameters, pParameters + num_parameters, [&](const D3D11_SIGNATURE_PARAMETER &inputSig) {
      return desc.SemanticIndex == inputSig.SemanticIndex && strcasecmp(desc.SemanticName, inputSig.SemanticName) == 0;
    });
    if (pSig == pParameters + num_parameters)
      continue; // shader has no such input register, so skip it
    auto &inputSig = *pSig;
    auto &attribute = pInputLayout[attribute_count++];

    attribute.format = metal_format.AttributeFormat;

    attribute.slot = desc.InputSlot;
    attribute.reg = inputSig.Register;
    attribute.aligned_byte_offset = aligned_byte_offset;
    // the layout stride is provided in IASetVertexBuffer
    attribute.step_function = desc.InputSlotClass;
    attribute.step_rate =
        desc.InputSlotClass == D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA ? desc.InstanceDataStepRate : 1;
    register_mask |= (1 << inputSig.Register);
  }
  for (UINT i = 0; i < num_parameters; i++) {
    auto &inputSig = pParameters[i];
    if (inputSig.SystemValue != D3D10_SB_NAME_UNDEFINED)
      continue; // ignore SIV & SGV
    if (!(register_mask & (1 << inputSig.Register))) {
      WARN(
          "CreateInputLayout: Vertex shader expects ", inputSig.SemanticName, "_", inputSig.SemanticIndex,
          " but it's not in input layout element descriptors"
      );
      return E_INVALIDARG;
    }
  }
  *pNumElementsOut = attribute_count;

  return S_OK;
}

class MTLD3D12GraphicsPipelineStateImpl : public MTLD3D12Pageable<MTLD3D12GraphicsPipelineState> {

  sm50_shader_t shader_vs;
  sm50_shader_t shader_ps;
  MTL_SHADER_REFLECTION ref_vs;
  MTL_SHADER_REFLECTION ref_ps;

public:
  MTLD3D12GraphicsPipelineStateImpl(MTLD3D12Device *pDevice) :
      MTLD3D12Pageable<MTLD3D12GraphicsPipelineState>(pDevice) {
    IsComputePipelineState = FALSE;
  }

  bool
  BlendFactorIsDualSource(D3D12_BLEND Blend) {
    return (Blend >= D3D12_BLEND_SRC1_COLOR) && (Blend <= D3D12_BLEND_INV_SRC1_ALPHA);
  }

  HRESULT
  Initialize(const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc) {
    if (pDesc->StreamOutput.NumEntries) {
      ERR("CreatePipelineState: SO not supported");
      return E_NOTIMPL;
    }

    if (pDesc->GS.pShaderBytecode) {
      ERR("CreatePipelineState: GS not supported");
      return E_NOTIMPL;
    }

    if (pDesc->HS.pShaderBytecode || pDesc->DS.pShaderBytecode) {
      ERR("CreatePipelineState: Tess not supported");
      return E_NOTIMPL;
    }

    HRESULT hr;
    sm50_error_t sm50_err;
    auto metal = device_->GetMTLDevice();
    WMT::Reference<WMT::Error> err;
    WMT::Reference<WMT::Function> vs_func, ps_func;

    SM50_SHADER_COMMON_DATA common;
    common.flags = {};
    common.type = SM50_SHADER_COMMON;
    common.metal_version = SM50_SHADER_METAL_310;
    common.next = nullptr;

    if (pDesc->VS.pShaderBytecode) {
      if (SM50Initialize(pDesc->VS.pShaderBytecode, pDesc->VS.BytecodeLength, &shader_vs, &ref_vs, &sm50_err)) {
        ERR("Failed to parse vs shader");
        return E_FAIL;
      }
      SM50_SHADER_IA_INPUT_LAYOUT_DATA data_ia_layout;
      data_ia_layout.type = SM50_SHADER_IA_INPUT_LAYOUT;
      data_ia_layout.index_buffer_format = SM50_INDEX_BUFFER_FORMAT_NONE;
      std::vector<SM50_IA_INPUT_ELEMENT> elements(pDesc->InputLayout.NumElements);
      hr = ExtractMTLInputLayoutElements(
          device_, pDesc->VS.pShaderBytecode, pDesc->InputLayout.pInputElementDescs, pDesc->InputLayout.NumElements,
          elements.data(), &data_ia_layout.num_elements
      );
      elements.resize(data_ia_layout.num_elements);
      data_ia_layout.elements = elements.data();
      if (FAILED(hr)) {
        return hr;
      }
      slot_mask = 0;
      for (auto &element : elements) {
        slot_mask |= (1 << element.slot);
      }
      data_ia_layout.slot_mask = slot_mask;
      data_ia_layout.next = &common;

      SM50_SHADER_ROOT_SIGNATURE_DATA rootsig;
      rootsig.type = SM50_SHADER_ROOT_SIGNATURE;
      if (pDesc->pRootSignature) {
        rootsig.bytecode_length =
            static_cast<MTLD3D12RootSignature *>(pDesc->pRootSignature)->GetBlob(&rootsig.bytecode);
      } else {
        rootsig.bytecode = pDesc->VS.pShaderBytecode;
        rootsig.bytecode_length = pDesc->VS.BytecodeLength;
      }
      rootsig.next = &data_ia_layout;

      sm50_bitcode_t vs_bitcode;

      if (SM50Compile(
              shader_vs, (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&rootsig, "vs_main", &vs_bitcode, &sm50_err
          )) {
        ERR("Failed to compile vs shader");
        return E_FAIL;
      }

      SM50_COMPILED_BITCODE vs_bitcode_compiled;
      SM50GetCompiledBitcode(vs_bitcode, &vs_bitcode_compiled);
      auto vs_data = WMT::MakeDispatchData(vs_bitcode_compiled.Data, vs_bitcode_compiled.Size);
      auto vs_lib = metal.newLibrary(vs_data, err);
      vs_func = vs_lib.newFunction("vs_main");
    } else {
      ERR("no vertex shader");
      return E_INVALIDARG;
    }

    WMTRenderPipelineInfo info;
    WMT::InitializeRenderPipelineInfo(info);

    bool dual_source_blending = false;

    uint32_t effective_dual_source_rtvs = 0;

    // PSO
    {
      MTL_DXGI_FORMAT_DESC format_desc;
      for (unsigned i = 0; i < pDesc->NumRenderTargets; i++) {
        if (pDesc->RTVFormats[i] == DXGI_FORMAT_UNKNOWN)
          continue;
        if (i >= 2 && dual_source_blending)
          break;
        auto &rt = info.colors[i];
        auto Format = pDesc->RTVFormats[i];
        if (FAILED(hr = MTLQueryDXGIFormat(device_->GetMTLDevice(), Format, format_desc))) {
          return hr;
        }
        rt.pixel_format = format_desc.PixelFormat;

        auto renderTarget = pDesc->BlendState.RenderTarget[pDesc->BlendState.IndependentBlendEnable ? i : 0];

        if (renderTarget.BlendEnable && renderTarget.LogicOpEnable)
          return E_INVALIDARG;

        if (pDesc->BlendState.IndependentBlendEnable && renderTarget.LogicOpEnable)
          return E_INVALIDARG;

        rt.write_mask = kColorWriteMaskMap[renderTarget.RenderTargetWriteMask];
        if (renderTarget.BlendEnable) {
          // TODO
          // if (!any_bit_set(device_->GetMTLPixelFormatCapability(rt.pixel_format) & FormatCapability::Blend)) {
          //   return E_INVALIDARG;
          // }
          if (BlendFactorIsDualSource(renderTarget.SrcBlendAlpha) || BlendFactorIsDualSource(renderTarget.SrcBlend) ||
              BlendFactorIsDualSource(renderTarget.DestBlendAlpha) || BlendFactorIsDualSource(renderTarget.DestBlend)) {
            dual_source_blending = true;
          }
          rt.alpha_blend_operation = kBlendOpMap[renderTarget.BlendOpAlpha];
          rt.rgb_blend_operation = kBlendOpMap[renderTarget.BlendOp];
          rt.blending_enabled = renderTarget.BlendEnable;
          rt.src_alpha_blend_factor = kBlendAlphaFactorMap[renderTarget.SrcBlendAlpha];
          rt.src_rgb_blend_factor = kBlendFactorMap[renderTarget.SrcBlend];
          rt.dst_alpha_blend_factor = kBlendAlphaFactorMap[renderTarget.DestBlendAlpha];
          rt.dst_rgb_blend_factor = kBlendFactorMap[renderTarget.DestBlend];
        }
        if (i < 2)
          effective_dual_source_rtvs++;
      }

      if (dual_source_blending && effective_dual_source_rtvs > 1)
        return E_INVALIDARG;

      if (pDesc->DSVFormat != DXGI_FORMAT_UNKNOWN) {
        if (FAILED(hr = MTLQueryDXGIFormat(device_->GetMTLDevice(), pDesc->DSVFormat, format_desc))) {
          return hr;
        }
        // TODO
        // if (any_bit_set(
        //         device_->GetMTLPixelFormatCapability(format_desc.PixelFormat) & FormatCapability::DepthStencil
        //     )) {
        //   info.depth_pixel_format = format_desc.PixelFormat;
        //   if (DepthStencilPlanarFlags(info.depth_pixel_format) & 2)
        //     info.stencil_pixel_format = info.depth_pixel_format;
        // }
      }
    }

    if (pDesc->PS.pShaderBytecode) {
      auto sha1 = Sha1HashState::compute(pDesc->PS.pShaderBytecode, pDesc->PS.BytecodeLength);

      std::string ps_name = "ps_main" + sha1.string().substr(0, 8);

      if (SM50Initialize(pDesc->PS.pShaderBytecode, pDesc->PS.BytecodeLength, &shader_ps, &ref_ps, &sm50_err)) {
        ERR("Failed to parse ps shader");
        return E_FAIL;
      }
      SM50_SHADER_PSO_PIXEL_SHADER_DATA data_ps;
      data_ps.dual_source_blending = dual_source_blending;
      data_ps.disable_depth_output = false;
      data_ps.unorm_output_reg_mask = 0;
      data_ps.sample_mask = pDesc->SampleMask;
      data_ps.type = SM50_SHADER_PSO_PIXEL_SHADER;
      data_ps.next = &common;

      SM50_SHADER_ROOT_SIGNATURE_DATA rootsig;
      rootsig.type = SM50_SHADER_ROOT_SIGNATURE;
      if (pDesc->pRootSignature) {
        rootsig.bytecode_length =
            static_cast<MTLD3D12RootSignature *>(pDesc->pRootSignature)->GetBlob(&rootsig.bytecode);
      } else {
        rootsig.bytecode = pDesc->PS.pShaderBytecode;
        rootsig.bytecode_length = pDesc->PS.BytecodeLength;
      }
      rootsig.next = &data_ps;

      sm50_bitcode_t ps_bitcode;
      if (SM50Compile(
              shader_ps, (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&rootsig, ps_name.c_str(), &ps_bitcode, &sm50_err
          )) {
        ERR("Failed to compile ps shader");
        return E_FAIL;
      }
      SM50_COMPILED_BITCODE ps_bitcode_compiled;
      SM50GetCompiledBitcode(ps_bitcode, &ps_bitcode_compiled);
      auto ps_data = WMT::MakeDispatchData(ps_bitcode_compiled.Data, ps_bitcode_compiled.Size);
      auto ps_lib = metal.newLibrary(ps_data, err);
      ps_func = ps_lib.newFunction(ps_name.c_str());
    }

    // PSO
    {
      info.vertex_function = vs_func.handle;
      info.fragment_function = ps_func.handle;

      switch (pDesc->PrimitiveTopologyType) {
      case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT:
        info.input_primitive_topology = WMTPrimitiveTopologyClassPoint;
        break;
      case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE:
        info.input_primitive_topology = WMTPrimitiveTopologyClassLine;
        break;
      case D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE:
      case D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH:
        info.input_primitive_topology = WMTPrimitiveTopologyClassTriangle;
        break;
      default:
        break;
      }

      info.raster_sample_count = pDesc->SampleDesc.Count;

      pso = metal.newRenderPipelineState(info, err);

      if (!pso) {
        ERR("Failed to create PSO: ", err.description().getUTF8String());
        return E_FAIL;
      }
    }

    // DSSO
    {
      WMTDepthStencilInfo info;
      info.depth_compare_function = WMTCompareFunctionAlways;
      info.depth_write_enabled = false;
      info.front_stencil.enabled = false;
      info.back_stencil.enabled = false;
      if (pDesc->DepthStencilState.DepthEnable) {
        info.depth_compare_function = kCompareFunctionMap[pDesc->DepthStencilState.DepthFunc];
        info.depth_write_enabled = pDesc->DepthStencilState.DepthWriteMask == D3D12_DEPTH_WRITE_MASK_ALL;
      }

      if (pDesc->DepthStencilState.StencilEnable) {
        info.front_stencil.enabled = true;
        info.back_stencil.enabled = true;
        {
          info.front_stencil.depth_stencil_pass_op =
              (kStencilOperationMap[pDesc->DepthStencilState.FrontFace.StencilPassOp]);
          info.front_stencil.stencil_fail_op = (kStencilOperationMap[pDesc->DepthStencilState.FrontFace.StencilFailOp]);
          info.front_stencil.depth_fail_op =
              (kStencilOperationMap[pDesc->DepthStencilState.FrontFace.StencilDepthFailOp]);
          info.front_stencil.stencil_compare_function =
              kCompareFunctionMap[pDesc->DepthStencilState.FrontFace.StencilFunc];
          info.front_stencil.write_mask = pDesc->DepthStencilState.StencilWriteMask;
          info.front_stencil.read_mask = pDesc->DepthStencilState.StencilReadMask;
        }
        {
          info.back_stencil.depth_stencil_pass_op =
              (kStencilOperationMap[pDesc->DepthStencilState.BackFace.StencilPassOp]);
          info.back_stencil.stencil_fail_op = (kStencilOperationMap[pDesc->DepthStencilState.BackFace.StencilFailOp]);
          info.back_stencil.depth_fail_op =
              (kStencilOperationMap[pDesc->DepthStencilState.BackFace.StencilDepthFailOp]);
          info.back_stencil.stencil_compare_function =
              kCompareFunctionMap[pDesc->DepthStencilState.BackFace.StencilFunc];
          info.back_stencil.write_mask = pDesc->DepthStencilState.StencilWriteMask;
          info.back_stencil.read_mask = pDesc->DepthStencilState.StencilReadMask;
        }
      }

      dsso = metal.newDepthStencilState(info);

      if (!dsso) {
        ERR("Failed to create DSSO");
        return E_FAIL;
      }
    }

    {
      fill_mode =
          pDesc->RasterizerState.FillMode == D3D12_FILL_MODE_SOLID ? WMTTriangleFillModeFill : WMTTriangleFillModeLines;
      switch (pDesc->RasterizerState.CullMode) {
      case D3D12_CULL_MODE_BACK:
        cull_mode = WMTCullModeBack;
        break;
      case D3D12_CULL_MODE_FRONT:
        cull_mode = WMTCullModeFront;
        break;
      case D3D12_CULL_MODE_NONE:
        cull_mode = WMTCullModeNone;
        break;
      }
      depth_clip_mode = pDesc->RasterizerState.DepthClipEnable ? WMTDepthClipModeClip : WMTDepthClipModeClamp;
      depth_bias = pDesc->RasterizerState.DepthBias;
      scole_scale = pDesc->RasterizerState.SlopeScaledDepthBias;
      depth_bias_clamp = pDesc->RasterizerState.DepthBiasClamp;
      winding = pDesc->RasterizerState.FrontCounterClockwise ? WMTWindingCounterClockwise : WMTWindingClockwise;
      forced_sample_count = pDesc->RasterizerState.ForcedSampleCount;
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
      WARN("D3D12GraphicsPipelineState: Unknown interface query ", str::format(riid));
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
CreateGraphicsPipelineState(
    MTLD3D12Device *pDevice, const D3D12_GRAPHICS_PIPELINE_STATE_DESC *pDesc, REFIID riid, void **ppPipelineState
) {
  InitReturnPtr(ppPipelineState);
  auto pso = Com(new MTLD3D12GraphicsPipelineStateImpl(pDevice));
  HRESULT hr = pso->Initialize(pDesc);
  if (FAILED(hr))
    return hr;
  return pso->QueryInterface(riid, ppPipelineState);
};

} // namespace dxmt