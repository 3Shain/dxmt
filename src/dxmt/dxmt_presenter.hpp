#pragma once
#include "Metal.hpp"
#include "dxmt_command.hpp"
#include "rc/util_rc.hpp"
#include "winemetal.h"
#include <atomic>

namespace dxmt {
class Presenter : public RcObject {
public:
  Presenter(WMT::Device device, WMT::MetalLayer layer, InternalCommandLibrary &lib, float scale_factor);

  bool changeLayerProperties(WMTPixelFormat format, WMTColorSpace colorspace, unsigned width, unsigned height);

  bool changeLayerColorSpace(WMTColorSpace colorspace);

  WMT::MetalDrawable encodeCommands(WMT::CommandBuffer cmdbuf, WMT::Texture backbuffer);

private:
  void buildRenderPipelineState();

  WMT::Device device_;
  WMT::MetalLayer layer_;
  InternalCommandLibrary &lib_;
  WMTLayerProps layer_props_;
  WMTColorSpace colorspace_;
  WMT::Reference<WMT::RenderPipelineState> present_blit_;
  WMT::Reference<WMT::RenderPipelineState> present_scale_;
  std::atomic_flag pso_valid = 0;
};
} // namespace dxmt