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
FenceLocalityCheck::ensureLocality(FenceSet strong_fences, EncoderId id) {
  FenceSet all_fences(strong_fences);
  all_fences.mergeWithLaneMaskOff(WEAK_FENCE_MASK[id], strong_fences.laneMask());
  return all_fences;
}

} // namespace dxmt