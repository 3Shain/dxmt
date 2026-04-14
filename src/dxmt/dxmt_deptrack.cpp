#include "dxmt_deptrack.hpp"
#include <cassert>

namespace dxmt {

void
GenericAccessTracker::accessShared(EncoderId id, FenceSet &wait_fences, EncoderBarrierState &barrier_state) {
  if (exclusive_ == id) {
    if (isShared)
      return;
    isShared = 1;
    barrier_state.barrierSet |= kBarrierTypeRW;
    return;
  }
  assert(exclusive_ < id);
  if (shared_.isLastAccess(id))
    return;
  shared_.add(id);
  if (id - exclusive_ < kLane) {
    wait_fences.set(exclusive_);
  }
}

void
GenericAccessTracker::accessExclusive(
    EncoderId id, FenceSet &wait_fences, EncoderBarrierState &barrier_state, bool uav
) {
  isShared = 0;
  if (exclusive_ == id) {
    barrier_state.barrierSet |= (1 << uav);
    return;
  }
  if (shared_.isLastAccess(id)) {
    barrier_state.barrierSet |= kBarrierTypeRW;
  }
  shared_.enumerate(id, [&](EncoderId id) { wait_fences.set(id); });
  shared_.clear();
  if (id - exclusive_ < kLane)
    wait_fences.set(exclusive_);
  exclusive_ = id;
}

void
GenericAccessTracker::accessSharedPreRaster(EncoderId id, FenceSet &wait_fences, EncoderBarrierState &barrier_state) {
  if (exclusive_ == id + 1) {
    if (isSharedPreRaster)
      return;
    isSharedPreRaster = 1;
    if (lastWriteFromPreRaster)
      barrier_state.barrierPreRasterSet |= kBarrierTypeRW;
    else
      barrier_state.barrierPreRasterAfterFragmentSet |= kBarrierTypeRW;
    return;
  }
  if (exclusive_ == id) {
    if (isSharedPreRaster)
      return;
    isSharedPreRaster = 1;
    barrier_state.barrierPreRasterSet |= kBarrierTypeRW;
    return;
  }
  assert(exclusive_ < id);
  if (shared_.isLastAccess(id + 1)) {
    if (isSharedPreRaster)
      return;
    isSharedPreRaster = 1;
    if (id - exclusive_ < kLane)
      wait_fences.set(exclusive_);
    return;
  } else if (shared_.isLastAccess(id)) {
    // NOP
    return;
  }
  shared_.add(id);
  if (id - exclusive_ < kLane)
    wait_fences.set(exclusive_);
  isSharedPreRaster = 1;
  isShared = 0;
}

void
GenericAccessTracker::accessExclusivePreRaster(
    EncoderId id, FenceSet &wait_fences, EncoderBarrierState &barrier_state, bool uav
) {
  auto last_exclusive = exclusive_;
  exclusive_ = id;

  if (last_exclusive == id + 1) {
    auto last = lastWriteFromPreRaster;
    lastWriteFromPreRaster = 1;
    if (!isShared && !isSharedPreRaster) {
      if (last)
        barrier_state.barrierPreRasterSet |= (1 << uav);
      else
        barrier_state.barrierPreRasterAfterFragmentSet |= (1 << uav);
      return;
    }
    if (isSharedPreRaster) {
      isSharedPreRaster = 0;
      barrier_state.barrierPreRasterSet |= kBarrierTypeRW;
    }
    if (isShared) {
      isShared = 0;
      barrier_state.barrierPreRasterAfterFragmentSet |= kBarrierTypeRW;
    }
    return;
  }
  if (last_exclusive == id) {
    if (!isShared && !isSharedPreRaster) {
      barrier_state.barrierPreRasterSet |= (1 << uav);
    }
    if (isSharedPreRaster) {
      isSharedPreRaster = 0;
      barrier_state.barrierPreRasterSet |= kBarrierTypeRW;
    }
    if (isShared) {
      isShared = 0;
      barrier_state.barrierPreRasterAfterFragmentSet |= kBarrierTypeRW;
    }
    return;
  }
  shared_.enumerate(id, [&](EncoderId id) { wait_fences.set(id); });
  if (last_exclusive)
    wait_fences.set(last_exclusive);

  if (shared_.isLastAccess(id + 1) || shared_.isLastAccess(id)) {
    if (isSharedPreRaster) {
      isSharedPreRaster = 0;
      barrier_state.barrierPreRasterSet |= kBarrierTypeRW;
    }
    if (isShared) {
      isShared = 0;
      barrier_state.barrierPreRasterAfterFragmentSet |= kBarrierTypeRW;
    }
    shared_.clear();
    return;
  }
  shared_.clear();
  isShared = 0;
  isSharedPreRaster = 0;
}

void
GenericAccessTracker::accessSharedFragment(EncoderId id, FenceSet &wait_fences, EncoderBarrierState &barrier_state) {
  if (exclusive_ == id) {
    if (isShared)
      return;
    isShared = 1;
    if (isSharedPreRaster)
      return; // IMPLICIT BARRIER
    if (lastWriteFromPreRaster)
      barrier_state.barrierFragmentAfterPreRasterSet |= kBarrierTypeRW;
    else
      barrier_state.barrierSet |= kBarrierTypeRW;
    return;
  }
  if (exclusive_ == id - 1) {
    if (isShared)
      return;
    isShared = 1;
    if (isSharedPreRaster)
      return; // IMPLICIT BARRIER
    barrier_state.barrierFragmentAfterPreRasterSet |= kBarrierTypeRW;
    return;
  }
  assert(exclusive_ < id - 1);
  if (shared_.isLastAccess(id))
    return;
  bool isVertexLastAccess = shared_.isLastAccess(id - 1);
  if (id - exclusive_ < kLane)
    wait_fences.set(exclusive_);
  shared_.add(id);
  isShared = 1;
  if (!isVertexLastAccess)
    isSharedPreRaster = 0;
}

void
GenericAccessTracker::accessExclusiveFragment(
    EncoderId id, FenceSet &wait_fences, EncoderBarrierState &barrier_state, bool uav
) {
  auto last_exclusive = exclusive_;
  exclusive_ = id;
  if (last_exclusive == id) {
    auto last = lastWriteFromPreRaster;
    lastWriteFromPreRaster = 0;
    if (!isShared && !isSharedPreRaster) {
      if (last)
        barrier_state.barrierFragmentAfterPreRasterSet |= (1 << uav);
      else
        barrier_state.barrierSet |= (1 << uav);
      return;
    }
    if (isSharedPreRaster) {
      isSharedPreRaster = 0;
      barrier_state.barrierFragmentAfterPreRasterSet |= kBarrierTypeRW;
    }
    if (isShared) {
      isShared = 0;
      barrier_state.barrierSet |= kBarrierTypeRW;
    }
    return;
  }
  lastWriteFromPreRaster = 0;
  if (last_exclusive == id - 1) {
    if (!isShared && !isSharedPreRaster) {
      barrier_state.barrierFragmentAfterPreRasterSet |= (1 << uav);
      return;
    }
    if (isSharedPreRaster) {
      isSharedPreRaster = 0;
      barrier_state.barrierFragmentAfterPreRasterSet |= kBarrierTypeRW;
    }
    if (isShared) {
      isShared = 0;
      barrier_state.barrierSet |= kBarrierTypeRW;
    }
    return;
  }
  shared_.enumerate(id, [&](EncoderId id) { wait_fences.set(id); });
  if (last_exclusive)
    wait_fences.set(last_exclusive);
  if (shared_.isLastAccess(id) || shared_.isLastAccess(id - 1)) {
    if (isSharedPreRaster) {
      isSharedPreRaster = 0;
      barrier_state.barrierFragmentAfterPreRasterSet |= kBarrierTypeRW;
    }
    if (isShared) {
      isShared = 0;
      barrier_state.barrierSet |= kBarrierTypeRW;
    }
    shared_.clear();
    return;
  }
  shared_.clear();
  isShared = 0;
  isSharedPreRaster = 0;
}

class WeakFenceMaskLTO {
public:
  constexpr WeakFenceMaskLTO() {
    int i = 0;
    for (int p = 0; p < kParity; ++p) {
      for (int l = 0; l < kLane; ++l) {
        weak_fences_lto[i++].fillGenerationBefore(p, l);
      }
    }
  }

  const FenceSet &
  operator[](EncoderId i) const {
    return weak_fences_lto[i % kParityLane];
  }

private:
  FenceSet weak_fences_lto[kParityLane];
};

constexpr auto WEAK_FENCE_MASK = WeakFenceMaskLTO();

FenceSet
FenceLocalityCheck::collectAndSimplifyWaits(FenceSet strong_fences, EncoderId id, bool implicit_pre_raster_wait) {
  if (implicit_pre_raster_wait)
    strong_fences.set(id - 1);

  FenceSet full_fences(strong_fences);
  full_fences.mergeWithLaneMaskOff(WEAK_FENCE_MASK[id], strong_fences.laneMask());

  FenceSet minimal_fences;
  FenceSet accessible_fences;

  constexpr auto start_offset = kParityLane == 1 ? 0 : 1;

  for (auto offset = start_offset; offset < kParityLane; offset++) {
    EncoderId prev_encoder_id = id - offset;

    if (full_fences.test(prev_encoder_id) && !accessible_fences.testAndSet(prev_encoder_id))
      minimal_fences.set(prev_encoder_id);
    if (accessible_fences.test(prev_encoder_id))
      accessible_fences.merge(summary_[prev_encoder_id % kParityLane]);
    if (accessible_fences.contains(full_fences))
      break;
  }

  summary_[id % kParityLane] = full_fences;

  if (implicit_pre_raster_wait)
    minimal_fences.unset(id - 1);

  return minimal_fences;
}

} // namespace dxmt