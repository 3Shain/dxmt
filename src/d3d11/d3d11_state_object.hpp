#pragma once

#include "Metal/MTLRenderPipeline.hpp"
#include "com/com_guid.hpp"
#include "d3d11_3.h"
#include "d3d11_device.hpp"
#include "d3d11_device_child.hpp"

DEFINE_COM_INTERFACE("77f0bbd5-2be7-4e9e-ad61-70684ff19e01",
                     IMTLD3D11SamplerState)
    : public ID3D11SamplerState {
  virtual MTL::SamplerState *GetSamplerState() = 0;
  virtual uint64_t GetArgumentHandle() = 0;
};

DEFINE_COM_INTERFACE("03629ed8-bcdd-4582-8997-3817209a34f4",
                     IMTLD3D11RasterizerState)
    : public ID3D11RasterizerState2 {
  virtual void SetupRasterizerState(MTL::RenderCommandEncoder * encoder) = 0;
  virtual bool IsScissorEnabled() = 0;
};

DEFINE_COM_INTERFACE("b01aaffa-b4d3-478a-91be-6195f215aaba",
                     IMTLD3D11DepthStencilState)
    : public ID3D11DepthStencilState {
  virtual MTL::DepthStencilState *GetDepthStencilState(
      uint32_t planar_flags) = 0;
};

DEFINE_COM_INTERFACE("279a1d66-2fc1-460c-a0a7-a7a5f2b7a48f",
                     IMTLD3D11BlendState)
    : public ID3D11BlendState1 {
  virtual void SetupMetalPipelineDescriptor(MTL::RenderPipelineDescriptor *
                                            render_pipeline_descriptor) = 0;
};

namespace std {
template <> struct hash<D3D11_SAMPLER_DESC> {
  size_t operator()(const D3D11_SAMPLER_DESC &v) const noexcept {
    constexpr size_t binsize = sizeof(v);
    return std::hash<string_view>{}(
        {reinterpret_cast<const char *>(&v), binsize});
  };
};

template <> struct equal_to<D3D11_SAMPLER_DESC> {
  bool operator()(const D3D11_SAMPLER_DESC &x,
                  const D3D11_SAMPLER_DESC &y) const {
    constexpr size_t binsize = sizeof(x);
    return std::string_view({reinterpret_cast<const char *>(&x), binsize}) ==
           std::string_view({reinterpret_cast<const char *>(&y), binsize});
  }
};

template <> struct hash<D3D11_BLEND_DESC1> {
  size_t operator()(const D3D11_BLEND_DESC1 &v) const noexcept {
    constexpr size_t binsize = sizeof(v);
    return std::hash<string_view>{}(
        {reinterpret_cast<const char *>(&v), binsize});
  };
};

template <> struct equal_to<D3D11_BLEND_DESC1> {
  bool operator()(const D3D11_BLEND_DESC1 &x,
                  const D3D11_BLEND_DESC1 &y) const {
    constexpr size_t binsize = sizeof(x);
    return std::string_view({reinterpret_cast<const char *>(&x), binsize}) ==
           std::string_view({reinterpret_cast<const char *>(&y), binsize});
  }
};
template <> struct hash<D3D11_RASTERIZER_DESC2> {
  size_t operator()(const D3D11_RASTERIZER_DESC2 &v) const noexcept {
    constexpr size_t binsize = sizeof(v);
    return std::hash<string_view>{}(
        {reinterpret_cast<const char *>(&v), binsize});
  };
};

template <> struct equal_to<D3D11_RASTERIZER_DESC2> {
  bool operator()(const D3D11_RASTERIZER_DESC2 &x,
                  const D3D11_RASTERIZER_DESC2 &y) const {
    constexpr size_t binsize = sizeof(x);
    return std::string_view({reinterpret_cast<const char *>(&x), binsize}) ==
           std::string_view({reinterpret_cast<const char *>(&y), binsize});
  }
};

template <> struct hash<D3D11_DEPTH_STENCIL_DESC> {
  size_t operator()(const D3D11_DEPTH_STENCIL_DESC &v) const noexcept {
    constexpr size_t binsize = sizeof(v);
    return std::hash<string_view>{}(
        {reinterpret_cast<const char *>(&v), binsize});
  };
};

template <> struct equal_to<D3D11_DEPTH_STENCIL_DESC> {
  bool operator()(const D3D11_DEPTH_STENCIL_DESC &x,
                  const D3D11_DEPTH_STENCIL_DESC &y) const {
    constexpr size_t binsize = sizeof(x);
    return std::string_view({reinterpret_cast<const char *>(&x), binsize}) ==
           std::string_view({reinterpret_cast<const char *>(&y), binsize});
  }
};
} // namespace std

namespace dxmt {

template <typename DESC, typename Object> class StateObjectCache {
public:
  StateObjectCache(IMTLD3D11Device *device) : device(device) {};
  HRESULT CreateStateObject(const DESC *pDesc, Object **ppRet);

private:
  IMTLD3D11Device *device;
  std::unordered_map<DESC, std::unique_ptr<ManagedDeviceChild<Object>>> cache;
  dxmt::mutex mutex_cache;
};

constexpr D3D11_RASTERIZER_DESC2 kDefaultRasterizerDesc = {
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
    .ConservativeRaster = D3D11_CONSERVATIVE_RASTERIZATION_MODE_OFF,
};

constexpr D3D11_RENDER_TARGET_BLEND_DESC1 kDefaultRenderTargetBlendDesc = {
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

constexpr D3D11_BLEND_DESC1 kDefaultBlendDesc = {
    .AlphaToCoverageEnable = FALSE,
    .IndependentBlendEnable = FALSE,
    .RenderTarget = {kDefaultRenderTargetBlendDesc}};

constexpr D3D11_SAMPLER_DESC kDefaultSamplerDesc = {
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

constexpr D3D11_DEPTH_STENCIL_DESC kDefaultDepthStencilDesc = {
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