#pragma once
#include <bitset>
#include <cstdint>
#include <limits>
#include <array>

namespace dxmt {

enum DXMT_ENCODER_RESOURCE_ACESS {
  DXMT_ENCODER_RESOURCE_ACESS_READ = 1 << 0,
  DXMT_ENCODER_RESOURCE_ACESS_WRITE = 1 << 1,
};

using FenceId = uint16_t;

constexpr unsigned kLog2FenceCount = 12;

static_assert(kLog2FenceCount < 16, "FenceId is uint16_t");

constexpr unsigned kFenceCount = 1 << kLog2FenceCount;

/* Number of fence avaliable (no contention) */
constexpr unsigned kFenceCountPerCommandBuffer = kFenceCount / 2;

constexpr unsigned kMaximumFencingDistance = kFenceCount - kFenceCountPerCommandBuffer;

constexpr FenceId kFenceIdMask = (1 << kLog2FenceCount) - 1;

constexpr FenceId kNotAFenceId = std::numeric_limits<FenceId>::max();

constexpr FenceId kInvalidFenceMask = ~kFenceIdMask;

constexpr bool
FenceIsValid(FenceId id) {
  return (id & kInvalidFenceMask) == 0;
};

class GenericAccessTracker {

public:
  FenceId access(uint64_t current_encoder_id, bool read_only);

private:
  uint64_t any_write_should_wait_for_;
  uint64_t any_read_should_wait_for_;
};

class FenceAliasMap {

public:
  void unalias(FenceId fence_id);

  void alias(FenceId alias_from, FenceId alias_to);

  FenceId get(FenceId fence_id) const;

private:
  std::array<FenceId, kFenceCount> alias_map_;
};

class FenceSet {

public:
  void
  add(FenceId id) {
    set_.set(id);
  }

  void
  remove(FenceId id) {
    set_.reset(id);
  }

  void
  merge(const FenceSet &set) {
    set_ |= set.set_;
  }

  bool
  contains(FenceId id) {
    return set_.test(id);
  }

  template <typename Visitor>
  void
  forEach(const FenceAliasMap &alias_map, Visitor &&visitor) {
    // TODO: more efficient iteration
    for (unsigned i = 0; i < kFenceCount; i++) {
      if (set_.test(i)) {
        visitor(alias_map.get(i));
      }
    }
  }

private:
  std::bitset<kFenceCount> set_{};
};

} // namespace dxmt
