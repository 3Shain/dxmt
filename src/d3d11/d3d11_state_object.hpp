#pragma once

#include "d3d11_device_child.h"
#include "Metal/MTLRenderCommandEncoder.hpp"
#include "Metal/MTLRenderPipeline.hpp"
#include "Metal/MTLSampler.hpp"

namespace dxmt {

class MTLD3D11Device;

// SamplerState

class MTLD3D11SamplerState : public MTLD3D11StateObject<ID3D11SamplerState> {

public:
  friend class MTLD3D11DeviceContext;
  MTLD3D11SamplerState(IMTLD3D11Device *device, MTL::SamplerState *samplerState,
                       const D3D11_SAMPLER_DESC &desc);
  ~MTLD3D11SamplerState();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;

  void STDMETHODCALLTYPE GetDesc(D3D11_SAMPLER_DESC *pDesc) final;

private:
  const D3D11_SAMPLER_DESC desc_;
  Obj<MTL::SamplerState> metal_sampler_state_;
};

// BlendState

class MTLD3D11BlendState : public MTLD3D11StateObject<ID3D11BlendState1> {

public:
  friend class MTLD3D11DeviceContext;
  MTLD3D11BlendState(IMTLD3D11Device *device, const D3D11_BLEND_DESC1 &desc);
  ~MTLD3D11BlendState();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;

  void STDMETHODCALLTYPE GetDesc(D3D11_BLEND_DESC *pDesc) final;

  void STDMETHODCALLTYPE GetDesc1(D3D11_BLEND_DESC1 *pDesc) final;

  void SetupMetalPipelineDescriptor(MTL::RenderPipelineDescriptor *pDescriptor);

private:
  const D3D11_BLEND_DESC1 desc_;
};

// RasterizerState

class MTLD3D11RasterizerState
    : public MTLD3D11StateObject<ID3D11RasterizerState1> {

public:
  friend class MTLD3D11DeviceContext;
  MTLD3D11RasterizerState(IMTLD3D11Device *device,
                          const D3D11_RASTERIZER_DESC1 &desc);
  ~MTLD3D11RasterizerState();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;

  void STDMETHODCALLTYPE GetDesc(D3D11_RASTERIZER_DESC *pDesc) final;

  void STDMETHODCALLTYPE GetDesc1(D3D11_RASTERIZER_DESC1 *pDesc) final;

  void STDMETHODCALLTYPE
  SetupRasterizerState(MTL::RenderCommandEncoder *encoder);

  bool IsScissorEnabled();

private:
  const D3D11_RASTERIZER_DESC1 m_desc;
};

// DepthStencilState

class MTLD3D11DepthStencilState
    : public MTLD3D11StateObject<ID3D11DepthStencilState> {

public:
  friend class MTLD3D11DeviceContext;
  MTLD3D11DepthStencilState(IMTLD3D11Device *device,
                            MTL::DepthStencilState *state,
                            const D3D11_DEPTH_STENCIL_DESC &desc);
  ~MTLD3D11DepthStencilState();

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) final;

  void STDMETHODCALLTYPE GetDesc(D3D11_DEPTH_STENCIL_DESC *pDesc) final;

  void STDMETHODCALLTYPE
  SetupDepthStencilState(MTL::RenderCommandEncoder *encoder);

private:
  const D3D11_DEPTH_STENCIL_DESC m_desc;
  Obj<MTL::DepthStencilState> metal_depth_stencil_state_;
};

constexpr MTL::CompareFunction compareFunctionMap[] = {
    MTL::CompareFunction::CompareFunctionNever, // pad 0 
    MTL::CompareFunction::CompareFunctionNever, // 1 - 1
    MTL::CompareFunction::CompareFunctionLess,
    MTL::CompareFunction::CompareFunctionEqual,
    MTL::CompareFunction::CompareFunctionLessEqual,
    MTL::CompareFunction::CompareFunctionGreater,
    MTL::CompareFunction::CompareFunctionNotEqual,
    MTL::CompareFunction::CompareFunctionGreaterEqual,
    MTL::CompareFunction::CompareFunctionAlways // 8 - 1
};

constexpr MTL::SamplerAddressMode addressModeMap[] = {
    // MTL: Texture coordinates wrap to the other side of the texture,
    // effectively keeping only the fractional part of the texture coordinate.

    // MSDN: Tile the texture at every (u,v) integer junction. For example, for
    // u values between 0 and 3, the texture is repeated three times.
    MTL::SamplerAddressMode::SamplerAddressModeRepeat, // 1 - 1

    // MTL:Between -1.0 and 1.0, the texture coordinates are mirrored across the
    // axis; outside -1.0 and 1.0, the image is repeated.

    // MSDN: Flip the texture at every (u,v) integer junction. For u values
    // between 0 and 1, for example, the texture is addressed normally; between
    // 1 and 2, the texture is flipped (mirrored); between 2 and 3, the texture
    // is normal again; and so on.
    MTL::SamplerAddressMode::SamplerAddressModeMirrorRepeat,
    // MTL: Texture coordinates are clamped between 0.0 and 1.0, inclusive.
    // MSDN: Texture coordinates outside the range [0.0, 1.0] are set to the
    // texture color at 0.0 or 1.0, respectively.
    MTL::SamplerAddressMode::SamplerAddressModeClampToEdge,

    // MTL: Out-of-range texture coordinates return the value specified by the
    // borderColor property.

    // MSDN: Texture coordinates outside the range [0.0, 1.0] are set to the
    // border color specified in D3D11_SAMPLER_DESC or HLSL code.
    MTL::SamplerAddressMode::SamplerAddressModeClampToBorderColor,
    // MTL: Between -1.0 and 1.0, the texture coordinates are mirrored across
    // the axis; outside -1.0 and 1.0, texture coordinates are clamped.

    // MSDN:
    //  Similar to D3D11_TEXTURE_ADDRESS_MIRROR and D3D11_TEXTURE_ADDRESS_CLAMP.
    //  Takes the absolute value of the texture coordinate (thus, mirroring
    //  around 0), and then clamps to the maximum value.
    MTL::SamplerAddressMode::SamplerAddressModeMirrorClampToEdge // 5 - 1
};

constexpr MTL::StencilOperation stencilOperationMap[] = {
    MTL::StencilOperationZero, // invalid
    MTL::StencilOperationKeep,
    MTL::StencilOperationZero,
    MTL::StencilOperationReplace,
    // D3D11_STENCIL_OP_INCR_SAT: Increment the stencil value by 1, and clamp
    // the result.
    MTL::StencilOperationIncrementClamp,
    MTL::StencilOperationDecrementClamp,
    MTL::StencilOperationInvert,
    // D3D11_STENCIL_OP_INCR:Increment the stencil value by 1, and wrap the
    // result if necessary.

    MTL::StencilOperationIncrementWrap,
    MTL::StencilOperationDecrementWrap,

};

constexpr MTL::BlendOperation g_blend_op_map[] = {
    MTL::BlendOperationAdd, // FIXME: invalid,0
    MTL::BlendOperationAdd,
    MTL::BlendOperationSubtract,
    MTL::BlendOperationReverseSubtract,
    MTL::BlendOperationMin,
    MTL::BlendOperationMax,
};

constexpr MTL::BlendFactor g_blend_factor_map[] = {
    MTL::BlendFactorZero, // FIXME: invalid,0
    MTL::BlendFactorZero,
    MTL::BlendFactorOne,
    MTL::BlendFactorSourceColor,
    MTL::BlendFactorOneMinusSourceColor,
    MTL::BlendFactorSourceAlpha,
    MTL::BlendFactorOneMinusSourceAlpha,
    MTL::BlendFactorDestinationAlpha,
    MTL::BlendFactorOneMinusDestinationAlpha,
    MTL::BlendFactorDestinationColor,
    MTL::BlendFactorOneMinusDestinationColor,
    MTL::BlendFactorSourceAlphaSaturated,
    MTL::BlendFactorZero,       // invalid,12
    MTL::BlendFactorZero,       // invalid,13
    MTL::BlendFactorBlendAlpha, // BLEND_FACTOR
    MTL::BlendFactorOneMinusBlendAlpha,
    MTL::BlendFactorSource1Color,
    MTL::BlendFactorOneMinusSource1Color,
    MTL::BlendFactorSource1Alpha,
    MTL::BlendFactorOneMinusSource1Alpha,
};

const MTL::ColorWriteMask g_color_write_mask_map[] = {
    // 0000
    0,
    // 0001
    MTL::ColorWriteMaskRed,
    // 0010
    MTL::ColorWriteMaskGreen,
    // 0011,
    MTL::ColorWriteMaskRed | MTL::ColorWriteMaskGreen,
    // 0100
    MTL::ColorWriteMaskBlue,
    // 0101
    MTL::ColorWriteMaskBlue | MTL::ColorWriteMaskRed,
    // 0110
    MTL::ColorWriteMaskBlue | MTL::ColorWriteMaskGreen,
    // 0111
    MTL::ColorWriteMaskBlue | MTL::ColorWriteMaskRed | MTL::ColorWriteMaskGreen,

    // 1000
    MTL::ColorWriteMaskAlpha | 0,
    // 1001
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskRed,
    // 1010
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskGreen,
    // 1011,
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskRed |
        MTL::ColorWriteMaskGreen,
    // 1100
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskBlue,
    // 0101
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskBlue | MTL::ColorWriteMaskRed,
    // 1110
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskBlue |
        MTL::ColorWriteMaskGreen,
    // 1111
    MTL::ColorWriteMaskAlpha | MTL::ColorWriteMaskBlue |
        MTL::ColorWriteMaskRed | MTL::ColorWriteMaskGreen,
};

constexpr D3D11_RASTERIZER_DESC1 defaultRasterizerDesc = {
    .FillMode = D3D11_FILL_SOLID,
    .CullMode = D3D11_CULL_BACK,
    .FrontCounterClockwise = FALSE,
    .DepthBias = 0,
    .DepthBiasClamp = 0.0f,
    .SlopeScaledDepthBias = 0.0f,
    .DepthClipEnable = TRUE,
    .ScissorEnable = FALSE,
    .MultisampleEnable = FALSE,
    .AntialiasedLineEnable = FALSE,
    .ForcedSampleCount = 0,
};

constexpr D3D11_RENDER_TARGET_BLEND_DESC1 defaultRenderTargetBlendDesc = {
    .BlendEnable = FALSE,
    .LogicOpEnable = FALSE,
    .SrcBlend = D3D11_BLEND_ONE,
    .DestBlend = D3D11_BLEND_ZERO,
    .BlendOp = D3D11_BLEND_OP_ADD,
    .SrcBlendAlpha = D3D11_BLEND_ONE,
    .DestBlendAlpha = D3D11_BLEND_ZERO,
    .BlendOpAlpha = D3D11_BLEND_OP_ADD,
    .LogicOp = D3D11_LOGIC_OP_NOOP,
    .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
};

constexpr D3D11_BLEND_DESC1 defaultBlendDesc = {
    .AlphaToCoverageEnable = FALSE,
    .IndependentBlendEnable = FALSE,
    .RenderTarget = {defaultRenderTargetBlendDesc}};

constexpr D3D11_SAMPLER_DESC defaultSamplerDesc = {
    .Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR,
    .AddressU = D3D11_TEXTURE_ADDRESS_CLAMP,
    .AddressV = D3D11_TEXTURE_ADDRESS_CLAMP,
    .AddressW = D3D11_TEXTURE_ADDRESS_CLAMP,
    .MipLODBias = 0.0f,
    .MaxAnisotropy = 1,
    .ComparisonFunc = D3D11_COMPARISON_NEVER,
    .BorderColor = {0.0f, 0.0f, 0.0f, 0.0f},
    .MinLOD = 0,
    .MaxLOD = 0,
};

constexpr D3D11_DEPTH_STENCIL_DESC defaultDepthStencilDesc = {
    .DepthEnable = TRUE,
    .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
    .DepthFunc = D3D11_COMPARISON_LESS,
    .StencilEnable = FALSE,
    .StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK,
    .StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK,
    .FrontFace =
        {
            .StencilFailOp = D3D11_STENCIL_OP_KEEP,
            .StencilDepthFailOp = D3D11_STENCIL_OP_KEEP,
            .StencilPassOp = D3D11_STENCIL_OP_KEEP,
            .StencilFunc = D3D11_COMPARISON_ALWAYS,
        },
    .BackFace = {
        .StencilFailOp = D3D11_STENCIL_OP_KEEP,
        .StencilDepthFailOp = D3D11_STENCIL_OP_KEEP,
        .StencilPassOp = D3D11_STENCIL_OP_KEEP,
        .StencilFunc = D3D11_COMPARISON_ALWAYS,
    }};

} // namespace dxmt
