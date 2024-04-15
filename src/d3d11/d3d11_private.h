#pragma once

#define IMPLEMENT_ME                                                           \
  Logger::err(                                                                 \
      str::format(__FILE__, ":", __FUNCTION__, " is not implemented."));       \
  while (1) {                                                                  \
  }


template <class... Ts> struct patterns : Ts... { using Ts::operator()...; };
template <class... Ts> patterns(Ts...) -> patterns<Ts...>;
