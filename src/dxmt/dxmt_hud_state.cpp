#include "dxmt_hud_state.hpp"
#include "Foundation/NSAutoreleasePool.hpp"
#include "Foundation/NSString.hpp"
#include "objc_pointer.hpp"
#include <string>

namespace dxmt {

void
HUDState::initialize(const std::string &heading) {
  auto pool = transfer(NS::AutoreleasePool::alloc()->init());
  auto str_dxmt_version = NS::String::alloc()->init("com.github.3shain.dxmt-version", NS::ASCIIStringEncoding);
  hud_->addLabel(str_dxmt_version, NS::String::string("com.apple.hud-graph.default", NS::ASCIIStringEncoding));
  hud_->updateLabel(str_dxmt_version, NS::String::string(heading.c_str(), NS::UTF8StringEncoding));
  line_labels_.push_back(std::move(str_dxmt_version));
}

void
HUDState::begin() {
  pool_ = transfer(NS::AutoreleasePool::alloc()->init());
  current_line_ = 1;
}

void
HUDState::printLine(const char *c_str) {
  while (current_line_ >= line_labels_.size()) {
    NS::String *prev = line_labels_.back();
    line_labels_.push_back(transfer(
      NS::String::alloc()->init(("com.github.3shain.dxmt-line" + std::to_string(line_labels_.size())).c_str(), NS::ASCIIStringEncoding)
    ));
    hud_->addLabel(line_labels_.back(), prev);
  }
  hud_->updateLabel(line_labels_[current_line_], NS::String::string(c_str, NS::ASCIIStringEncoding));
  current_line_++;
}

void
HUDState::end() {
  pool_ = nullptr;
}

} // namespace dxmt