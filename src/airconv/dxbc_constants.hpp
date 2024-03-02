#pragma once

#include <cstdint>

namespace dxmt::dxbc {

enum class RegisterComponentType: uint32_t { Unknown, Uint = 1, Int = 2, Float = 3 };

enum class InstructionReturnType { Float = 0, Uint = 1 };

}