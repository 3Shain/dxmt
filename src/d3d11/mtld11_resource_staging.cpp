#include "d3d11_private.h"
#include "d3d11_enumerable.hpp"
#include "mtld11_resource.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

class StagingBufferInternal {
  Obj<MTL::Buffer> buffer;
  void *buffer_mapped;
  uint64_t buffer_len;
  uint32_t bytes_per_row;
  uint32_t bytes_per_image;
  // prevent read from staging before
  uint64_t cpu_coherent_after_finished_seq_id = 0;
  // prevent write to staging before
  uint64_t gpu_occupied_until_finished_seq_id = 0;
  bool mapped = false;

public:
  StagingBufferInternal(Obj<MTL::Buffer> &&buffer, uint32_t bytes_per_row,
                        uint32_t bytes_per_image)
      : buffer(std::move(buffer)), buffer_mapped(this->buffer->contents()),
        buffer_len(this->buffer->length()), bytes_per_row(bytes_per_row),
        bytes_per_image(bytes_per_image) {}

  bool UseCopyDestination(uint64_t seq_id, MTL_STAGING_RESOURCE *pBuffer,
                          uint32_t *pBytesPerRow, uint32_t *pBytesPerImage) {
    if (mapped) {
      return false;
    }
    cpu_coherent_after_finished_seq_id = seq_id; // coherent read-after-write
    gpu_occupied_until_finished_seq_id = seq_id; // coherent write-after-write
    pBuffer->Type = MTL_BIND_BUFFER_UNBOUNDED;
    pBuffer->Buffer = buffer.ptr();
    *pBytesPerRow = bytes_per_row;
    *pBytesPerImage = bytes_per_image;
    return true;
  };

  bool UseCopySource(uint64_t seq_id, MTL_STAGING_RESOURCE *pBuffer,
                     uint32_t *pBytesPerRow, uint32_t *pBytesPerImage) {
    if (mapped) {
      return false;
    }
    // read-after-read doesn't hurt anyway
    gpu_occupied_until_finished_seq_id = seq_id; // coherent write-after-read
    pBuffer->Type = MTL_BIND_BUFFER_UNBOUNDED;
    pBuffer->Buffer = buffer.ptr();
    *pBytesPerRow = bytes_per_row;
    *pBytesPerImage = bytes_per_image;
    return true;
  };

  int64_t TryMap(uint64_t finished_seq_id,
                 // uint64_t commited_seq_id,
                 D3D11_MAP flag, D3D11_MAPPED_SUBRESOURCE *pMappedResource) {
    if (mapped)
      return -1;
    if (flag == D3D11_MAP_READ) {
      if (finished_seq_id < cpu_coherent_after_finished_seq_id) {
        return cpu_coherent_after_finished_seq_id - finished_seq_id;
      }
    } else if (flag == D3D11_MAP_WRITE) {
      if (finished_seq_id < gpu_occupied_until_finished_seq_id) {
        return gpu_occupied_until_finished_seq_id - finished_seq_id;
      }
    } else if (flag == D3D11_MAP_READ_WRITE) {
      if (finished_seq_id < cpu_coherent_after_finished_seq_id) {
        return cpu_coherent_after_finished_seq_id - finished_seq_id;
      }
      if (finished_seq_id < gpu_occupied_until_finished_seq_id) {
        return gpu_occupied_until_finished_seq_id - finished_seq_id;
      }
    } else {
      return -1;
    }
    pMappedResource->pData = buffer_mapped;
    pMappedResource->RowPitch = bytes_per_row;
    pMappedResource->DepthPitch = bytes_per_image;
    return 0;
  };
  void Unmap() {
    if (!mapped)
      return;
    mapped = false;
  };
};

#pragma region StagingBuffer

class StagingBuffer : public TResourceBase<tag_buffer, IMTLD3D11Staging> {
private:
  StagingBufferInternal internal;

public:
  StagingBuffer(const tag_buffer::DESC_S *desc, IMTLD3D11Device *device,
                StagingBufferInternal &&internal)
      : TResourceBase<tag_buffer, IMTLD3D11Staging>(desc, device),
        internal(std::move(internal)) {}

  bool UseCopyDestination(uint32_t Subresource, uint64_t seq_id,
                          MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
                          uint32_t *pBytesPerImage) override {
    D3D11_ASSERT(Subresource == 0);
    return internal.UseCopyDestination(seq_id, pBuffer, pBytesPerRow,
                                       pBytesPerImage);
  };

  bool UseCopySource(uint32_t Subresource, uint64_t seq_id,
                     MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
                     uint32_t *pBytesPerImage) override {
    D3D11_ASSERT(Subresource == 0);
    return internal.UseCopySource(seq_id, pBuffer, pBytesPerRow,
                                  pBytesPerImage);
  };

  virtual int64_t TryMap(uint32_t Subresource, uint64_t finished_seq_id,
                         // uint64_t commited_seq_id,
                         D3D11_MAP flag,
                         D3D11_MAPPED_SUBRESOURCE *pMappedResource) override {
    return internal.TryMap(finished_seq_id, flag, pMappedResource);
  };
  virtual void Unmap(uint32_t Subresource) override {
    return internal.Unmap();
  };
};

HRESULT
CreateStagingBuffer(IMTLD3D11Device *pDevice, const D3D11_BUFFER_DESC *pDesc,
                    const D3D11_SUBRESOURCE_DATA *pInitialData,
                    ID3D11Buffer **ppBuffer) {
  auto metal = pDevice->GetMTLDevice();
  auto byte_width = pDesc->ByteWidth;
  auto buffer =
      transfer(metal->newBuffer(byte_width, MTL::ResourceStorageModeShared));
  if (pInitialData) {
    memcpy(buffer->contents(), pInitialData->pSysMem, byte_width);
  }
  *ppBuffer = ref(new StagingBuffer(
      pDesc, pDevice,
      StagingBufferInternal(std::move(buffer), byte_width, byte_width)));
  return S_OK;
}

#pragma endregion

#pragma region StagingTexture

template <typename tag_texture>
class StagingTexture : public TResourceBase<tag_texture, IMTLD3D11Staging> {
  std::vector<StagingBufferInternal> subresources;
  uint32_t subresource_count;

public:
  StagingTexture(const tag_texture::DESC_S *pDesc, IMTLD3D11Device *pDevice,
                 std::vector<StagingBufferInternal> &&subresources)
      : TResourceBase<tag_texture, IMTLD3D11Staging>(pDesc, pDevice),
        subresources(std::move(subresources)),
        subresource_count(this->subresources.size()) {}

  bool UseCopyDestination(uint32_t Subresource, uint64_t seq_id,
                          MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
                          uint32_t *pBytesPerImage) override {
    D3D11_ASSERT(Subresource < subresource_count);
    return subresources.at(Subresource)
        .UseCopyDestination(seq_id, pBuffer, pBytesPerRow, pBytesPerImage);
  };

  bool UseCopySource(uint32_t Subresource, uint64_t seq_id,
                     MTL_STAGING_RESOURCE *pBuffer, uint32_t *pBytesPerRow,
                     uint32_t *pBytesPerImage) override {
    D3D11_ASSERT(Subresource < subresource_count);
    return subresources.at(Subresource)
        .UseCopySource(seq_id, pBuffer, pBytesPerRow, pBytesPerImage);
  };

  virtual int64_t TryMap(uint32_t Subresource, uint64_t finished_seq_id,
                         // uint64_t commited_seq_id,
                         D3D11_MAP flag,
                         D3D11_MAPPED_SUBRESOURCE *pMappedResource) override {
    if (Subresource >= subresource_count) {
      ERR("Todo: ...");
      return -1;
    }
    return subresources.at(Subresource)
        .TryMap(finished_seq_id, flag, pMappedResource);
  };
  virtual void Unmap(uint32_t Subresource) override {
    if (Subresource >= subresource_count) {
      return;
    }
    return subresources.at(Subresource).Unmap();
  };
};

#pragma endregion

template <typename tag>
HRESULT CreateStagingTextureInternal(IMTLD3D11Device *pDevice,
                                     const typename tag::DESC_S *pDesc,
                                     const D3D11_SUBRESOURCE_DATA *pInitialData,
                                     typename tag::COM **ppTexture) {
  auto metal = pDevice->GetMTLDevice();
  typename tag::DESC_S finalDesc;
  Obj<MTL::TextureDescriptor> texDesc; // unused
  // clang-format, why do you piss me off
  // is this really expected to be read by human?
  if (FAILED(
          CreateMTLTextureDescriptor(pDevice, pDesc, &finalDesc, &texDesc))) {
    return E_INVALIDARG;
  }
  std::vector<StagingBufferInternal> subresources;
  D3D11_ASSERT(!pInitialData);
  for (auto &sub : EnumerateSubresources(finalDesc)) {
    uint32_t w, h, d;
    GetMipmapSize(&finalDesc, sub.MipLevel, &w, &h, &d);
    uint32_t bpr, bpi, buf_len;
    if (FAILED(GetLinearTextureLayout(pDevice, &finalDesc, sub.MipLevel, &bpr,
                                      &bpi, &buf_len))) {
      return E_FAIL;
    }
    D3D11_ASSERT(subresources.size() == sub.SubresourceId);
    auto buffer =
        transfer(metal->newBuffer(buf_len, MTL::ResourceStorageModeShared));
    if (pInitialData) {
      // FIXME: SysMemPitch and SysMemSlicePitch should be respected!
      memcpy(buffer->contents(), pInitialData[sub.SubresourceId].pSysMem,
             buf_len);
    }
    subresources.push_back(StagingBufferInternal(std::move(buffer), bpr, bpi));
  }

  *ppTexture = ref(
      new StagingTexture<tag>(&finalDesc, pDevice, std::move(subresources)));
  return S_OK;
}

HRESULT
CreateStagingTexture1D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE1D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture1D **ppTexture) {
  return CreateStagingTextureInternal<tag_texture_1d>(pDevice, pDesc,
                                                      pInitialData, ppTexture);
}

HRESULT
CreateStagingTexture2D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE2D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture2D **ppTexture) {
  return CreateStagingTextureInternal<tag_texture_2d>(pDevice, pDesc,
                                                      pInitialData, ppTexture);
}

HRESULT
CreateStagingTexture3D(IMTLD3D11Device *pDevice,
                       const D3D11_TEXTURE3D_DESC *pDesc,
                       const D3D11_SUBRESOURCE_DATA *pInitialData,
                       ID3D11Texture3D **ppTexture) {
  return CreateStagingTextureInternal<tag_texture_3d>(pDevice, pDesc,
                                                      pInitialData, ppTexture);
}

} // namespace dxmt