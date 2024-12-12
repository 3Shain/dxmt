#pragma once

extern "C" void _massert (const char *_Message, const char *_File, unsigned _Line) __attribute__((noreturn));

#ifdef NDEBUG
#define D3D11_ASSERT(_Expression) ((void)0)
#else
#define D3D11_ASSERT(_Expression) \
 (void) \
 ((!!(_Expression)) || \
  (_massert(#_Expression,__FILE__,__LINE__),0))
#endif

#define IMPLEMENT_ME                                                           \
  Logger::err(                                                                 \
      str::format(__FILE__, ":", __FUNCTION__, " is not implemented."));       \
  D3D11_ASSERT(0);

#define UNREACHABLE                                                                                                    \
  Logger::err(str::format(__FILE__, ":", __FUNCTION__, " runs into an unreachable path."));                            \
  D3D11_ASSERT(0);

template <class... Ts> struct patterns : Ts... {
  using Ts::operator()...;
};
template <class... Ts> patterns(Ts...) -> patterns<Ts...>;
