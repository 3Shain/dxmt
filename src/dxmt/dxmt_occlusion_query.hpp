#pragma once

#include <cassert>
#include <cstdint>

namespace dxmt {
class VisibilityResultOffsetBumpState {
public:
  void
  beginEncoder() {
    assert(!within_encoder);
    assert(!current_data_is_dirty);
    within_encoder = true;
    should_update_offset = true;
  }

  uint64_t
  getNextWriteOffset(bool has_active_occlusion_queries) {
    if (has_active_occlusion_queries) {
      if (!current_data_is_dirty) {
        current_data_is_dirty = true;
      }
    }
    if (should_update_offset) {
      should_update_offset = false;
      return next_offset;
    }
    return ~0uLL;
  };

  uint64_t
  getNextReadOffset() {
    if (within_encoder) {
      if (current_data_is_dirty) {
        current_data_is_dirty = false;
        should_update_offset = true;
        return ++next_offset;
      }
    } else {
      assert(!current_data_is_dirty);
    }
    return next_offset;
  }

  void
  endEncoder() {
    assert(within_encoder);
    within_encoder = false;
    if (current_data_is_dirty) {
      next_offset++;
      current_data_is_dirty = false;
    }
  }

  uint64_t
  reset() {
    assert(!within_encoder && "encoder still active");
    assert(!current_data_is_dirty && "encoder still active");
    auto ret = next_offset;
    next_offset = 0;
    return ret;
  }

  uint64_t
  preserveCount(uint64_t count) {
    assert(!within_encoder && "encoder still active");
    assert(!current_data_is_dirty && "encoder still active");
    auto ret = next_offset;
    next_offset += count;
    return ret;
  }

private:
  bool within_encoder = false;
  bool current_data_is_dirty = false;
  bool should_update_offset = false;
  uint64_t next_offset = 0;
};

} // namespace dxmt
