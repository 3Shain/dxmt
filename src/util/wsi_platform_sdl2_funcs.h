/*
 * This file is part of DXMT, Copyright (c) 2023 Feifan He
 *
 * Derived from a part of DXVK (originally under zlib License),
 * Copyright (c) 2017 Philip Rebohle
 * Copyright (c) 2019 Joshua Ashton
 *
 * See <https://github.com/doitsujin/dxvk/blob/master/LICENSE>
 */

#if defined(DXMT_WSI_SDL2)
SDL_PROC(SDL_DisplayMode*, SDL_GetClosestDisplayMode, (int, const SDL_DisplayMode*, SDL_DisplayMode*))
SDL_PROC(int, SDL_GetCurrentDisplayMode, (int, SDL_DisplayMode*))
SDL_PROC(int, SDL_GetDesktopDisplayMode, (int, SDL_DisplayMode*))
SDL_PROC(int, SDL_GetDisplayBounds, (int, SDL_Rect*))
SDL_PROC(int, SDL_GetDisplayMode, (int, int, SDL_DisplayMode*))
SDL_PROC(const char*, SDL_GetError, (void))
SDL_PROC(int, SDL_GetNumVideoDisplays, (void))
SDL_PROC(int, SDL_GetWindowDisplayIndex, (SDL_Window*))
SDL_PROC(int, SDL_SetWindowDisplayMode, (SDL_Window*, const SDL_DisplayMode*))
SDL_PROC(int, SDL_SetWindowFullscreen, (SDL_Window*, Uint32))
SDL_PROC(SDL_WindowFlags, SDL_GetWindowFlags, (SDL_Window *))
SDL_PROC(void, SDL_GetWindowSize, (SDL_Window*, int*, int*))
SDL_PROC(void, SDL_SetWindowSize, (SDL_Window*, int, int))
SDL_PROC(SDL_MetalView, SDL_Metal_CreateView, (SDL_Window*))
SDL_PROC(void*, SDL_Metal_GetLayer, (SDL_MetalView))
#undef SDL_PROC
#endif // defined(DXMT_WSI_SDL2)