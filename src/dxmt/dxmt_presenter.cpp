
#include "Metal.hpp"
#include "dxmt_format.hpp"
#include "util_likely.hpp"
#include "winemetal.h"
#include "dxmt_presenter.hpp"

namespace dxmt {

Presenter::Presenter(WMT::Device device, WMT::MetalLayer layer, InternalCommandLibrary &lib, float scale_factor) :
    device_(device),
    layer_(layer),
    lib_(lib) {
  layer_.getProps(layer_props_);
  layer_props_.device = device;
  layer_props_.opaque = true;
  layer_props_.display_sync_enabled = false;
  layer_props_.framebuffer_only = false; // how strangely setting it true results in worse performance
  layer_props_.contents_scale = layer_props_.contents_scale * scale_factor;
}

bool
Presenter::changeLayerProperties(WMTPixelFormat format, WMTColorSpace colorspace, unsigned width, unsigned height) {
  bool should_invalidated = colorspace_ != colorspace;
  should_invalidated |= source_format_ != format;
  source_format_ = format;
  layer_props_.pixel_format = Forget_sRGB(format);
  layer_props_.drawable_height = height;
  layer_props_.drawable_width = width;
  layer_.setProps(layer_props_);
  colorspace_ = colorspace;
  layer_.setColorSpace(colorspace);
  if (should_invalidated)
    pso_valid.clear();
  return should_invalidated;
}

bool
Presenter::changeLayerColorSpace(WMTColorSpace colorspace) {
  bool should_invalidated = colorspace_ != colorspace;
  colorspace_ = colorspace;
  layer_.setColorSpace(colorspace);
  if (should_invalidated)
    pso_valid.clear();
  return should_invalidated;
}

WMT::MetalDrawable
Presenter::encodeCommands(WMT::CommandBuffer cmdbuf, WMT::Texture backbuffer) {

  if (unlikely(!pso_valid.test_and_set()))
    buildRenderPipelineState();

  auto drawable = layer_.nextDrawable();

  float edr_scale = 1.0f;
  if (WMT_COLORSPACE_IS_HDR(colorspace_)) {
    WMTEDRValue edr_value;
    MetalLayer_getEDRValue(layer_, &edr_value);
    edr_scale = edr_value.maximum_edr_color_component_value / edr_value.maximum_potential_edr_color_component_value;
  }

  WMTRenderPassInfo info;
  WMT::InitializeRenderPassInfo(info);
  info.colors[0].load_action = WMTLoadActionDontCare;
  info.colors[0].store_action = WMTStoreActionStore;
  info.colors[0].texture = drawable.texture();
  auto encoder = cmdbuf.renderCommandEncoder(info);
  encoder.setFragmentTexture(backbuffer, 0);

  uint32_t extend[4] = {(uint32_t)layer_props_.drawable_width, (uint32_t)layer_props_.drawable_height, 0, 0};
  if (colorspace_ == WMTColorSpaceHDR_scRGB)
    edr_scale *= 0.8;
  extend[3] = std::bit_cast<uint32_t>(edr_scale);
  encoder.setFragmentBytes(extend, sizeof(extend), 0);
  if (backbuffer.width() == extend[0] && backbuffer.height() == extend[1]) {
    encoder.setRenderPipelineState(present_blit_);
  } else {
    encoder.setRenderPipelineState(present_scale_);
  }
  encoder.setViewport({0, 0, (double)extend[0], (double)extend[1], 0, 1});
  encoder.drawPrimitives(WMTPrimitiveTypeTriangle, 0, 3);

  encoder.endEncoding();

  return drawable;
}

constexpr uint32_t kPresentFCIndex_BackbufferSizeMatched = 0x100;
constexpr uint32_t kPresentFCIndex_HDRPQ = 0x101;
constexpr uint32_t kPresentFCIndex_WithHDRMetadata = 0x102;
constexpr uint32_t kPresentFCIndex_BackbufferIsSRGB = 0x103;

void
Presenter::buildRenderPipelineState() {
  auto pool = WMT::MakeAutoreleasePool();

  auto library = lib_.getLibrary();

  uint32_t true_data = true, false_data = false;
  WMTFunctionConstant constants[4];
  constants[0].data.set(&true_data);
  constants[0].type = WMTDataTypeBool;
  constants[0].index = kPresentFCIndex_BackbufferSizeMatched;
  constants[1].data.set(colorspace_ == WMTColorSpaceHDR_PQ ? &true_data: &false_data);
  constants[1].type = WMTDataTypeBool;
  constants[1].index = kPresentFCIndex_HDRPQ;
  constants[2].data.set(&false_data);
  constants[2].type = WMTDataTypeBool;
  constants[2].index = kPresentFCIndex_WithHDRMetadata;
  constants[3].data.set(Is_sRGBVariant(source_format_) ? &true_data: &false_data);
  constants[3].type = WMTDataTypeBool;
  constants[3].index = kPresentFCIndex_BackbufferIsSRGB;

  WMT::Reference<WMT::Error> error;
  auto vs_present_quad = library.newFunction("vs_present_quad");
  auto fs_present_quad = library.newFunctionWithConstants("fs_present_quad", constants, std::size(constants), error);

  constants[0].data.set(&false_data);
  auto fs_present_quad_scaled =
      library.newFunctionWithConstants("fs_present_quad", constants, std::size(constants), error);
  {
    WMTRenderPipelineInfo present_pipeline;
    WMT::InitializeRenderPipelineInfo(present_pipeline);
    present_pipeline.colors[0].pixel_format = layer_props_.pixel_format;
    present_pipeline.vertex_function = vs_present_quad;
    present_pipeline.fragment_function = fs_present_quad;
    present_blit_ = device_.newRenderPipelineState(present_pipeline, error);
    present_pipeline.fragment_function = fs_present_quad_scaled;
    present_scale_ = device_.newRenderPipelineState(present_pipeline, error);
  }
}

} // namespace dxmt