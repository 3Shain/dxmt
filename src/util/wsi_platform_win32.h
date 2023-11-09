#pragma once

#include <windows.h>

namespace dxmt::wsi {

/**
 * \brief Impl-dependent state
 */
struct DXMTWindowState {
  LONG style = 0;
  LONG exstyle = 0;
  RECT rect = {0, 0, 0, 0};
};

} // namespace dxmt::wsi