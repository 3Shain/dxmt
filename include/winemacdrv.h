#pragma once
#include "objc-wrapper/abi.h"
#if __cplusplus
extern "C" {
#endif
/**
 * winemac.drv private definitions
 */
#include "windef.h"

typedef struct macdrv_opaque_metal_device *macdrv_metal_device;
typedef struct macdrv_opaque_metal_view *macdrv_metal_view;
typedef struct macdrv_opaque_metal_layer *macdrv_metal_layer;
typedef struct macdrv_opaque_view *macdrv_view;
typedef struct macdrv_opaque_window *macdrv_window;
typedef struct opaque_window_surface *window_surface;

struct macdrv_win_data {
  HWND hwnd; /* hwnd that this private data belongs to */
  macdrv_window cocoa_window;
  macdrv_view cocoa_view;
  macdrv_view client_cocoa_view;
  RECT window_rect;   /* USER window rectangle relative to parent */
  RECT whole_rect;    /* Mac window rectangle for the whole window relative to
                         parent */
  RECT client_rect;   /* client area relative to parent */
  int pixel_format;   /* pixel format for GL */
  COLORREF color_key; /* color key for layered window; CLR_INVALID is not color
                         keyed */
  HANDLE drag_event;  /* event to signal that Cocoa-driven window dragging has
                         ended */
  unsigned int on_screen : 1; /* is window ordered in? (minimized or not) */
  unsigned int shaped : 1;    /* is window using a custom region shape? */
  unsigned int layered : 1;   /* is window layered and with valid attributes? */
  unsigned int
      ulw_layered : 1; /* has UpdateLayeredWindow() been called for window? */
  unsigned int per_pixel_alpha : 1; /* is window using per-pixel alpha? */
  unsigned int minimized : 1;       /* is window minimized? */
  unsigned int swap_interval : 1;   /* GL swap interval for window */
  window_surface surface;
  window_surface unminimized_surface;
};

extern macdrv_metal_device SYSV_ABI macdrv_create_metal_device(void);
extern void SYSV_ABI macdrv_release_metal_device(macdrv_metal_device d);
extern macdrv_metal_view SYSV_ABI
macdrv_view_create_metal_view(macdrv_view v, macdrv_metal_device d);
/*
(CAMetalLayer*)view.layer
*/
extern macdrv_metal_layer SYSV_ABI
macdrv_view_get_metal_layer(macdrv_metal_view v);
extern void SYSV_ABI macdrv_view_release_metal_view(macdrv_metal_view v);
extern macdrv_view SYSV_ABI macdrv_get_client_cocoa_view(HWND hwnd);
extern struct macdrv_win_data *SYSV_ABI get_win_data(HWND hwnd);
extern void SYSV_ABI release_win_data(struct macdrv_win_data *data);
#if __cplusplus
}
#endif