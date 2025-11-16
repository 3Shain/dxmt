#include "d3d11_fence.hpp"
#include "d3d11_device_child.hpp"
#include "d3d11_resource.hpp"

namespace dxmt {

class MTLD3D11FenceImpl : public MTLD3D11DeviceChild<MTLD3D11Fence> {
public:
  MTLD3D11FenceImpl(MTLD3D11Device *pDevice, bool shared)
      : MTLD3D11DeviceChild<MTLD3D11Fence>(pDevice) {
    event = pDevice->GetMTLDevice().newSharedEvent();
    if (shared) {
      D3DKMT_CREATESYNCHRONIZATIONOBJECT2 create = {};
      create.hDevice = pDevice->GetLocalD3DKMT();
      create.Info.Type = D3DDDI_FENCE;
      create.Info.Flags.Shared = 1;
      create.Info.Flags.NtSecuritySharing = 1;
      if (D3DKMTCreateSynchronizationObject2(&create)) {
        ERR("D3D11Fence: Failed to create D3DKMT handle");
        return;
      }
      local_kmt = create.hSyncObject;

      mach_port_t mach_port = event.createMachPort();
      if (!mach_port) {
        ERR("D3D11Fence: Failed to create mach port for shared fence");
        return;
      }
      char mach_port_name[54];
      MakeUniqueSharedName(mach_port_name);
      if (!WMTBootstrapRegister(mach_port_name, mach_port)) {
        ERR("D3D11Fence: Failed to register mach port for shared fence");
        return;
      }
      D3DKMT_ESCAPE escape = {};
      escape.Type = D3DKMT_ESCAPE_UPDATE_RESOURCE_WINE;
      escape.pPrivateDriverData = mach_port_name;
      escape.PrivateDriverDataSize = sizeof(mach_port_name);
      escape.hContext = local_kmt;
      if (!D3DKMTEscape(&escape)) {
        ERR("D3D11Fence: Failed to escape mach port for shared fence");
        return;
      }
    }
  };

  MTLD3D11FenceImpl(MTLD3D11Device *pDevice, WMT::Reference<WMT::SharedEvent> importedEvent, 
                    D3DKMT_HANDLE localHandle)
      : MTLD3D11DeviceChild<MTLD3D11Fence>(pDevice) {
    event = std::move(importedEvent);
    local_kmt = localHandle;
  };

  ~MTLD3D11FenceImpl() {
    if (local_kmt) {
      D3DKMT_DESTROYSYNCHRONIZATIONOBJECT destroy = {};
      destroy.hSyncObject = local_kmt;
      D3DKMTDestroySynchronizationObject(&destroy);
    }
  };

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11Fence)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11Query), riid)) {
      WARN("D3D11Fence: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  };

  HRESULT STDMETHODCALLTYPE
  CreateSharedHandle(const SECURITY_ATTRIBUTES *pAttributes, DWORD Access,
                     const WCHAR *Name, HANDLE *pHandle) final {
    return E_NOTIMPL;
  };

  UINT64 STDMETHODCALLTYPE GetCompletedValue() final {
    return event.signaledValue();
  };

  HRESULT STDMETHODCALLTYPE SetEventOnCompletion(UINT64 Value,
                                                 HANDLE Event) final {
    auto shared_event_listener = this->m_parent->GetDXMTDevice().queue().GetSharedEventListener();
    MTLSharedEvent_setWin32EventAtValue(event.handle, shared_event_listener, Event, Value);
    return S_OK;
  };
};

HRESULT
CreateFence(MTLD3D11Device *pDevice, UINT64 InitialValue,
            D3D11_FENCE_FLAG Flags, REFIID riid, void **ppFence) {
  bool shared = !!(Flags & (D3D11_FENCE_FLAG_SHARED | D3D11_FENCE_FLAG_SHARED_CROSS_ADAPTER));
  auto fence = new MTLD3D11FenceImpl(pDevice, shared);
  fence->event.signalValue(InitialValue);
  return fence->QueryInterface(riid, ppFence);
}

HRESULT
OpenSharedFence(MTLD3D11Device *pDevice, HANDLE hResource,
                REFIID riid, void **ppFence) {
  return E_FAIL; // Not implemented yet
}

} // namespace dxmt