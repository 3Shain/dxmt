#define ASM_FORWARD(name, offset)                                              \
  __attribute__((naked, dllexport)) void name(void) {                          \
    asm("movq    .refptr.__wine_unixlib_handle(%rip), %rax\n"                  \
        "movq    (%rax), %rax\n"                                               \
        "addq    $" #offset ", %rax\n"                                         \
        "jmpq    *(%rax)\n\t");                                                \
  };

ASM_FORWARD(objc_lookUpClass, 0)
ASM_FORWARD(sel_registerName, 8)
ASM_FORWARD(objc_msgSend, 16)
ASM_FORWARD(objc_getProtocol, 24)
ASM_FORWARD(_Block_object_assign, 8 * 6)
ASM_FORWARD(_Block_object_dispose, 8 * 7)
ASM_FORWARD(unix_dlsym, 8 * 8)
ASM_FORWARD(MTLCreateSystemDefaultDevice, 8 * 9)
ASM_FORWARD(MTLCopyAllDevices, 8 * 10)
ASM_FORWARD(MTLCopyAllDevicesWithObserver, 8 * 11)
ASM_FORWARD(MTLRemoveDeviceObserver, 8 * 12)
ASM_FORWARD(macdrv_create_metal_device, 8 * 13)
ASM_FORWARD(macdrv_release_metal_device, 8 * 14)
ASM_FORWARD(macdrv_view_create_metal_view, 8 * 15)
ASM_FORWARD(macdrv_view_get_metal_layer, 8 * 16)
ASM_FORWARD(macdrv_view_release_metal_view, 8 * 17)
ASM_FORWARD(macdrv_get_cocoa_view, 8 * 18)
ASM_FORWARD(macdrv_get_client_cocoa_view, 8 * 19)
ASM_FORWARD(get_win_data, 8 * 20)
ASM_FORWARD(release_win_data, 8 * 21)
ASM_FORWARD(dispatch_semaphore_create, 8 * 22)
ASM_FORWARD(dispatch_semaphore_signal, 8 * 23)
ASM_FORWARD(dispatch_semaphore_wait, 8 * 24)
ASM_FORWARD(unix_printf, 8 * 25)
ASM_FORWARD(dispatch_release, 8 * 26)
ASM_FORWARD(__pthread_getspecific, 8 * 27)
ASM_FORWARD(__pthread_setspecific, 8 * 28)
ASM_FORWARD(unix_malloc, 8 * 29)
ASM_FORWARD(unix_free, 8 * 30)
ASM_FORWARD(unix_setjmp, 8 * 31)
ASM_FORWARD(unix_longjmp, 8 * 32)
ASM_FORWARD(dispatch_get_main_queue, 8 * 33)
ASM_FORWARD(dispatch_data_create, 8 * 34)
ASM_FORWARD(SM50Initialize, 8 * 35)
ASM_FORWARD(SM50Destroy, 8 * 36)
ASM_FORWARD(SM50Compile, 8 * 37)
ASM_FORWARD(SM50GetCompiledBitcode, 8 * 38)
ASM_FORWARD(SM50DestroyBitcode, 8 * 39)
ASM_FORWARD(SM50GetErrorMesssage, 8 * 40)
ASM_FORWARD(SM50FreeError, 8 * 41)
extern void *__wine_unixlib_handle;