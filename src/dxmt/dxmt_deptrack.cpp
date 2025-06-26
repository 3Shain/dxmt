#include "dxmt_deptrack.hpp"
#include <cassert>

namespace dxmt {

FenceId
GenericAccessTracker::access(uint64_t current_encoder_id, bool read_only) {
  FenceId ret = kNotAFenceId;

  if (any_read_should_wait_for_ != current_encoder_id) {
    if (current_encoder_id - any_read_should_wait_for_ < kMaximumFencingDistance)
      ret = any_read_should_wait_for_ & kFenceIdMask;
  } // FIXME: otherwise a interpass read-after-write or write-after-write barrier is requried

  if (!read_only) {
    if (any_write_should_wait_for_ != current_encoder_id) {
      if (current_encoder_id - any_write_should_wait_for_ < kMaximumFencingDistance)
        ret = any_write_should_wait_for_ & kFenceIdMask;
    } // FIXME: otherwise a interpass write-after-read barrier is required
    any_read_should_wait_for_ = current_encoder_id;
  }
  any_write_should_wait_for_ = current_encoder_id;

  return ret;
}

void
FenceAliasMap::unalias(FenceId fence_id) {
  alias_map_[fence_id] = fence_id;
}

void
FenceAliasMap::alias(FenceId alias_from, FenceId alias_to) {
  alias_map_[alias_from] = alias_to;
}

FenceId
FenceAliasMap::get(FenceId fence_id) const {
  if (fence_id != alias_map_[fence_id]) {
    fence_id = alias_map_[fence_id];
    assert(fence_id == alias_map_[fence_id] && "nested alias occurred");
  }
  return fence_id;
}

} // namespace dxmt