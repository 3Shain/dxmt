#include "d3d11_device.hpp"
#include "d3d11_private.h"
#include "d3d11_context_impl.cpp"
#include "dxmt_command_queue.hpp"

namespace dxmt {

struct DeferredContextInternalState {
  CommandQueue &cmd_queue;
  Com<MTLD3D11CommandList> current_cmdlist;
  std::unordered_map<DynamicBuffer *, Rc<BufferAllocation>> current_dynamic_buffer_allocations;
  std::unordered_map<DynamicTexture *, Rc<TextureAllocation>> current_dynamic_texture_allocations;
  std::unordered_map<void *, std::pair<Com<IMTLD3DOcclusionQuery>, uint32_t>> building_visibility_queries;
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
      ctx_state({pDevice->GetDXMTDevice().queue(), {}}),
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
    UINT buffer_length = 0;
    UINT bind_flag = 0;
    if (auto dynamic = GetDynamicBuffer(pResource, &buffer_length, &bind_flag)) {
      D3D11_MAPPED_SUBRESOURCE Out;
      switch (MapType) {
      case D3D11_MAP_READ:
      case D3D11_MAP_WRITE:
      case D3D11_MAP_READ_WRITE:
        return E_INVALIDARG;
      case D3D11_MAP_WRITE_DISCARD: {
        if (bind_flag & D3D11_BIND_VERTEX_BUFFER) {
          state_.InputAssembler.VertexBuffers.set_dirty();
        }
        if (bind_flag & D3D11_BIND_CONSTANT_BUFFER) {
          for (auto &stage : state_.ShaderStages) {
            stage.ConstantBuffers.set_dirty();
          }
        }
        if (bind_flag & D3D11_BIND_SHADER_RESOURCE) {
          for (auto &stage : state_.ShaderStages) {
            stage.SRVs.set_dirty();
          }
        }
        Rc<BufferAllocation> new_allocation = dynamic->allocate(ctx_state.cmd_queue.CoherentSeqId());
        // track the current allocation in case of a following NO_OVERWRITE map
        ctx_state.current_dynamic_buffer_allocations.insert_or_assign(dynamic.ptr(), new_allocation);
        // collect allocated buffers and recycle them when the command list is released
        ctx_state.current_cmdlist->used_dynamic_allocations.push_back({dynamic.ptr(), new_allocation});
        Emit([allocation = new_allocation, buffer = Rc(dynamic->buffer)](ArgumentEncodingContext &enc) mutable {
          auto _ = buffer->rename(forward_rc(allocation));
        });

        Out.pData = new_allocation->mappedMemory;
        Out.RowPitch = buffer_length;
        Out.DepthPitch = buffer_length;
        break;
      }
      case D3D11_MAP_WRITE_NO_OVERWRITE: {
        auto ret = ctx_state.current_dynamic_buffer_allocations.find(dynamic.ptr());
        if (ret == ctx_state.current_dynamic_buffer_allocations.end()) {
          ERR("DeferredContext: Invalid NO_OVERWRITE map on deferred context occurs without any prior DISCARD map.");
          return E_INVALIDARG;
        }
        Out.pData = ret->second->mappedMemory;
        Out.RowPitch = buffer_length;
        Out.DepthPitch = buffer_length;
        break;
      }
      }
      if (pMappedResource) {
        *pMappedResource = Out;
      }
      return S_OK;
    }
    if (auto dynamic = GetDynamicTexture(pResource, &buffer_length, &bind_flag)) {
      D3D11_MAPPED_SUBRESOURCE Out;
      switch (MapType) {
      case D3D11_MAP_READ:
      case D3D11_MAP_WRITE:
      case D3D11_MAP_READ_WRITE:
        return E_INVALIDARG;
      case D3D11_MAP_WRITE_DISCARD: {
        for (auto &stage : state_.ShaderStages) {
            stage.SRVs.set_dirty();
        }
        Rc<TextureAllocation> new_allocation = dynamic->allocate(ctx_state.cmd_queue.CoherentSeqId());
        // track the current allocation in case of a following NO_OVERWRITE map
        ctx_state.current_dynamic_texture_allocations.insert_or_assign(dynamic.ptr(), new_allocation);
        // collect allocated buffers and recycle them when the command list is released
        ctx_state.current_cmdlist->used_dynamic_texture_allocations.push_back({dynamic.ptr(), new_allocation});
        Emit([allocation = new_allocation, texture = Rc(dynamic->texture)](ArgumentEncodingContext &enc) mutable {
          auto _ = texture->rename(forward_rc(allocation));
        });

        Out.pData = new_allocation->mappedMemory;
        Out.RowPitch = buffer_length;
        Out.DepthPitch = bind_flag;
        break;
      }
      case D3D11_MAP_WRITE_NO_OVERWRITE: {
        auto ret = ctx_state.current_dynamic_texture_allocations.find(dynamic.ptr());
        if (ret == ctx_state.current_dynamic_texture_allocations.end()) {
          ERR("DeferredContext: Invalid NO_OVERWRITE map on deferred context occurs without any prior DISCARD map.");
          return E_INVALIDARG;
        }
        Out.pData = ret->second->mappedMemory;
        Out.RowPitch = buffer_length;
        Out.DepthPitch = bind_flag;
        break;
      }
      }
      if (pMappedResource) {
        *pMappedResource = Out;
      }
      return S_OK;
    }
    return E_FAIL;
  }

  void
  Unmap(ID3D11Resource *pResource, UINT Subresource) override {
    UINT buffer_length = 0;
    UINT bind_flag = 0;
    if (auto dynamic = GetDynamicBuffer(pResource, &buffer_length, &bind_flag)) {
      return;
    }
    if (auto dynamic = GetDynamicTexture(pResource, &buffer_length, &bind_flag)) {
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
      auto building_query = ctx_state.building_visibility_queries.find(pAsync);
      if (building_query != ctx_state.building_visibility_queries.end()) {
        // need to figure out if it's the intended behavior
        D3D11_ASSERT(0 && "unexpected branch condition hit, please file an issue.");
        // Begin() after another Begin()
        Emit([query_id = building_query->second.second](ArgumentEncodingContext &enc) mutable {
          enc.endVisibilityResultQuery(enc.currentDeferredVisibilityQuery(query_id));
        });
        ctx_state.current_cmdlist->issued_visibility_query.push_back(std::move(building_query->second));
        ctx_state.building_visibility_queries.erase(building_query);
      }
      auto query_id = ctx_state.current_cmdlist->visibility_query_count++;
      Emit([=](ArgumentEncodingContext &enc) mutable {
        enc.beginVisibilityResultQuery(enc.currentDeferredVisibilityQuery(query_id));
      });
      ctx_state.building_visibility_queries.insert(
          {(void *)pAsync, {static_cast<IMTLD3DOcclusionQuery *>(pAsync), query_id}}
      );
      break;
    }
    case D3D11_QUERY_TIMESTAMP:
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      // ignore
      break;
    }
    default:
      ERR("DeferredContext: unhandled query type ", desc.Query);
      break;
    }
  }

  void
  End(ID3D11Asynchronous *pAsync) override {
    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT:
      promote_flush = true;
      ctx_state.current_cmdlist->issued_event_query.push_back(static_cast<IMTLD3DEventQuery *>(pAsync));
      break;
    case D3D11_QUERY_OCCLUSION:
    case D3D11_QUERY_OCCLUSION_PREDICATE: {
      auto building_query = ctx_state.building_visibility_queries.find(pAsync);
      if (building_query == ctx_state.building_visibility_queries.end()) {
        // need to figure out if it's the intended behavior
        D3D11_ASSERT(0 && "unexpected branch condition hit, please file an issue.");
        // no corresponding Begin()
        WARN("DeferredContext: An occclusion query End() is called without corresponding Begin()");
        return;
      }
      promote_flush = true;
      Emit([query_id = building_query->second.second](ArgumentEncodingContext &enc) mutable {
        enc.endVisibilityResultQuery(enc.currentDeferredVisibilityQuery(query_id));
      });
      ctx_state.current_cmdlist->issued_visibility_query.push_back(std::move(building_query->second));
      ctx_state.building_visibility_queries.erase(building_query);
      break;
    }
    case D3D11_QUERY_TIMESTAMP:
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      // ignore
      break;
    }
    default:
      ERR("DeferredContext: unhandled query type ", desc.Query);
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

    for (const auto &[_, building_query] : ctx_state.building_visibility_queries) {
      End(building_query.first.ptr());
    }
    ctx_state.building_visibility_queries.clear();
    D3D11_ASSERT(
        ctx_state.current_cmdlist->visibility_query_count == ctx_state.current_cmdlist->issued_visibility_query.size()
    );
    ctx_state.current_dynamic_buffer_allocations.clear();
    ctx_state.current_dynamic_texture_allocations.clear();

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