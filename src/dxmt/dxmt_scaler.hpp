#pragma once

#include "Metal.hpp"
#include <atomic>

namespace dxmt {

class SpatialScaler {
public:
  void incRef();
  void decRef();

  SpatialScaler(WMT::Device device, const WMTFXSpatialScalerInfo &info);

  WMT::FXSpatialScaler scaler() {
    return scaler_;
  }

  WMT::Fence fence() {
    return fence_;
  }

private:
  WMT::Reference<WMT::FXSpatialScaler> scaler_;
  WMT::Reference<WMT::Fence> fence_;
  std::atomic<uint32_t> refcount_;
};

class TemporalScaler {
public:
  void incRef();
  void decRef();

  TemporalScaler(WMT::Device device, const WMTFXTemporalScalerInfo &info);

  WMT::FXTemporalScaler scaler() {
    return scaler_;
  }

  WMT::Fence fence() {
    return fence_;
  }

private:
  WMT::Reference<WMT::FXTemporalScaler> scaler_;
  WMT::Reference<WMT::Fence> fence_;
  std::atomic<uint32_t> refcount_;
};

} // namespace dxmt