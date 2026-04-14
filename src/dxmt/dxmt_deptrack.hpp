#pragma once
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include "util_bit.hpp"

namespace dxmt {

namespace ResourceAccess {
constexpr int Read = 1 << 0;
constexpr int Write = 1 << 1;
constexpr int ReadWrite = Read | Write;
constexpr int UAV = 1 << 2;
constexpr int All = ReadWrite | UAV;
}; // namespace ResourceAccess

constexpr auto kLog2Lane = 6ull;
constexpr auto kLane = 1 << kLog2Lane;
constexpr auto kLaneMask = kLane - 1;
constexpr auto kAllLaneMask = ~0ull >> (64 /* uint64_t */ - kLane);
constexpr auto kParity = 4; // can also use 3, although power of 2 is nice
constexpr auto kParityLane = kParity * kLane;

static_assert(kLog2Lane <= 6);
static_assert(kLane > 1);

using LaneStorage = uint64_t;
using EncoderId = uint64_t;

constexpr auto
PARITY(EncoderId id) {
  return (id >> kLog2Lane) % kParity;
}

constexpr auto
LANE(EncoderId id) {
  return id & kLaneMask;
}

class FenceSet {
public:
  constexpr FenceSet() {
    for (int i = 0; i < kParity; i++) {
      storage_[i] = 0;
    }
  }

  constexpr FenceSet(EncoderId id) {
    for (int i = 0; i < kParity; i++) {
      storage_[i] = 0;
    }
    set(id);
  }

  FenceSet(const FenceSet &copy) {
    memcpy(&storage_, &copy.storage_, sizeof(storage_));
  }

  FenceSet &
  operator=(const dxmt::FenceSet &copy) {
    memcpy(&storage_, &copy.storage_, sizeof(storage_));
    return *this;
  }

  ~FenceSet() = default;

  constexpr void
  set(EncoderId id) {
    storage_[PARITY(id)] |= (1ull << LANE(id));
  }

  constexpr void
  unset(EncoderId id) {
    storage_[PARITY(id)] &= (kAllLaneMask & ~(1ull << LANE(id)));
  }

  constexpr void
  fillGenerationBefore(int parity, int lane) {
    const int idx = (parity + kParity + (kParity - 1)) * kLane + lane;
    for (int offset = 0; offset < kLane; ++offset) {
      set(idx - offset);
    }
  }

  constexpr bool
  test(EncoderId id) const {
    return storage_[PARITY(id)] & (1ull << LANE(id));
  }

  constexpr bool
  testAndSet(EncoderId id) {
    auto P = PARITY(id);
    auto LM = 1ull << LANE(id);
    if (storage_[P] & LM)
      return true;
    storage_[P] |= LM;
    return false;
  }

  constexpr bool
  intersectedWith(const FenceSet &set) const {
    for (int i = 0; i < kParity; i++) {
      if (storage_[i] & set.storage_[i])
        return true;
    }
    return false;
  }

  constexpr bool
  contains(const FenceSet &set) const {
    for (int i = 0; i < kParity; i++) {
      if ((storage_[i] & set.storage_[i]) != set.storage_[i])
        return false;
    }
    return true;
  }

  FenceSet &
  merge(const FenceSet &set) {
    for (int i = 0; i < kParity; i++) {
      storage_[i] |= set.storage_[i];
    }
    return *this;
  }

  FenceSet
  unionOf(const FenceSet &set) const {
    FenceSet ret{};
    for (int i = 0; i < kParity; i++) {
      ret.storage_[i] = storage_[i] | set.storage_[i];
    }
    return ret;
  }

  FenceSet &
  subtract(const FenceSet &set) {
    for (int i = 0; i < kParity; i++) {
      storage_[i] &= (kAllLaneMask & ~set.storage_[i]);
    }
    return *this;
  }

  FenceSet &
  mergeWithLaneMaskOff(const FenceSet &set, const LaneStorage &mask) {
    for (int i = 0; i < kParity; i++) {
      storage_[i] |= (set.storage_[i] & (kAllLaneMask & ~mask));
    }
    return *this;
  }

  LaneStorage
  laneMask() const {
    LaneStorage ret = 0;
    for (int i = 0; i < kParity; i++) {
      ret |= storage_[i];
    }
    return ret;
  }

  bool
  empty() const {
    return laneMask() == 0;
  }

  template <typename Fn>
  void
  forEach(Fn &&fn) {
    for (int P = 0; P < kParity; P++) {
      auto lanes = storage_[P];
      while (lanes) {
        auto lane = bit::tzcnt(lanes);
        fn(P * kLane + lane);
        lanes &= ~(1ull << lane);
      }
    }
  }

  template <typename Fn, typename FnPrior>
  void
  forEach(const FenceSet &prior, FnPrior &&fnPrior, Fn &&fn) {
    for (int P = 0; P < kParity; P++) {
      auto lanes = storage_[P];
      auto lanes_prior = prior.storage_[P];
      while (auto lanes_combine = lanes | lanes_prior) {
        auto lane = bit::tzcnt(lanes_combine);
        if (lanes_prior & (1ull << lane))
          fnPrior(P * kLane + lane);
        else
          fn(P * kLane + lane);
        lanes &= ~(1ull << lane);
        lanes_prior &= ~(1ull << lane);
      }
    }
  }

private:
  LaneStorage storage_[kParity];
};

template <size_t Sz = kLane, size_t Forward = 1> class TrackingSet {
public:
  TrackingSet() {
    cursor = 0;
    clear();
  };

  bool
  add(EncoderId id) {
    assert(storage_[cursor] <= id);
    if (storage_[cursor] == id)
      return false;
    {
      cursor++;
      cursor = cursor % Sz;
    }
    storage_[cursor] = id;
    return true;
  };

  bool
  isLastAccess(EncoderId id) {
    return storage_[cursor] == id;
  }

  void
  clear() {
    storage_[cursor] = 0;
  };

  template <typename Fn>
  size_t
  enumerate(EncoderId id_before, Fn &&fn) {
    size_t count = 0;
    assert(id_before > Sz);
    for (size_t i = 0; i < Sz; i++) {
      auto c = storage_[(cursor + Sz - i) % Sz];
      if (c >= id_before) {
        assert(c - id_before <= Forward);
        continue;
      }
      if (c > (id_before - Sz)) {
        fn(c);
        count++;
        continue;
      }
      break;
    }
    return count;
  }

private:
  EncoderId storage_[Sz + Forward];
  uint32_t cursor;
};

constexpr auto kBarrierTypeRW = 1 << 0;
constexpr auto kBarrierTypeWaW = 1 << 1;

struct EncoderBarrierState {
  uint64_t barrierSet                       : 2 = 0;
  uint64_t barrierPreRasterSet              : 2 = 0;
  uint64_t barrierFragmentAfterPreRasterSet : 2 = 0;
  uint64_t barrierPreRasterAfterFragmentSet : 2 = 0;
  uint64_t reserved                         : 56;
};

class GenericAccessTracker {
public:
  void accessShared(EncoderId id, FenceSet &wait_fences, EncoderBarrierState &barrier_state);
  void accessExclusive(EncoderId id, FenceSet &wait_fences, EncoderBarrierState &barrier_state, bool uav);

  void accessSharedPreRaster(EncoderId id, FenceSet &wait_fences, EncoderBarrierState &barrier_state);
  void accessExclusivePreRaster(EncoderId id, FenceSet &wait_fences, EncoderBarrierState &barrier_state, bool uav);
  void accessSharedFragment(EncoderId id, FenceSet &wait_fences, EncoderBarrierState &barrier_state);
  void accessExclusiveFragment(EncoderId id, FenceSet &wait_fences, EncoderBarrierState &barrier_state, bool uav);

private:
  /**
   * Previous shared access
   */
  TrackingSet<> shared_;
  /**
   * Last exclusive access
   */
  EncoderId exclusive_{};
  uint64_t isShared               : 1 = 0;
  uint64_t isSharedPreRaster      : 1 = 0;
  uint64_t lastWriteFromPreRaster : 1 = 0;
};

class FenceLocalityCheck {
public:
  FenceSet collectAndSimplifyWaits(FenceSet strong_fences, EncoderId id, bool implicit_pre_raster_wait = false);

private:
  std::array<FenceSet, kParityLane> summary_;
};

} // namespace dxmt
