#include "objc/runtime.h"
#include "objc/message.h"
#include "objc-wrapper/abi.h"
#import <Metal/Metal.h>
#import <MetalFX/MTLFXTemporalScaler.h>
#include "dlfcn.h"
#include "pthread.h"
#include "../unix_thunks.h"

typedef int NTSTATUS;
#define STATUS_SUCCESS 0

typedef struct macdrv_opaque_metal_device *macdrv_metal_device;
typedef struct macdrv_opaque_metal_view *macdrv_metal_view;
typedef struct macdrv_opaque_metal_layer *macdrv_metal_layer;
typedef struct macdrv_opaque_view *macdrv_view;
typedef struct macdrv_opaque_window *macdrv_window;
typedef struct macdrv_opaque_window_data *macdrv_window_data;
typedef struct opaque_window_surface *window_surface;
typedef struct opaque_HWND *HWND;

struct macdrv_functions_t
{
    void (*macdrv_init_display_devices)(BOOL);
    struct macdrv_win_data*(*get_win_data)(HWND hwnd);
    void (*release_win_data)(struct macdrv_win_data *data);
    macdrv_window(*macdrv_get_cocoa_window)(HWND hwnd, BOOL require_on_screen);
    macdrv_metal_device (*macdrv_create_metal_device)(void);
    void (*macdrv_release_metal_device)(macdrv_metal_device d);
    macdrv_metal_view (*macdrv_view_create_metal_view)(macdrv_view v, macdrv_metal_device d);
    macdrv_metal_layer (*macdrv_view_get_metal_layer)(macdrv_metal_view v);
    void (*macdrv_view_release_metal_view)(macdrv_metal_view v);
    void (*on_main_thread)(dispatch_block_t b);
};

static int winemetal_unix_init();

static NTSTATUS thunk_SM50Initialize(void *args)
{
    struct sm50_initialize_params *params = args;

    params->ret = SM50Initialize(params->bytecode, params->bytecode_size, params->shader, params->reflection, params->error);

    return STATUS_SUCCESS;
}

static NTSTATUS thunk_SM50Destroy(void *args)
{
    struct sm50_destroy_params *params = args;

    SM50Destroy(params->shader);

    return STATUS_SUCCESS;
}

static NTSTATUS thunk_SM50Compile(void *args)
{
    struct sm50_compile_params *params = args;

    params->ret = SM50Compile((SM50Shader*)params->shader, params->args, params->func_name, params->bitcode, params->error);

    return STATUS_SUCCESS;
}


#include <signal.h>

void temp_handler(int signum) {
    printf("received signal %d in temp_handler(), and it may cause problem!\n", signum);
}

static const int SIGNALS[] = {SIGHUP,
                              SIGINT,
                              SIGTERM,
                              SIGUSR2,
                              SIGILL,
                              SIGTRAP,
                              SIGABRT,
                              SIGFPE,
                              SIGBUS,
                              SIGSEGV,
                              SIGQUIT
#ifdef SIGSYS
                              ,
                              SIGSYS
#endif
#ifdef SIGXCPU
                              ,
                              SIGXCPU
#endif
#ifdef SIGXFSZ
                              ,
                              SIGXFSZ
#endif
#ifdef SIGEMT
                              ,
                              SIGEMT
#endif
                              ,
                              SIGUSR1
#ifdef SIGINFO
                              ,
                              SIGINFO
#endif
};

NTSTATUS CreateMTLFXTemporalScaler(struct create_fxscaler_params *params) {

  struct sigaction old_action[sizeof(SIGNALS) / sizeof(int)], new_action;
  new_action.sa_handler = temp_handler;
  sigemptyset(&new_action.sa_mask);
  new_action.sa_flags = 0;

  MTLFXTemporalScalerDescriptor *desc = params->desc;
  id<MTLDevice> device = params->device;

  /* 
  HACK: newTemporalScalerWithDevice calls sigaction() which can break wine,
  so we save current handlers and restore them when the method returns.
  */
  for (unsigned int i = 0; i < sizeof(SIGNALS) / sizeof(int); i++)
    sigaction(SIGNALS[i], &new_action, &old_action[i]);
  params->ret = [desc newTemporalScalerWithDevice:device];
  for (unsigned int i = 0; i < sizeof(SIGNALS) / sizeof(int); i++)
    sigaction(SIGNALS[i], &old_action[i], NULL);
  return STATUS_SUCCESS;
};

const void *__wine_unix_call_funcs[] = {
    &objc_lookUpClass,
    &sel_registerName,
    &objc_msgSend,
    &objc_getProtocol,
    &CreateMTLFXTemporalScaler,
    0,
    0,
    0,
    &dlsym,
    &MTLCreateSystemDefaultDevice,
    &MTLCopyAllDevices,
    &MTLCopyAllDevicesWithObserver,
    &MTLRemoveDeviceObserver,
    // &macdrv_create_metal_device,
    0,
    // &macdrv_release_metal_device,
    0,
    // &macdrv_view_create_metal_view,
    0,
    // &macdrv_view_get_metal_layer,
    0,
    // &macdrv_view_release_metal_view,
    0,
    // &macdrv_get_cocoa_view,
    0,
    // &macdrv_get_client_cocoa_view,
    0,
    // &get_win_data,
    0,
    // &release_win_data,
    0,
    &dispatch_semaphore_create,
    &dispatch_semaphore_signal,
    &dispatch_semaphore_wait,
    &printf,
    &dispatch_release,
    0,
    0,
    &malloc,
    &free,
    &SM50CompileGeometryPipelineVertex,
    &SM50CompileGeometryPipelineGeometry,
    &dispatch_get_main_queue,
    &dispatch_data_create,
    &thunk_SM50Initialize,
    &thunk_SM50Destroy,
    &thunk_SM50Compile,
    &SM50GetCompiledBitcode,
    &SM50DestroyBitcode,
    &SM50GetErrorMesssage,
    &SM50FreeError,
    &winemetal_unix_init,
    &objc_msgSend_fpret,
    &SM50CompileTessellationPipelineVertex,
    &SM50CompileTessellationPipelineHull,
    &SM50CompileTessellationPipelineDomain,
    &pthread_set_qos_class_self_np,
    &objc_msgSend_stret,
};
// wow64: things become funny

static struct macdrv_functions_t* macdrv_functions;

static int winemetal_unix_init() {
    if((macdrv_functions = dlsym(RTLD_DEFAULT, "macdrv_functions")))
    {
        __wine_unix_call_funcs[13] = macdrv_functions->macdrv_create_metal_device;
        __wine_unix_call_funcs[14] = macdrv_functions->macdrv_release_metal_device;
        __wine_unix_call_funcs[15] = macdrv_functions->macdrv_view_create_metal_view;
        __wine_unix_call_funcs[16] = macdrv_functions->macdrv_view_get_metal_layer;
        __wine_unix_call_funcs[17] = macdrv_functions->macdrv_view_release_metal_view;
        __wine_unix_call_funcs[19] = macdrv_functions->macdrv_get_cocoa_window;
        __wine_unix_call_funcs[20] = macdrv_functions->get_win_data;
        __wine_unix_call_funcs[21] = macdrv_functions->release_win_data;
        return 0;
    }

#define LOAD_FUNCPTR(f, idx) \
    if (!(__wine_unix_call_funcs[idx] = dlsym( RTLD_DEFAULT, #f ))) \
    { \
        goto fail; \
    }

    LOAD_FUNCPTR( macdrv_create_metal_device, 13 )
    LOAD_FUNCPTR( macdrv_release_metal_device, 14 )
    LOAD_FUNCPTR( macdrv_view_create_metal_view, 15 )
    LOAD_FUNCPTR( macdrv_view_get_metal_layer, 16 )
    LOAD_FUNCPTR( macdrv_view_release_metal_view, 17 )
    LOAD_FUNCPTR( macdrv_get_cocoa_window, 19 )
    LOAD_FUNCPTR( get_win_data, 20 )
    LOAD_FUNCPTR( release_win_data, 21 )
#undef LOAD_FUNCPTR

    return 0;

fail:
    return 1;
};