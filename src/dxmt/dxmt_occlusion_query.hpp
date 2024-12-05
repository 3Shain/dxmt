#pragma once

#include "Metal/MTLBuffer.hpp"
#include "objc_pointer.hpp"
#include "rc/util_rc_ptr.hpp"
#include <atomic>
#include <cassert>
#include <cstdint>
#include <vector>

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

class VisibilityResultQuery {
public:
  void
  incRef() {
    refcount_.fetch_add(1u, std::memory_order_acquire);
  }
  void
  decRef() {
    if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
      delete this;
  }

  void
  begin(uint64_t seqId, unsigned offset) {
    accumulated_value_ = 0;
    seq_id_begin = seqId;
    occlusion_counter_begin = offset;
    seq_id_end = ~0uLL;
    occlusion_counter_end = ~0uLL;
  }

  void
  end(uint64_t seqId, unsigned offset) {
    seq_id_end = seqId;
    occlusion_counter_end = offset;
  }

  uint64_t queryEndAt() {
    return seq_id_end;
  };

  void
  issue(uint64_t seqId, uint64_t const *readbackBuffer, unsigned numResults) {
    assert(seqId >= seq_id_begin);
    assert(seqId <= seq_id_end);
    uint64_t const *start = seqId == seq_id_begin ? readbackBuffer + occlusion_counter_begin : readbackBuffer;
    uint64_t const *end = seqId == seq_id_end ? readbackBuffer + occlusion_counter_end : readbackBuffer + numResults;
    assert(start <= end);
    while (start != end) {
      accumulated_value_ += *start;
      start++;
    }
    seq_id_issued = seqId;
  }

  bool
  getValue(uint64_t *value) {
    if (seq_id_end <= seq_id_issued) {
      *value = accumulated_value_;
      return true;
    }
    return false;
  }

  void
  reset() {
    accumulated_value_ = 0;
    seq_id_begin = ~0uLL;
    occlusion_counter_begin = ~0uLL;
    seq_id_end = ~0uLL;
    occlusion_counter_end = ~0uLL;
    seq_id_issued = 0;
  };

private:
  uint64_t accumulated_value_ = 0;
  uint64_t seq_id_begin = ~0uLL;
  uint64_t occlusion_counter_begin = ~0uLL;
  uint64_t seq_id_end = ~0uLL;
  uint64_t occlusion_counter_end = ~0uLL;
  uint64_t seq_id_issued = 0;
  std::atomic<uint32_t> refcount_ = {0u};
};

class VisibilityResultReadback {
public:
  VisibilityResultReadback(
      uint64_t seq_id, uint64_t num_results, std::vector<Rc<VisibilityResultQuery>> &queries,
      Obj<MTL::Buffer> &&visibility_result_heap
  ) :
      seq_id(seq_id),
      num_results(num_results),
      queries(queries),
      visibility_result_heap(std::move(visibility_result_heap)) {}
  ~VisibilityResultReadback() {
    for (auto query : queries) {
      query->issue(seq_id, (uint64_t *)visibility_result_heap->contents(), num_results);
    }
  }

  VisibilityResultReadback(const VisibilityResultReadback &) = delete;
  VisibilityResultReadback(VisibilityResultReadback &&) = default;

  uint64_t seq_id;
  uint64_t num_results;
  std::vector<Rc<VisibilityResultQuery>> queries;
  Obj<MTL::Buffer> visibility_result_heap;
};

} // namespace dxmt
