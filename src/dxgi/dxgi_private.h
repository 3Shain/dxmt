#pragma once

#include "../util/com/com_guid.h"
#include "../util/com/com_object.h"
#include "../util/com/com_pointer.h"

#include "../util/objc_pointer.h"

#include "../util/log/log.h"
#include "../util/log/log_debug.h"

#include "../util/rc/util_rc.h"
#include "../util/rc/util_rc_ptr.h"

#include "../util/util_env.h"
// #include "../util/util_enum.h"
#include "../util/util_error.h"
// #include "../util/util_flags.h"
#include "../util/util_likely.h"
#include "../util/util_math.h"
#include "../util/util_string.h"

#include <dxgi1_2.h>

#include "dxgi_names.hpp"
#include "dxgi_interfaces.h"

#define IMPLEMENT_ME                                                           \
  Logger::err(                                                                 \
      str::format(__FILE__, ":", __FUNCTION__, " is not implemented."));       \
  while (1) {                                                                  \
  }
