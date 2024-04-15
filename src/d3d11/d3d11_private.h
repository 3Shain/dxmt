#pragma once
#include <cassert>

#define IMPLEMENT_ME                                                           \
  Logger::err(                                                                 \
      str::format(__FILE__, ":", __FUNCTION__, " is not implemented."));       \
  assert(0);

template <class... Ts> struct patterns : Ts... {
  using Ts::operator()...;
};
template <class... Ts> patterns(Ts...) -> patterns<Ts...>;
