#include "dxmt_deptrack.hpp"
#include <cassert>

namespace dxmt {

void
GenericAccessTracker::read(EncoderId id, FenceSet &wait_fences) {
  if (read_.add(id)) {
    write_.enumerate(id, [&](EncoderId id) { wait_fences.set(id); });
  }
}

void
GenericAccessTracker::write(EncoderId id, FenceSet &wait_fences) {
  if (write_.check(id)) {
    write_.enumerate(id, [&](EncoderId id) { wait_fences.set(id); });
    read_.enumerate(id, [&](EncoderId id) { wait_fences.set(id); });
    read_.clear();
    write_.clearAndAdd(id);
  }
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