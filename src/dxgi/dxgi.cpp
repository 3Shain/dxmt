#include "dxgi_private.h"
#include "dxgi_factory.h"
#include "log/log.hpp"

namespace dxmt {
Logger Logger::s_instance("dxgi.log");

extern "C" HRESULT __stdcall CreateDXGIFactory1(REFIID riid, void **ppFactory) {
  try {
    Com<MTLDXGIFactory> factory = new MTLDXGIFactory(0);
    HRESULT hr = factory->QueryInterface(riid, ppFactory);

    if (FAILED(hr))
      return hr;

    return S_OK;
  } catch (const MTLD3DError &e) {
    Logger::err(e.message());
    return E_FAIL;
  }
}

extern "C" HRESULT __stdcall CreateDXGIFactory(REFIID riid, void **factory) {
  return CreateDXGIFactory1(riid, factory);
}
} // namespace dxmt

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason,
                               LPVOID reserved) {
  if (reason != DLL_PROCESS_ATTACH)
    return TRUE;

  DisableThreadLibraryCalls(instance);

  return TRUE;
}