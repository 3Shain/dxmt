#include "dxmt_deptrack.hpp"
#include <cstdint>
#include <cstring>
#include <utility>
#include <cassert>

namespace dxmt {

FenceId
GenericAccessTracker::read(EncoderId current_encoder_id) {
  assert(any_read_should_wait_for_ <= current_encoder_id);
  FenceId target_fence = kNotAFenceId;
  if (any_read_should_wait_for_ < current_encoder_id) {
    if (current_encoder_id - any_read_should_wait_for_ < kMaximumFencingDistance)
      target_fence = any_read_should_wait_for_ & kFenceIdMask;
  } else {
    // FIXME: otherwise a interpass read-after-write or write-after-write barrier is requried
    // WARN("potential RaW hazard");
    return kNotAFenceId;
  }
  auto forgotten_read_encoder_id = any_write_should_wait_for_.add(current_encoder_id);
  if (any_read_should_wait_for_ < forgotten_read_encoder_id) {
    target_fence = forgotten_read_encoder_id & kFenceIdMask;
  }
  return target_fence;
}

FenceSet
GenericAccessTracker::write(EncoderId current_encoder_id) {
  auto latest_access = any_write_should_wait_for_.latest();
  assert(latest_access <= current_encoder_id);
  any_read_should_wait_for_ = current_encoder_id;
  if (latest_access < current_encoder_id) {
    return any_write_should_wait_for_.reset(current_encoder_id);
  }
  // FIXME: otherwise a interpass write-after-read barrier is required
  // WARN("potential WaR/WaW hazard");
  return {};
}

EncoderId
SharedAccessHistory::add(EncoderId encoder_id) {
  EncoderId ret = 0;
  if (encoder_id <= latest())
    return ret;
  ret = std::exchange(storage_[(count_++) & kSharedAccessHistoryIndexMask], encoder_id);
  ret = (encoder_id - ret) > kMaximumFencingDistance ? 0 : ret;
  return ret;
}

FenceSet
SharedAccessHistory::reset(EncoderId current_encoder_id) {
  FenceSet ret;
  for (unsigned i = 1; i <= std::min<uint64_t>(kSharedAccessHistorySize, count_); i++) {
    auto id = storage_[(count_ - i) & kSharedAccessHistoryIndexMask];
    if (current_encoder_id - id > kMaximumFencingDistance)
      break;
    ret.add(id & kFenceIdMask);
  }
  std::memset(storage_.data(), 0, sizeof(uint64_t) * std::min<uint64_t>(kSharedAccessHistorySize, count_));
  count_ = 1;
  storage_[0] = current_encoder_id;
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