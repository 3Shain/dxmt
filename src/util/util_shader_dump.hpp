#pragma once
#include "sha1/sha1_util.hpp"
#include <cstdint>

namespace dxmt {
    void DumpShaderBytecode(const Sha1Digest&, const void*, uint32_t);
}
