
#include <algorithm>
#include "Metal.hpp"
#include "dxmt_format.hpp"
#include "dxmt_presenter.hpp"
#include "util_likely.hpp"


namespace dxmt {

Presenter::Presenter(WMT::Device device, WMT::MetalLayer layer, InternalCommandLibrary &lib, float scale_factor, uint8_t sample_count) :
    device_(device),
    layer_(layer),
    lib_(lib),
    sample_count_(sample_count) {
  layer_.getProps(layer_props_);
  layer_props_.device = device;
  layer_props_.opaque = true;
  layer_props_.display_sync_enabled = false;
  layer_props_.framebuffer_only = false; // how strangely setting it true results in worse performance
  layer_props_.contents_scale = layer_props_.contents_scale * scale_factor;
}

bool
Presenter::changeLayerProperties(
    WMTPixelFormat format, WMTColorSpace colorspace, double width, double height, uint8_t sample_count
) {
  bool should_invalidated = colorspace_ != colorspace;
  should_invalidated |= source_format_ != format;
  source_format_ = format;
  should_invalidated |= sample_count_ != sample_count;
  sample_count_ = sample_count;
  layer_props_.pixel_format = Forget_sRGB(format);
  layer_props_.drawable_height = height;
  layer_props_.drawable_width = width;
  colorspace_ = colorspace;
  if (should_invalidated)
    pso_valid.clear(); // defer changes
  else
    layer_.setProps(layer_props_);
  return should_invalidated;
}

bool
Presenter::changeLayerColorSpace(WMTColorSpace colorspace) {
  bool should_invalidated = colorspace_ != colorspace;
  colorspace_ = colorspace;
  if (should_invalidated)
    pso_valid.clear();
  return should_invalidated;
}

void
Presenter::changeHDRMetadata(const WMTHDRMetadata *metadata) {
  if (metadata) {
    has_hdr_metadata_ = true;
    hdr_metadata_ = *metadata;
    pso_valid.clear();
  } else {
    if (has_hdr_metadata_) {
      has_hdr_metadata_ = false;
      pso_valid.clear();
    }
  }
}

Presenter::PresentState
Presenter::synchronizeLayerProperties() {
  uint64_t display_setting_version = 0;

  WMTQueryDisplaySettingForLayer(
      layer_.handle, &display_setting_version, &display_colorspace_, &display_hdr_metadata_, &display_edr_value_
  );

  if (display_setting_version != display_setting_version_) {
    display_setting_version_ = display_setting_version;
    pso_valid.clear();
  }

  auto final_colorspace = display_setting_version_ > 0 ? display_colorspace_ : colorspace_;
  auto is_hdr = WMT_COLORSPACE_IS_HDR(final_colorspace);
  auto hdr_metadata = display_setting_version_ > 0 ? &display_hdr_metadata_
                      : has_hdr_metadata_          ? &hdr_metadata_
                                                   : nullptr;
  if (unlikely(!pso_valid.test_and_set())) {
    frame_presented_.wait(frame_requested_);
    buildRenderPipelineState(final_colorspace == WMTColorSpaceHDR_PQ, is_hdr && hdr_metadata != nullptr, sample_count_ > 1);
    layer_.setProps(layer_props_);
    layer_.setColorSpace(final_colorspace);
  }

  DXMTPresentMetadata metadata;

  metadata.edr_scale = 1.0;
  metadata.max_content_luminance = 10000;
  metadata.max_display_luminance = display_edr_value_.maximum_potential_edr_color_component_value * 100;

  if (is_hdr) {
    metadata.edr_scale = display_edr_value_.maximum_edr_color_component_value /
                display_edr_value_.maximum_potential_edr_color_component_value;

    if (hdr_metadata) {
      metadata.max_content_luminance = std::max<uint32_t>(
          {(uint32_t)metadata.max_display_luminance, hdr_metadata->max_content_light_level,
           hdr_metadata->max_mastering_luminance, hdr_metadata->max_frame_average_light_level}
      );
    }
  }

  if (final_colorspace == WMTColorSpaceHDR_scRGB)
    metadata.edr_scale *= 0.8;

  return {metadata, ++frame_requested_, this};
}

WMT::MetalDrawable
Presenter::encodeCommands(
    WMT::CommandBuffer cmdbuf, WMT::Fence fence, WMT::Texture backbuffer, DXMTPresentMetadata metadata
) {
  auto drawable = layer_.nextDrawable();

  WMTRenderPassInfo info;
  WMT::InitializeRenderPassInfo(info);
  info.colors[0].load_action = WMTLoadActionDontCare;
  info.colors[0].store_action = WMTStoreActionStore;
  info.colors[0].texture = drawable.texture();
  auto encoder = cmdbuf.renderCommandEncoder(info);
  if (fence)
    encoder.waitForFence(fence, WMTRenderStageFragment);
  encoder.setFragmentTexture(backbuffer, 0);

  double width = layer_props_.drawable_width;
  double height = layer_props_.drawable_height;

  encoder.setFragmentBytes(&metadata, sizeof(metadata), 0);
  if (backbuffer.width() == (uint64_t)width && backbuffer.height() == (uint64_t)height) {
    encoder.setRenderPipelineState(present_blit_);
  } else {
    encoder.setRenderPipelineState(present_scale_);
  }
  encoder.setViewport({0, 0, width, height, 0, 1});
  encoder.drawPrimitives(WMTPrimitiveTypeTriangle, 0, 3);

  encoder.endEncoding();

  return drawable;
}

constexpr uint32_t kPresentFCIndex_BackbufferSizeMatched = 0x100;
constexpr uint32_t kPresentFCIndex_HDRPQ = 0x101;
constexpr uint32_t kPresentFCIndex_WithHDRMetadata = 0x102;
constexpr uint32_t kPresentFCIndex_BackbufferIsSRGB = 0x103;
constexpr uint32_t kPresentFCIndex_BackbufferIsMS = 0x104;

void
Presenter::buildRenderPipelineState(bool is_pq, bool with_hdr_metadata, bool is_ms) {
  auto pool = WMT::MakeAutoreleasePool();

  auto library = lib_.getLibrary();

  uint32_t true_data = true, false_data = false;
  WMTFunctionConstant constants[5];
  constants[0].data.set(&true_data);
  constants[0].type = WMTDataTypeBool;
  constants[0].index = kPresentFCIndex_BackbufferSizeMatched;
  constants[1].data.set(is_pq ? &true_data : &false_data);
  constants[1].type = WMTDataTypeBool;
  constants[1].index = kPresentFCIndex_HDRPQ;
  constants[2].data.set(with_hdr_metadata ? &true_data : &false_data);
  constants[2].type = WMTDataTypeBool;
  constants[2].index = kPresentFCIndex_WithHDRMetadata;
  constants[3].data.set(Is_sRGBVariant(source_format_) ? &true_data : &false_data);
  constants[3].type = WMTDataTypeBool;
  constants[3].index = kPresentFCIndex_BackbufferIsSRGB;
  constants[4].data.set(is_ms ? &true_data : &false_data);
  constants[4].type = WMTDataTypeBool;
  constants[4].index = kPresentFCIndex_BackbufferIsMS;

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