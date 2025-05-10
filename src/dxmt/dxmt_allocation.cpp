#include "dxmt_allocation.hpp"

namespace dxmt {

void
Allocation::incRef() {
  refcount_.fetch_add(1u, std::memory_order_acquire);
};

void
Allocation::decRef() {
  if (refcount_.fetch_sub(1u, std::memory_order_release) == 1u)
    delete this;
};

}