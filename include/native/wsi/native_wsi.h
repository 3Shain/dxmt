#pragma once

#ifdef DXVK_WSI_WIN32
#error You shouldnt be using this code path.
#elif DXVK_WSI_SDL2
#include "wsi/native_sdl2.h"
#elif DXVK_WSI_GLFW
#include "wsi/native_glfw.h"
#elif DXVK_WSI_HEADLESS
#if defined(VBOX) && defined(_WIN32)
#include "native/wsi/native_headless.h"
#else
#include "wsi/native_headless.h"
#endif
#else
#error Unknown wsi!
#endif