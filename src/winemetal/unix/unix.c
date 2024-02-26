#include "objc/runtime.h"
#include "objc/message.h"
#include "objc-wrapper/abi.h"
#import <Metal/Metal.h>
#include "Block.h"
#include "dlfcn.h"
#include "pthread.h"

typedef struct macdrv_opaque_metal_device *macdrv_metal_device;
typedef struct macdrv_opaque_metal_view *macdrv_metal_view;
typedef struct macdrv_opaque_metal_layer *macdrv_metal_layer;
typedef struct macdrv_opaque_view *macdrv_view;
typedef struct macdrv_opaque_window *macdrv_window;
typedef struct macdrv_opaque_window_data *macdrv_window_data;
typedef struct opaque_window_surface *window_surface;
typedef struct opaque_HWND *HWND;

extern macdrv_metal_device SYSV_ABI macdrv_create_metal_device(void);
extern void SYSV_ABI macdrv_release_metal_device(macdrv_metal_device d);
extern macdrv_metal_view SYSV_ABI
macdrv_view_create_metal_view(macdrv_view v, macdrv_metal_device d);
extern macdrv_metal_layer SYSV_ABI
macdrv_view_get_metal_layer(macdrv_metal_view v);
extern void SYSV_ABI macdrv_view_release_metal_view(macdrv_metal_view v);
extern macdrv_view SYSV_ABI macdrv_get_cocoa_view(HWND hwnd);
extern macdrv_view SYSV_ABI macdrv_get_client_cocoa_view(HWND hwnd);
extern macdrv_window_data SYSV_ABI get_win_data(HWND hwnd);
extern void SYSV_ABI release_win_data(macdrv_window_data data);

const void *__wine_unix_call_funcs[] = {
    &objc_lookUpClass,
    &sel_registerName,
    &objc_msgSend,
    &objc_getProtocol,
    _NSConcreteGlobalBlock, // 4
    _NSConcreteStackBlock,  // 5
    // &_Block_object_assign,  // 6
    0,
    // &_Block_object_dispose, // 7
    0,
    &dlsym,                 // 8
    &MTLCreateSystemDefaultDevice,
    &MTLCopyAllDevices,
    &MTLCopyAllDevicesWithObserver,
    &MTLRemoveDeviceObserver,

    &macdrv_create_metal_device,
    &macdrv_release_metal_device,
    &macdrv_view_create_metal_view,
    &macdrv_view_get_metal_layer,
    &macdrv_view_release_metal_view,
    &macdrv_get_cocoa_view,
    &macdrv_get_client_cocoa_view,
    &get_win_data,
    &release_win_data,
    &dispatch_semaphore_create,
    &dispatch_semaphore_signal,
    &dispatch_semaphore_wait,
    &printf,
    &dispatch_release,
    &pthread_getspecific,
    &pthread_setspecific,
    &malloc,
    &free,
    &sigsetjmp,
    &siglongjmp,
    &dispatch_get_main_queue,
    &dispatch_data_create,
};
// wow64: things become funny