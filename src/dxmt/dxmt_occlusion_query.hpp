#pragma once

#include <cstdint>

namespace dxmt {

struct VisibilityResultObserver {
  /**
    Semantic: called when the counter value at seq is ready
    return true if it doesn't need any later values, and the publisher
    (should be CommandQueue) can remove the observer from the observer list
    Complexity: Observer should maintain their own lifetime and ensure
    it's alive if it's registered to the publisher. It can be done by add
    a ref before registered then release when observation is done.
    And the publisher should assume that if Update returns true, the pointer
    is invalid then.
   */
  virtual bool Update(uint64_t seq, uint64_t value) = 0;
};
} // namespace dxmt