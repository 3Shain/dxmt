#pragma once
#include <cstdint>
#include <array>
#include <cassert>
#include <cstring>

namespace dxmt {

enum DXMT_ENCODER_RESOURCE_ACESS {
  DXMT_ENCODER_RESOURCE_ACESS_READ = 1 << 0,
  DXMT_ENCODER_RESOURCE_ACESS_WRITE = 1 << 1,
};

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

  FenceSet &
  intersect(const FenceSet &set) {
    for (int i = 0; i < kParity; i++) {
      storage_[i] &= set.storage_[i];
    }
    return *this;
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

  template <typename Fn>
  void
  forEach(Fn &&fn) {
    static_assert(kParity == 4);
    for (int P = 0; P < kParity; P++) {
      auto lanes = storage_[P];
      if (lanes == 0)
        continue;
      for (int L = 0; L < kLane; L++) {
        if (lanes & (1ull << L)) {
          fn(P * kLane + L);
        }
      }
    }

    // for (int i = 0; i < kParityLane; i++) {
    //   if (test(i)) {
    //     fn(i);
    //   }
    // }
  }

  template <typename Fn, typename FnPrior>
  void
  forEach(const FenceSet &prior, FnPrior &&fnPrior, Fn &&fn) {
    static_assert(kParity == 4);
    for (int P = 0; P < kParity; P++) {
      auto lanes = storage_[P];
      auto lanes_prior = prior.storage_[P];
      if ((lanes | lanes_prior) == 0)
        continue;
      for (int L = 0; L < kLane; L++) {
        if (lanes_prior & (1ull << L)) {
          fnPrior(P * kLane + L);
        } else if (lanes & (1ull << L)) {
          fn(P * kLane + L);
        }
      }
    }

    // for (int i = 0; i < kParityLane; i++) {
    //   if (prior.test(i)) {
    //     fnPrior(i);
    //   } else if (test(i)) {
    //     fn(i);
    //   }
    // }
  }

private:
  LaneStorage storage_[kParity];
};

class TrackingSet {
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
    bumpCursor();
    storage_[cursor] = id;
    return true;
  };

  bool
  check(EncoderId id) {
    return storage_[cursor] != id;
  }

  void
  clear() {
    storage_[cursor] = 0;
  };

  void
  clearAndAdd(EncoderId id) {
    storage_[cursor] = 0;
    bumpCursor();
    storage_[cursor] = id;
  };

  template <typename Fn>
  void
  enumerate(EncoderId id, Fn &&fn) {
    for (int i = 0; i < kLane; i++) {
      auto c = storage_[(cursor + kLane - i) & kLaneMask];
      if (c == id)
        continue;
      if (c > (id - kLane)) {
        fn(c);
        continue;
      }
      break;
    }
  }

private:
  void
  bumpCursor() {
    cursor++;
    cursor &= kLaneMask;
  }

  EncoderId storage_[kLane];
  uint32_t cursor;
};

class GenericAccessTracker {
public:
  void read(EncoderId id, FenceSet &);
  void write(EncoderId id, FenceSet &);

private:
  TrackingSet read_;
  TrackingSet write_;
};

class FenceLocalityCheck {
public:
  FenceSet collectAndSimplifyWaits(FenceSet strong_fences, EncoderId id, bool implicit_pre_raster_wait = false);

private:
  std::array<FenceSet, kParityLane> summary_;
};

} // namespace dxmt
