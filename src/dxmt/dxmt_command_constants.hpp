#pragma once

#ifdef __METAL__
#define CONSTANT constant
#else
#define CONSTANT
#endif

namespace dxmt {

constexpr CONSTANT auto kCustomBufferArgumentIndex0 = 30;
constexpr CONSTANT auto kCustomBufferArgumentIndex1 = 29;

} // namespace dxmt
