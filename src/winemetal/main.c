#include "windef.h"
#include "winbase.h"
#include "wineunixlib.h"

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
  if (reason != DLL_PROCESS_ATTACH)
    return TRUE;

  DisableThreadLibraryCalls(instance);
  return TRUE;
}

extern BOOL WINAPI DllMainCRTStartup(HANDLE hDllHandle, DWORD dwReason,
                                       LPVOID lpreserved);

extern void initialize_veh();
extern void cleanup_veh();

extern __attribute__((sysv_abi)) BOOL winemetal_unix_init();

BOOL WINAPI WineMetalEntry(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) {
  if (dwReason == DLL_PROCESS_ATTACH) {
    if (__wine_init_unix_call()) {
      return FALSE;
    }
    if (winemetal_unix_init()) {
      return FALSE;
    }
    initialize_veh();
  }
  if(dwReason == DLL_PROCESS_DETACH) {
    cleanup_veh();
  }

  // Then call the actual CRT startup
  return DllMainCRTStartup(hDllHandle, dwReason, lpreserved);
}
