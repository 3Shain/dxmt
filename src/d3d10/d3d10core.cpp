
#include "d3d10.h"
#include "log/log.hpp"

namespace dxmt {
Logger Logger::s_instance("d3d10core.log");

extern "C" HRESULT STDMETHODCALLTYPE D3D10CoreCreateDevice(
    IDXGIFactory *pFactory, IDXGIAdapter *pAdapter, UINT Flags,
    D3D_FEATURE_LEVEL FeatureLevel, ID3D10Device **ppDevice) {
  ERR("D3D10CoreCreateDevice: stub");
  return E_NOTIMPL;
}
} // namespace dxmt