#include "d3d11_private.h"
#include "d3d11_query.hpp"
#include "dxmt_command_queue.hpp"
#include "d3d11_context_impl.cpp"
#include "dxmt_context.hpp"

namespace dxmt {
struct ContextInternalState {
  CommandQueue &cmd_queue;
};


template<typename Object> Rc<Object> forward_rc(Rc<Object>& obj) {
  return std::move(obj);
}

using ImmediateContextBase = MTLD3D11DeviceContextImplBase<ContextInternalState>;

template <>
template <CommandWithContext<ArgumentEncodingContext> cmd>
void
ImmediateContextBase:: Emit(cmd &&fn) {
  CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();
  chk->emitcc(std::forward<cmd>(fn));
}

template <>
template <typename T>
moveonly_list<T>
ImmediateContextBase::AllocateCommandData(size_t n) {
  CommandChunk *chk = ctx_state.cmd_queue.CurrentChunk();
  return moveonly_list<T>((T *)chk->allocate_cpu_heap(sizeof(T) * n, alignof(T)), n);
}

template <>
std::tuple<void *, MTL::Buffer *, uint64_t>
ImmediateContextBase::AllocateStagingBuffer(size_t size, size_t alignment) {
  return ctx_state.cmd_queue.AllocateStagingBuffer(size, alignment);
}

template <>
void
ImmediateContextBase::UseCopyDestination(
    IMTLD3D11Staging *pResource, uint32_t Subresource, MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
    uint32_t *pBytesPerImage
) {
  pResource->UseCopyDestination(
      Subresource, ctx_state.cmd_queue.CurrentSeqId(), pBuffer, pBytesPerRow, pBytesPerImage
  );
}
template <>
void
ImmediateContextBase::UseCopySource(
    IMTLD3D11Staging *pResource, uint32_t Subresource, MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
    uint32_t *pBytesPerImage
) {
  pResource->UseCopySource(
      Subresource, ctx_state.cmd_queue.CurrentSeqId(), pBuffer, pBytesPerRow, pBytesPerImage
  );
}

class MTLD3D11ImmediateContext : public ImmediateContextBase {
public:
  MTLD3D11ImmediateContext(MTLD3D11Device *pDevice, CommandQueue &cmd_queue) :
      ImmediateContextBase(pDevice, ctx_state),
      cmd_queue(cmd_queue),
      ctx_state({cmd_queue}) {}

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
    if (unlikely(!refCount))
      this->m_parent->Release();

    return refCount;
  }

  HRESULT
  Map(ID3D11Resource *pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags,
      D3D11_MAPPED_SUBRESOURCE *pMappedResource) override {
    if (auto dynamic = com_cast<IMTLDynamicBuffer>(pResource)) {
      D3D11_MAPPED_SUBRESOURCE Out;
      switch (MapType) {
      case D3D11_MAP_READ:
      case D3D11_MAP_WRITE:
      case D3D11_MAP_READ_WRITE:
        return E_INVALIDARG;
      case D3D11_MAP_WRITE_DISCARD: {
        dynamic->RotateBuffer(device);
        auto bind_flag = dynamic->GetBindFlag();
        // if (bind_flag & D3D11_BIND_VERTEX_BUFFER) {
        // FIXME: bind_flag can be inaccurate
          state_.InputAssembler.VertexBuffers.set_dirty();
        // }
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
        Out.pData = dynamic->GetMappedMemory(&Out.RowPitch, &Out.DepthPitch);
        if (dynamic->__isBuffer_dyn()) {
          Emit([buffer = dynamic->__buffer_dyn(),
                allocation = dynamic->__bufferAllocated()](ArgumentEncodingContext &enc) mutable {
            auto _ = buffer->rename(forward_rc(allocation));
          });
        } else {
          Emit([texture = dynamic->__texture_dyn(),
                allocation = dynamic->__textureAllocated()](ArgumentEncodingContext &enc) mutable {
            auto _ = texture->rename(forward_rc(allocation));
          });
        }
        break;
      }
      case D3D11_MAP_WRITE_NO_OVERWRITE: {
        Out.pData = dynamic->GetMappedMemory(&Out.RowPitch, &Out.DepthPitch);
        break;
      }
      }
      if (pMappedResource) {
        *pMappedResource = Out;
      }
      return S_OK;
    }
    if (auto staging = com_cast<IMTLD3D11Staging>(pResource)) {
      while (true) {
        auto coh = cmd_queue.CoherentSeqId();
        auto ret = staging->TryMap(Subresource, coh, MapType, pMappedResource);
        if (ret < 0) {
          return E_FAIL;
        }
        if (ret == 0) {
          TRACE("staging map ready");
          return S_OK;
        }
        if (MapFlags & D3D11_MAP_FLAG_DO_NOT_WAIT) {
          return DXGI_ERROR_WAS_STILL_DRAWING;
        }
        // even it's in a while loop
        // only the first flush will have effect
        // and the following calls are essentially no-op
        Flush();
        TRACE("staging map block");
        cmd_queue.FIXME_YieldUntilCoherenceBoundaryUpdate(coh);
      };
    };
    D3D11_ASSERT(0 && "unknown mapped resource (USAGE_DEFAULT?)");
  }

  void
  Unmap(ID3D11Resource *pResource, UINT Subresource) override {
    if (auto dynamic = com_cast<IMTLDynamicBuffer>(pResource)) {
      return;
    }
    if (auto staging = com_cast<IMTLD3D11Staging>(pResource)) {
      staging->Unmap(Subresource);
      return;
    };
    D3D11_ASSERT(0 && "unknown mapped resource (USAGE_DEFAULT?)");
  }

  void
  Begin(ID3D11Asynchronous *pAsync) override {
    // in theory pAsync could be any of them: { Query, Predicate, Counter }.
    // However `Predicate` and `Counter` are not supported at all
    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT:
      break;
    case D3D11_QUERY_OCCLUSION:
    case D3D11_QUERY_OCCLUSION_PREDICATE: {
      auto query = static_cast<IMTLD3DOcclusionQuery *>(pAsync);
      if (query->Begin())
        Emit([query_ = query->__query()](ArgumentEncodingContext &enc) mutable {
          enc.beginVisibilityResultQuery(std::move(query_));
        });
      break;
    }
    case D3D11_QUERY_TIMESTAMP:
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      // ignore
      break;
    }
    default:
      ERR("Unknown query type ", desc.Query);
      break;
    }
  }

  // See Begin()
  void
  End(ID3D11Asynchronous *pAsync) override {
    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT:
      ((IMTLD3DEventQuery *)pAsync)->Issue(cmd_queue.EncodedWorkFinishAt());
      break;
    case D3D11_QUERY_OCCLUSION:
    case D3D11_QUERY_OCCLUSION_PREDICATE: {
      auto query = static_cast<IMTLD3DOcclusionQuery *>(pAsync);
      if (query->End())
        Emit([qeury_ = query->__query()](ArgumentEncodingContext &enc) mutable {
          enc.endVisibilityResultQuery(std::move(qeury_));
        });
      promote_flush = true;
      break;
    }
    case D3D11_QUERY_TIMESTAMP:
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      // ignore
      break;
    }
    default:
      ERR("Unknown query type ", desc.Query);
      break;
    }
  }

  HRESULT
  GetData(ID3D11Asynchronous *pAsync, void *pData, UINT DataSize, UINT GetDataFlags) override {
    if (!pAsync || (DataSize && !pData))
      return E_INVALIDARG;

    // Allow dataSize to be zero
    if (DataSize && DataSize != pAsync->GetDataSize())
      return E_INVALIDARG;

    if (GetDataFlags != D3D11_ASYNC_GETDATA_DONOTFLUSH) {
      Flush();
    }

    D3D11_QUERY_DESC desc;
    ((ID3D11Query *)pAsync)->GetDesc(&desc);
    switch (desc.Query) {
    case D3D11_QUERY_EVENT: {
      BOOL null_data;
      BOOL *data_ptr = pData ? (BOOL *)pData : &null_data;
      return ((IMTLD3DEventQuery *)pAsync)->GetData(data_ptr, cmd_queue.CoherentSeqId());
    }
    case D3D11_QUERY_OCCLUSION: {
      uint64_t null_data;
      uint64_t *data_ptr = pData ? (uint64_t *)pData : &null_data;
      return ((IMTLD3DOcclusionQuery *)pAsync)->GetData(data_ptr);
    }
    case D3D11_QUERY_OCCLUSION_PREDICATE: {
      BOOL null_data;
      BOOL *data_ptr = pData ? (BOOL *)pData : &null_data;
      return ((IMTLD3DOcclusionQuery *)pAsync)->GetData(data_ptr);
    }
    case D3D11_QUERY_TIMESTAMP: {
      if (pData) {
        (*static_cast<uint64_t *>(pData)) = 0;
      }
      return S_OK;
    }
    case D3D11_QUERY_TIMESTAMP_DISJOINT: {
      if (pData) {
        (*static_cast<D3D11_QUERY_DATA_TIMESTAMP_DISJOINT *>(pData)) = {1, TRUE};
      }
      return S_OK;
    }
    default:
      ERR("Unknown query type ", desc.Query);
      return E_FAIL;
    }
  }

  void
  Flush() override {
    if (cmd_queue.CurrentChunk()->has_no_work_encoded_yet()) {
      return;
    }
    promote_flush = true;
    InvalidateCurrentPass();
  }

  void PrepareFlush() override {
    InvalidateCurrentPass(true);
  }

  void
  ExecuteCommandList(ID3D11CommandList *pCommandList, BOOL RestoreContextState) override {
    ResetEncodingContextState();

    Com<MTLD3D11CommandList> cmdlist = static_cast<MTLD3D11CommandList *>(pCommandList);

    promote_flush = cmdlist->promote_flush;

    // TODO: handle command list query & staging resource tracking

    Emit([cmdlist = std::move(cmdlist)](ArgumentEncodingContext& enc) {
      // Finished command list should clean up the encoding context
      cmdlist->Execute(enc);
    });

    if (RestoreContextState)
      RestoreEncodingContextState();
    else
      ResetD3D11ContextState();
  }

  HRESULT
  FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList **ppCommandList) override {
    return DXGI_ERROR_INVALID_CALL;
  }

  D3D11_DEVICE_CONTEXT_TYPE
  GetType() override {
    return D3D11_DEVICE_CONTEXT_IMMEDIATE;
  }

  virtual void
  WaitUntilGPUIdle() override {
    uint64_t seq = cmd_queue.CurrentSeqId();
    if(!InvalidateCurrentPass())
      Commit();
    cmd_queue.WaitCPUFence(seq);
  };

  void
  Commit() override {
    promote_flush = false;
    D3D11_ASSERT(cmdbuf_state == CommandBufferState::Idle);
    ctx_state.cmd_queue.CommitCurrentChunk();
  };

private:
  std::vector<Com<IMTLD3DOcclusionQuery>> pending_occlusion_queries;
  CommandQueue &cmd_queue;
  ContextInternalState ctx_state;
  std::atomic<uint32_t> refcount = 0;
};

std::unique_ptr<MTLD3D11DeviceContextBase>
InitializeImmediateContext(MTLD3D11Device *pDevice, CommandQueue &cmd_queue) {
  return std::make_unique<MTLD3D11ImmediateContext>(pDevice, cmd_queue);
}

}; // namespace dxmt