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

BOOL WINAPI WineMetalEntry(HANDLE hDllHandle, DWORD dwReason, LPVOID lpreserved) {
  if (dwReason == DLL_PROCESS_ATTACH && __wine_init_unix_call())
    return FALSE;
  // Then call the actual CRT startup
  return DllMainCRTStartup(hDllHandle, dwReason, lpreserved);
}
