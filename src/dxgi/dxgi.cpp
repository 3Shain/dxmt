#include "log/log.hpp"

namespace dxmt {
Logger Logger::s_instance("dxgi.log");

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason,
                               LPVOID reserved) {
  if (reason != DLL_PROCESS_ATTACH)
    return TRUE;

  DisableThreadLibraryCalls(instance);

  return TRUE;
}
} // namespace dxmt