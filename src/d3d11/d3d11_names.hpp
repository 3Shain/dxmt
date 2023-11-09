#pragma once

#include <ostream>
#include "../util/util_names.h"
#include "d3d11.h"

std::ostream& operator << (std::ostream& os, D3D_FEATURE_LEVEL e);
std::ostream& operator << (std::ostream& os, D3D11_RESOURCE_DIMENSION e);