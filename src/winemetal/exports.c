#define ASM_FORWARD(name, offset)                                              \
  __attribute__((naked, dllexport)) void name(void) {                          \
    asm("movq    .refptr.__wine_unixlib_handle(%rip), %rax\n"                  \
        "movq    (%rax), %rax\n"                                               \
        "addq    $" #offset "* 8 , %rax\n"                                         \
        "jmpq    *(%rax)\n\t");                                                \
  };

ASM_FORWARD(objc_lookUpClass, 0)
ASM_FORWARD(sel_registerName, 1)
ASM_FORWARD(objc_msgSend, 2)
ASM_FORWARD(objc_getProtocol, 3)
ASM_FORWARD(unix_dlsym, 8)
ASM_FORWARD(MTLCreateSystemDefaultDevice, 9)
ASM_FORWARD(MTLCopyAllDevices, 10)
ASM_FORWARD(MTLCopyAllDevicesWithObserver, 11)
ASM_FORWARD(MTLRemoveDeviceObserver, 12)
ASM_FORWARD(macdrv_create_metal_device, 13)
ASM_FORWARD(macdrv_release_metal_device, 14)
ASM_FORWARD(macdrv_view_create_metal_view, 15)
ASM_FORWARD(macdrv_view_get_metal_layer, 16)
ASM_FORWARD(macdrv_view_release_metal_view, 17)
// ASM_FORWARD(macdrv_get_cocoa_view, 18)
ASM_FORWARD(macdrv_get_client_cocoa_view, 19)
ASM_FORWARD(get_win_data, 20)
ASM_FORWARD(release_win_data, 21)
ASM_FORWARD(dispatch_semaphore_create, 22)
ASM_FORWARD(dispatch_semaphore_signal, 23)
ASM_FORWARD(dispatch_semaphore_wait, 24)
ASM_FORWARD(unix_printf, 25)
ASM_FORWARD(dispatch_release, 26)
ASM_FORWARD(unix_malloc, 29)
ASM_FORWARD(unix_free, 30)
ASM_FORWARD(SM50CompileGeometryPipelineVertex, 31)
ASM_FORWARD(SM50CompileGeometryPipelineGeometry, 32)
ASM_FORWARD(dispatch_get_main_queue, 33)
ASM_FORWARD(dispatch_data_create, 34)
ASM_FORWARD(SM50GetCompiledBitcode, 38)
ASM_FORWARD(SM50DestroyBitcode, 39)
ASM_FORWARD(SM50GetErrorMesssage, 40)
ASM_FORWARD(SM50FreeError, 41)
ASM_FORWARD(winemetal_unix_init, 42)
ASM_FORWARD(objc_msgSend_fpret, 43)
ASM_FORWARD(SM50CompileTessellationPipelineVertex, 44)
ASM_FORWARD(SM50CompileTessellationPipelineHull, 45)
ASM_FORWARD(SM50CompileTessellationPipelineDomain, 46)
ASM_FORWARD(__pthread_set_qos_class_self_np, 47)
ASM_FORWARD(objc_msgSend_stret, 48)
extern void *__wine_unixlib_handle;