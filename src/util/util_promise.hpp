
#include "objc-wrapper/dispatch.h"
#include <utility>

namespace dxmt {

template <typename T> class MyLeakyPromise {
public:
  MyLeakyPromise() : fulfilled(false) {
    semaphore = dispatch_semaphore_create(0);
  };

  T get() {
    if (fulfilled) {
      return store;
    }
    dispatch_semaphore_wait(semaphore, (~0ull));
    return store;
  };

  void set_value(const T &t) {
    fulfilled = true;
    store = std::move(t);
  }

private:
  T store;
  bool fulfilled;
  dispatch_semaphore_t semaphore;
};
} // namespace dxmt