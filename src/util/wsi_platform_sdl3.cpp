#if defined(DXMT_WSI_SDL3)

#include "wsi_platform_sdl3.hpp"

#include "log/log.hpp"
#include "util_string.hpp"

#include <dlfcn.h>
#include <SDL3/SDL.h>

#include <mach-o/dyld.h>

namespace dxmt::wsi {

SDL3Initializer::SDL3Initializer() {

  // first, try to see if we already loaded the dylib in the process...
  uint32_t image_count = _dyld_image_count();
  for (uint32_t i = 0; i < image_count; i++) {
    const char* image_name = _dyld_get_image_name(i);
    if(strstr(image_name, "SDL3") != nullptr) {
      libsdl = dlopen(image_name, RTLD_NOW | RTLD_LOCAL);
      if (libsdl) {
        Logger::trace(str::format("SDL3 WSI: Found SDL3 in loaded images: ", image_name));
        break;
      }
    }
  }

  // if there's no sdl, the app might've statically linked against sdl, so we can try dlsym with RTLD_DEFAULT
  #define SDL_PROC(ret, name, params) \
    name = reinterpret_cast<pfn_##name>(dlsym((libsdl == nullptr ? RTLD_DEFAULT : libsdl), #name)); \
    if (!name) { \
      Logger::err(str::format("SDL3 WSI: Failed to get function named ", #name)); \
    }
  #include "wsi_platform_sdl3_funcs.hpp"

  if (!SDL_GetClosestFullscreenDisplayMode) {
    ERR("SDL3 WSI: failed to get SDL function pointers.");
    abort();
  }

}

}

#endif // defined(DXMT_WSI_SDL3)
