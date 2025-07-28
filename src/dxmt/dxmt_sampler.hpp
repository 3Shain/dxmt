#pragma once
#include "Metal.hpp"
#include "rc/util_rc_ptr.hpp"
#include <atomic>

namespace dxmt {

class Sampler {
public:
  void incRef();
  void decRef();

  /* readonly */
  float lod_bias;
  WMT::Reference<WMT::SamplerState> sampler_state;
  WMT::Reference<WMT::SamplerState> sampler_state_cube;
  uint64_t sampler_state_handle;
  uint64_t sampler_state_cube_handle;

  static Rc<Sampler> createSampler(WMT::Device device, const WMTSamplerInfo &info, float lod_bias);

private:
  WMTSamplerInfo info_;
  std::atomic<uint32_t> refcount_ = {0u};
};

} // namespace dxmt