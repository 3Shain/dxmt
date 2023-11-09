#pragma once
#include "Foundation/NSAutoreleasePool.hpp"
#include "Metal/MTLBuffer.hpp"
#include "Metal/MTLResource.hpp"
#include "mtld11_resource.hpp"

namespace dxmt {

class StagingBuffer {
public:
  static const D3D11_RESOURCE_DIMENSION Dimension =
      D3D11_RESOURCE_DIMENSION_BUFFER;
  static const D3D11_USAGE Usage = D3D11_USAGE_STAGING;
  typedef D3D11_BUFFER_DESC Description;
  typedef ID3D11Buffer Interface;
  typedef void *Data;

  IMTLD3D11Device *__device__;
  ID3D11Buffer *__resource__;

  StagingBuffer(IMTLD3D11Device *pDevice, Data, const Description *desc,
                const D3D11_SUBRESOURCE_DATA *pInit) {
    if (pInit) {
      staging_buffer_ = pDevice->GetMTLDevice()->newBuffer(
          pInit->pSysMem, desc->ByteWidth, MTL::ResourceStorageModeShared);
    } else {
      staging_buffer_ = pDevice->GetMTLDevice()->newBuffer(
          desc->ByteWidth, MTL::ResourceStorageModeShared);
    }
  }

  HRESULT CreateShaderResourceView(const D3D11_SHADER_RESOURCE_VIEW_DESC *desc,
                                   ID3D11ShaderResourceView **ppView) {
    return 0;
  }

  Obj<MTL::Buffer> staging_buffer_;
};

class ImmutableBuffer {
public:
  static const D3D11_RESOURCE_DIMENSION Dimension =
      D3D11_RESOURCE_DIMENSION_BUFFER;
  static const D3D11_USAGE Usage = D3D11_USAGE_IMMUTABLE;
  typedef D3D11_BUFFER_DESC Description;
  typedef ID3D11Buffer Interface;
  typedef void *Data;

  IMTLD3D11Device *__device__;

  ImmutableBuffer(IMTLD3D11Device *pDevice, Data data, const Description *desc,
                  const D3D11_SUBRESOURCE_DATA *pInit) {
    buffer_ = transfer(pDevice->GetMTLDevice()->newBuffer(
        pInit->pSysMem, desc->ByteWidth, MTL::ResourceStorageModeShared));
  }

  Obj<MTL::Buffer> buffer_;
  bool initailzed = false;

  VertexBufferBinding *BindAsVertexBuffer(DXMTCommandStream *cs) {
    if (!initailzed) {
      auto ondeviceBuffer = transfer(__device__->GetMTLDevice()->newBuffer(
          buffer_->length(), MTL::ResourceStorageModePrivate));
      ;
      cs->EmitPreCommand(
          MTLInitializeBufferCommand(std::move(buffer_), ondeviceBuffer.ptr()));
      // TODO: use compare exchange for thread safety? is it really necessary..
      initailzed = true;
      buffer_ = std::move(ondeviceBuffer);
    }
    return new SimpleVertexBufferBinding(buffer_.ptr());
  };

  IndexBufferBinding *BindAsIndexBuffer(DXMTCommandStream *cs) {
    if (!initailzed) {
      auto ondeviceBuffer = transfer(__device__->GetMTLDevice()->newBuffer(
          buffer_->length(), MTL::ResourceStorageModePrivate));
      cs->EmitPreCommand(
          MTLInitializeBufferCommand(std::move(buffer_), ondeviceBuffer.ptr()));
      // TODO: use compare exchange for thread safety? is it really necessary..
      initailzed = true;
      buffer_ = std::move(ondeviceBuffer);
    }
    return new SimpleIndexBufferBinding(buffer_.ptr());
  };
};

class DynamicBuffer {
public:
  static const D3D11_RESOURCE_DIMENSION Dimension =
      D3D11_RESOURCE_DIMENSION_BUFFER;
  static const D3D11_USAGE Usage = D3D11_USAGE_STAGING;
  typedef D3D11_BUFFER_DESC Description;
  typedef ID3D11Buffer Interface;
  typedef void *Data;

  IMTLD3D11Device *__device__;
  ID3D11Buffer *__resource__;

  DynamicBuffer(IMTLD3D11Device *pDevice, Data, const Description *desc,
                const D3D11_SUBRESOURCE_DATA *pInit) {
    if (pInit) {
      buffer_ = transfer(pDevice->GetMTLDevice()->newBuffer(
          pInit->pSysMem, desc->ByteWidth, MTL::ResourceStorageModeShared));
    } else {
      buffer_ = transfer(pDevice->GetMTLDevice()->newBuffer(
          desc->ByteWidth, MTL::ResourceStorageModeShared));
    }
  }

  ConstantBufferBinding *BindAsConstantBuffer(DXMTCommandStream *cs) {
    // return new SimpleConstantBufferBinding(buffer_.ptr());

    // are you sure? TODO: inspect this!
    return new SimpleLazyConstantBufferBinding(
        [&]() {
          auto c = this->buffer_.ptr();
          return c;
        },
        this);
  };

  HRESULT MapDiscard(uint32_t subresource, MappedResource *mappedResource,
                     DXMTCommandStream *cs) {
    if (commited) {
      // rotate buffer;
      buffer_ = transfer(__device__->GetMTLDevice()->newBuffer(
          buffer_->length(), MTL::ResourceStorageModeShared |
                                 MTL::ResourceCPUCacheModeWriteCombined |
                                 MTL::ResourceHazardTrackingModeUntracked));
      cs->Emit(MTLBufferRotated(this, buffer_.ptr()));
    }
    mappedResource->pData = buffer_->contents();
    return S_OK;
  };

  HRESULT Unmap(uint32_t subresource) {
    commited = true;
    return S_OK;
  }

  Obj<MTL::Buffer> buffer_;
  bool commited = true;
};

} // namespace dxmt