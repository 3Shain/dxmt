#pragma once

#include "Foundation/NSAutoreleasePool.hpp"
#include "Foundation/NSString.hpp"
#include "QuartzCore/CADeveloperHUDProperties.hpp"
#include "objc_pointer.hpp"
#include <string>
#include <vector>

namespace dxmt {

class HUDState {
public:
  void initialize(const std::string &heading);

  void begin();

  void
  printLine(const std::string &str) {
    printLine(str.c_str());
  }

  void printLine(const char *c_str);

  void end();

  HUDState(CA::DeveloperHUDProperties *hud) : hud_(hud){};

private:
  CA::DeveloperHUDProperties *hud_;
  std::vector<Obj<NS::String>> line_labels_;
  Obj<NS::AutoreleasePool> pool_;
  unsigned current_line_;
};

} // namespace dxmt