#include "d3d11_device.hpp"
#include "d3d11_private.h"
#define __DXMT_DISABLE_OCCLUSION_QUERY__ 1
#include "d3d11_context_impl.cpp"
#include "dxmt_command_queue.hpp"

namespace dxmt {

struct DeferredContextInternalState {
  Com<MTLD3D11CommandList> current_cmdlist;
};

template<typename Object> Rc<Object> forward_rc(Rc<Object>& obj) {
  return obj;
}

using DeferredContextBase = MTLD3D11DeviceContextImplBase<DeferredContextInternalState>;

template <>
template <CommandWithContext<ArgumentEncodingContext> cmd>
void
DeferredContextBase:: Emit(cmd &&fn) {
  ctx_state.current_cmdlist->Emit(std::forward<cmd>(fn));
}

template <>
template <typename T>
moveonly_list<T>
DeferredContextBase::AllocateCommandData(size_t n) {
  return ctx_state.current_cmdlist->AllocateCommandData<T>(n);
}

template <>
std::tuple<void *, MTL::Buffer *, uint64_t>
DeferredContextBase::AllocateStagingBuffer(size_t size, size_t alignment) {
  return ctx_state.current_cmdlist->AllocateStagingBuffer(size, alignment);
}

template <>
void
DeferredContextBase::UseCopyDestination(
    IMTLD3D11Staging *pResource, uint32_t Subresource, MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
    uint32_t *pBytesPerImage
) {
  pResource->UseCopyDestination(Subresource, 0, pBuffer, pBytesPerRow, pBytesPerImage);
  IMPLEMENT_ME // TODO: track usage
}

template <>
void
DeferredContextBase::UseCopySource(
    IMTLD3D11Staging *pResource, uint32_t Subresource, MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
    uint32_t *pBytesPerImage
) {
  pResource->UseCopySource(Subresource, 0, pBuffer, pBytesPerRow, pBytesPerImage);
  IMPLEMENT_ME // TODO: track usage
}

class MTLD3D11DeferredContext : public DeferredContextBase {
public:
  MTLD3D11DeferredContext(MTLD3D11Device *pDevice, UINT ContextFlags) :
      DeferredContextBase(pDevice, ctx_state),
      ctx_state(),
      context_flag(ContextFlags) {
    device->CreateCommandList((ID3D11CommandList **)&ctx_state.current_cmdlist);
  }

  ULONG STDMETHODCALLTYPE
  AddRef() override {
    uint32_t refCount = this->refcount++;
    if (unlikely(!refCount))
      this->m_parent->AddRef();

    return refCount + 1;
  }

  ULONG STDMETHODCALLTYPE
  Release() override {
    uint32_t refCount = --this->refcount;
    D3D11_ASSERT(refCount != ~0u && "try to release a 0 reference object");
    if (unlikely(!refCount)) {
      this->m_parent->Release();
      delete this;
    }

    return refCount;
  }

  HRESULT
  Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags,
      D3D11_MAPPED_SUBRESOURCE *pMappedResource) override {
    IMPLEMENT_ME
    return E_FAIL;
  }

  void
  Unmap(ID3D11Resource *pResource, UINT Subresource) override {
    if (auto dynamic = GetDynamic(pResource)) {
      return;
    }
    IMPLEMENT_ME;
  }

  void
  Begin(ID3D11Asynchronous *pAsync) override {
    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT:
      break;
    case D3D11_QUERY_OCCLUSION:
    case D3D11_QUERY_OCCLUSION_PREDICATE: {
      // auto query = static_cast<IMTLD3DOcclusionQuery *>(pAsync);
      IMPLEMENT_ME
      break;
    }
    case D3D11_QUERY_TIMESTAMP:
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      // ignore
      break;
    }
    default:
      ERR("Deferred context: unhandled query type ", desc.Query);
      break;
    }
  }

  void
  End(ID3D11Asynchronous *pAsync) override {
    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT:
      IMPLEMENT_ME;
      promote_flush = true;
      break;
    case D3D11_QUERY_OCCLUSION:
    case D3D11_QUERY_OCCLUSION_PREDICATE: {
      // auto query = static_cast<IMTLD3DOcclusionQuery *>(pAsync);
      IMPLEMENT_ME;
      promote_flush = true;
      break;
    }
    case D3D11_QUERY_TIMESTAMP:
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      // ignore
      break;
    }
    default:
      ERR("Deferred context: unhandled query type ", desc.Query);
      break;
    }
  }

  HRESULT
  GetData(ID3D11Asynchronous *pAsync, void *pData, UINT DataSize, UINT GetDataFlags) override {
    return E_FAIL;
  }

  void
  Flush() override {
    // nop
  }

  void
  ExecuteCommandList(ID3D11CommandList *pCommandList, BOOL RestoreContextState) override{IMPLEMENT_ME}

  HRESULT FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList **ppCommandList) override {
    ResetEncodingContextState();

    ctx_state.current_cmdlist->promote_flush = promote_flush;
    promote_flush = false;

    *ppCommandList = std::move(ctx_state.current_cmdlist);
    device->CreateCommandList((ID3D11CommandList **)&ctx_state.current_cmdlist);

    if (RestoreDeferredContextState)
      RestoreEncodingContextState();
    else
      ResetD3D11ContextState();

    return S_OK;
  }

  D3D11_DEVICE_CONTEXT_TYPE GetType() override {
    return D3D11_DEVICE_CONTEXT_DEFERRED;
  }

  virtual void WaitUntilGPUIdle() override{
      // nop
  };

  virtual void PrepareFlush() override{
      // nop
  };

  void Commit() override{
      // nop
  };

private:
  DeferredContextInternalState ctx_state;
  std::atomic<uint32_t> refcount = 0;
  UINT context_flag;
};

Com<MTLD3D11DeviceContextBase>
CreateDeferredContext(MTLD3D11Device *pDevice, UINT ContextFlags) {
  return new MTLD3D11DeferredContext(pDevice, ContextFlags);
}

class MTLD3D11CommandListPool : public MTLD3D11CommandListPoolBase {
public:
  MTLD3D11CommandListPool(MTLD3D11Device *device) : device(device){};

  ~MTLD3D11CommandListPool(){

  };

  void
  RecycleCommandList(ID3D11CommandList *cmdlist) override {
    std::unique_lock<dxmt::mutex> lock(mutex);
    free_commandlist.push_back((MTLD3D11CommandList *)cmdlist);
  };

  void
  CreateCommandList(ID3D11CommandList **ppCommandList) override {
    std::unique_lock<dxmt::mutex> lock(mutex);
    if (!free_commandlist.empty()) {
      free_commandlist.back()->QueryInterface(IID_PPV_ARGS(ppCommandList));
      free_commandlist.pop_back();
      return;
    }
    auto instance = std::make_unique<MTLD3D11CommandList>(device, this, 0);
    *ppCommandList = ref(instance.get());
    instances.push_back(std::move(instance));
    return;
  };

private:
  std::vector<MTLD3D11CommandList *> free_commandlist;
  std::vector<std::unique_ptr<MTLD3D11CommandList>> instances;
  MTLD3D11Device *device;
  dxmt::mutex mutex;
};

std::unique_ptr<MTLD3D11CommandListPoolBase>
InitializeCommandListPool(MTLD3D11Device *device) {
  return std::make_unique<MTLD3D11CommandListPool>(device);
}

}; // namespace dxmt