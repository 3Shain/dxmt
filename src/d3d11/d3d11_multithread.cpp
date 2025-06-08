#include "d3d11_multithread.hpp"

namespace dxmt {

void d3d11_device_mutex::lock() {
  if (!protected_)
    return;
  internal_mutex_.lock();
}
void d3d11_device_mutex::unlock() noexcept {
  if (!protected_)
    return;
  internal_mutex_.unlock();
}

bool d3d11_device_mutex::set_protected(bool Protected) {
  bool ret = protected_;
  protected_ = Protected;
  return ret;
}
bool d3d11_device_mutex::get_protected() { return protected_; }

}; // namespace dxmt