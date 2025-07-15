#if defined(DXMT_WSI_SDL2)

#include "wsi_platform_sdl2.hpp"

#include "log/log.hpp"
#include "util_string.hpp"

#include <dlfcn.h>
#include <SDL2/SDL.h>

#include <mach-o/dyld.h>

namespace dxmt::wsi {

// this is messy, but dynamically linking on macos seems pretty finicky
SDL2Initializer::SDL2Initializer() {

  // first, try to see if we already loaded the dylib in the process...
  uint32_t image_count = _dyld_image_count();
  for (uint32_t i = 0; i < image_count; i++) {
    const char* image_name = _dyld_get_image_name(i);
    if(strstr(image_name, "SDL2") != nullptr) {
      libsdl = dlopen(image_name, RTLD_NOW | RTLD_LOCAL);
      if (libsdl) {
        Logger::trace(str::format("SDL2 WSI: Found SDL2 in loaded images: ", image_name));
        break;
      }
    }
  }

  // if there's no sdl, try to force load it
  if (!libsdl) {
    constexpr const char* possible_sdl2_paths[] = {
      "libSDL2.dylib",
      "libSDL2-2.0.dylib",
      "libSDL2-2.0.0.dylib",
      "/Library/Frameworks/SDL2.framework/SDL2"
    };
  
    for(const auto& iter : possible_sdl2_paths) {
      libsdl = dlopen(iter, RTLD_NOW | RTLD_LOCAL);
      if (libsdl) {
        Logger::trace(str::format("SDL2 WSI: Found SDL2 by dlopen: ", iter));
        break;
      }
    }
  }

  // if there's *still* no sdl, the app might've statically linked against sdl, so we can try dlsym with RTLD_DEFAULT
  if(!libsdl) {
    #define SDL_PROC(ret, name, params) \
      name = reinterpret_cast<pfn_##name>(dlsym(RTLD_DEFAULT, #name)); \
      if (!name) { \
        Logger::err(str::format("SDL2 WSI: Failed to dlsym RTLD_DEFAULT function named ", #name)); \
      }
    #include "wsi_platform_sdl2_funcs.h"
  } else {
    #define SDL_PROC(ret, name, params) \
      name = reinterpret_cast<pfn_##name>(dlsym(libsdl, #name)); \
      if (!name) { \
        Logger::err(str::format("SDL2 WSI: Failed to dlsym from module function named ", #name)); \
      }
    #include "wsi_platform_sdl2_funcs.h"
  }
  
}

}

#endif // defined(DXMT_WSI_SDL2)