#pragma once

#include <ostream>
#include "../util/util_names.h"
#include "dxgi.h"
#include "Metal/MTLPixelFormat.hpp"

namespace MTL {
    __attribute__((dllexport)) std::ostream& operator << (std::ostream& os, PixelFormat e);
}

__attribute__((dllexport)) std::ostream& operator << (std::ostream& os, DXGI_FORMAT e);
