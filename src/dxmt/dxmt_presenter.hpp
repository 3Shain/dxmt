#pragma once
#include "Metal.hpp"
#include "dxmt_command.hpp"
#include "rc/util_rc.hpp"
#include "util_cpu_fence.hpp"
#include "winemetal.h"
#include <atomic>

namespace dxmt {

struct DXMTPresentMetadata {
  float edr_scale;
  float max_content_luminance;
  float max_display_luminance;
};

class Presenter : public RcObject {
public:
  Presenter(
      WMT::Device device, WMT::MetalLayer layer, InternalCommandLibrary &lib, float scale_factor, uint8_t sample_count
  );

  bool changeLayerProperties(
      WMTPixelFormat format, WMTColorSpace colorspace, double width, double height, uint8_t sample_count
  );

  bool changeLayerColorSpace(WMTColorSpace colorspace);

  void changeHDRMetadata(const WMTHDRMetadata *metadata);

  class PresentState {
  public:
    DXMTPresentMetadata metadata;
    uint64_t frame_id;
    Presenter *presenter;
    PresentState(DXMTPresentMetadata metadata, uint64_t frame_id, Presenter *presenter) :
        metadata(metadata),
        frame_id(frame_id),
        presenter(presenter) {}
    PresentState(const PresentState &copy) = delete;
    PresentState(PresentState &&move)  {
      metadata = move.metadata;
      frame_id = move.frame_id;
      presenter = move.presenter;
      move.presenter = nullptr;
    };
    ~PresentState() {
      if (presenter) {
        presenter->frame_presented_.signal(frame_id);
        presenter = nullptr;
      }
    };
  };

  PresentState synchronizeLayerProperties();

  WMT::MetalDrawable
  encodeCommands(WMT::CommandBuffer cmdbuf, WMT::Fence fence, WMT::Texture backbuffer, DXMTPresentMetadata metadata);

private:
  void buildRenderPipelineState(bool is_pq, bool with_hdr_metadata, bool is_ms);

  WMT::Device device_;
  WMT::MetalLayer layer_;
  InternalCommandLibrary &lib_;
  WMTLayerProps layer_props_;
  uint32_t sample_count_;
  WMTColorSpace colorspace_ = WMTColorSpaceSRGB;
  WMTHDRMetadata hdr_metadata_;
  bool has_hdr_metadata_ = false;
  WMTPixelFormat source_format_ = WMTPixelFormatInvalid;
  uint64_t display_setting_version_ = 0;
  WMTColorSpace display_colorspace_ = WMTColorSpaceSRGB;
  WMTHDRMetadata display_hdr_metadata_;
  WMTEDRValue display_edr_value_{0.0, 1.0};
  WMT::Reference<WMT::RenderPipelineState> present_blit_;
  WMT::Reference<WMT::RenderPipelineState> present_scale_;
  std::atomic_flag pso_valid = 0;
  uint64_t frame_requested_ = 0;
  CpuFence frame_presented_ = 0;
};
} // namespace dxmt