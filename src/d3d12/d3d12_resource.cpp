#include "d3d12_resource.hpp"
#include "d3d12_device.hpp"
#include "d3d12_pipeline_state.hpp"
#include "log/log.hpp"
#include "util_string.hpp"

#define RTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, "Resource::" fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

namespace dxmt {

MTLD3D12Resource::MTLD3D12Resource(
    MTLD3D12Device *device, const D3D12_RESOURCE_DESC &desc,
    D3D12_RESOURCE_STATES initial_state,
    D3D12_HEAP_PROPERTIES heap_properties)
    : m_device(device), m_desc(desc), m_state(initial_state),
      m_heap_properties(heap_properties) {
  m_device->AddRef();

  auto wmt_device = m_device->GetDXMTDevice().device();
  RTRACE("ctor: wmt_device=%llu dim=%u fmt=%u w=%llu h=%u", (unsigned long long)wmt_device.handle, desc.Dimension, desc.Format, desc.Width, desc.Height);

  if (desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER) {
    WMTBufferInfo buf_info = {};
    buf_info.length = desc.Width ? desc.Width : 256;
    buf_info.options = WMTResourceStorageModeShared;
    m_mtl_buffer = wmt_device.newBuffer(buf_info);
    m_cpu_addr = buf_info.memory.get_accessible_or_null();
    m_gpu_addr = buf_info.gpu_address;
    m_buf_info = buf_info;
    RTRACE("ctor: buffer cpu=%p gpu=0x%llx len=%llu", m_cpu_addr, (unsigned long long)m_gpu_addr, (unsigned long long)desc.Width);
  } else {
    bool cpu_accessible = (heap_properties.Type == D3D12_HEAP_TYPE_UPLOAD ||
                           heap_properties.Type == D3D12_HEAP_TYPE_READBACK);
    WMTTextureInfo tex_info = {};
    tex_info.width = desc.Width;
    tex_info.height = desc.Height;
    tex_info.depth = (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
                         ? desc.DepthOrArraySize
                         : 1;
    tex_info.array_length =
        (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
            ? desc.DepthOrArraySize
            : 1;
    tex_info.mipmap_level_count = desc.MipLevels ? desc.MipLevels : 1;
    tex_info.sample_count = desc.SampleDesc.Count ? desc.SampleDesc.Count : 1;
    tex_info.type = WMTTextureType2D;
    tex_info.usage = (WMTTextureUsage)(WMTTextureUsageRenderTarget |
                                      WMTTextureUsageShaderRead);
    tex_info.options = cpu_accessible ? WMTResourceStorageModeShared : WMTResourceStorageModePrivate;
    tex_info.pixel_format = MTLD3D12PipelineState::DXGIToMTLPixelFormat(static_cast<DXGI_FORMAT>(desc.Format));
    if (tex_info.pixel_format == WMTPixelFormatInvalid)
      tex_info.pixel_format = WMTPixelFormatBGRA8Unorm;

    if (cpu_accessible) {
      m_mtl_texture = wmt_device.newTexture(tex_info);
      RTRACE("ctor: texture CPU-accessible fmt=%u %llux%u", tex_info.pixel_format, tex_info.width, tex_info.height);
    } else {
      m_mtl_texture = {}; // lazy creation
      RTRACE("ctor: texture deferred (lazy) fmt=%u %llux%u", tex_info.pixel_format, tex_info.width, tex_info.height);
    }
  }

  Logger::info(str::format("D3D12Resource: dim=", desc.Dimension,
                            " ", desc.Width, "x", desc.Height,
                            " gpu=", m_gpu_addr));
  m_device->RegisterResource(this);
}

WMT::Reference<WMT::Texture> MTLD3D12Resource::GetMTLTexture() {
  if (!m_mtl_texture.handle && m_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) {
    bool cpu_accessible = (m_heap_properties.Type == D3D12_HEAP_TYPE_UPLOAD ||
                           m_heap_properties.Type == D3D12_HEAP_TYPE_READBACK);
    auto wmt_device = m_device->GetDXMTDevice().device();
    WMTTextureInfo tex_info = {};
    tex_info.width = m_desc.Width ? m_desc.Width : 1;
    tex_info.height = m_desc.Height ? m_desc.Height : 1;
    tex_info.depth = (m_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
                          ? m_desc.DepthOrArraySize : 1;
    tex_info.array_length = (m_desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
                                  ? m_desc.DepthOrArraySize : 1;
    tex_info.mipmap_level_count = m_desc.MipLevels ? m_desc.MipLevels : 1;
    tex_info.sample_count = m_desc.SampleDesc.Count ? m_desc.SampleDesc.Count : 1;
    tex_info.type = WMTTextureType2D;
    tex_info.usage = (WMTTextureUsage)(WMTTextureUsageRenderTarget | WMTTextureUsageShaderRead);
    tex_info.options = cpu_accessible ? WMTResourceStorageModeShared : WMTResourceStorageModePrivate;
    tex_info.pixel_format = MTLD3D12PipelineState::DXGIToMTLPixelFormat(static_cast<DXGI_FORMAT>(m_desc.Format));
    if (tex_info.pixel_format == WMTPixelFormatInvalid)
      tex_info.pixel_format = WMTPixelFormatBGRA8Unorm;
    RTRACE("GetMTLTexture: creating fmt=%u %llux%u", tex_info.pixel_format, tex_info.width, tex_info.height);
    m_mtl_texture = wmt_device.newTexture(tex_info);
    RTRACE("GetMTLTexture: handle=%llu", (unsigned long long)m_mtl_texture.handle);
  }
  return m_mtl_texture;
}

MTLD3D12Resource::~MTLD3D12Resource() {
  m_device->UnregisterResource(this);
  m_mtl_buffer = nullptr;
  m_mtl_texture = nullptr;
  m_device->Release();
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Resource::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Pageable ||
      riid == IID_ID3D12Resource) {
    *ppvObject = ref(this);
    return S_OK;
  }
  RTRACE("QI unknown IID %s -> E_NOINTERFACE", str::format(riid).c_str());
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE MTLD3D12Resource::AddRef() { return ++m_refCount; }

ULONG STDMETHODCALLTYPE MTLD3D12Resource::Release() {
  uint32_t rc = --m_refCount;
  if (!rc) {
    uint32_t rp = --m_refPrivate;
    if (!rp) {
      m_refPrivate += 0x80000000;
      delete this;
    }
  }
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Resource::GetPrivateData(REFGUID guid, UINT *data_size, void *data) {
  RTRACE("GetPrivateData E_NOTIMPL");
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Resource::SetPrivateData(REFGUID guid, UINT data_size,
                                 const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Resource::SetPrivateDataInterface(REFGUID guid, const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Resource::SetName(LPCWSTR name) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Resource::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}
HRESULT STDMETHODCALLTYPE
MTLD3D12Resource::Map(UINT sub_resource,
                                                 const D3D12_RANGE *read_range,
                                                 void **data) {
  RTRACE("Map sub=%u", sub_resource);
  if (!data)
    return E_POINTER;
  if (m_desc.Dimension > D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
    RTRACE("Map: invalid dimension, returning fake pointer");
    *data = (void*)1;
    return S_OK;
  }
  if (m_cpu_addr) {
    *data = m_cpu_addr;
    RTRACE("Map returning cpu_addr=%p gpu_addr=0x%llx", m_cpu_addr, (unsigned long long)m_gpu_addr);
    return S_OK;
  }
  RTRACE("Map FAILED - no cpu_addr");
  return E_FAIL;
}

void STDMETHODCALLTYPE MTLD3D12Resource::Unmap(
    UINT sub_resource, const D3D12_RANGE *written_range) {}

D3D12_RESOURCE_DESC *STDMETHODCALLTYPE
MTLD3D12Resource::GetDesc(D3D12_RESOURCE_DESC *__ret) {
  *__ret = m_desc;
  return __ret;
}

D3D12_GPU_VIRTUAL_ADDRESS STDMETHODCALLTYPE
MTLD3D12Resource::GetGPUVirtualAddress() {
  RTRACE("GetGPUVirtualAddress -> 0x%llx", (unsigned long long)m_gpu_addr);
  return m_gpu_addr;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Resource::WriteToSubresource(
    UINT dst_sub_resource, const D3D12_BOX *dst_box, const void *src_data,
    UINT src_row_pitch, UINT src_slice_pitch) {
  RTRACE("WriteToSubresource sub=%u box=%p", dst_sub_resource, dst_box);
  if (!src_data)
    return E_POINTER;
  if (m_desc.Dimension > D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
    return S_OK;
  }
  if (m_cpu_addr) {
    if (dst_box) {
      UINT rows = dst_box->bottom - dst_box->top;
      UINT depth = dst_box->back - dst_box->front;
      for (UINT z = 0; z < depth; z++) {
        for (UINT y = 0; y < rows; y++) {
          memcpy((char *)m_cpu_addr + (dst_box->front + z) * src_slice_pitch + (dst_box->top + y) * src_row_pitch + dst_box->left,
                 (char *)src_data + z * src_slice_pitch + y * src_row_pitch,
                 dst_box->right - dst_box->left);
        }
      }
    } else {
      memcpy(m_cpu_addr, src_data, src_slice_pitch ? src_slice_pitch : src_row_pitch);
    }
    return S_OK;
  }
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12Resource::ReadFromSubresource(
    void *dst_data, UINT dst_row_pitch, UINT dst_slice_pitch,
    UINT src_sub_resource, const D3D12_BOX *src_box) {
  void *vtable = *(void**)this;
  RTRACE("ReadFromSubresource dst=%p row_pitch=%u slice_pitch=%u sub=%u box=%p dim=%u this=%p vtable=%p", dst_data, dst_row_pitch, dst_slice_pitch, src_sub_resource, src_box, m_desc.Dimension, (void*)this, vtable);
  if (!dst_data)
    return E_POINTER;
  if (m_desc.Dimension > D3D12_RESOURCE_DIMENSION_TEXTURE3D) {
    RTRACE("ReadFromSubresource: invalid dimension %u, this=%p is NOT a resource! Skipping.", m_desc.Dimension, (void*)this);
    return S_OK;
  }
  if (m_cpu_addr) {
    UINT rows = m_desc.Height ? m_desc.Height : 1;
    if (src_box) {
      UINT copy_rows = src_box->bottom - src_box->top;
      UINT copy_depth = src_box->back - src_box->front;
      UINT copy_width = src_box->right - src_box->left;
      for (UINT z = 0; z < copy_depth; z++) {
        for (UINT y = 0; y < copy_rows; y++) {
          memcpy((char *)dst_data + z * dst_slice_pitch + y * dst_row_pitch,
                 (char *)m_cpu_addr + (src_box->front + z) * rows * dst_row_pitch + (src_box->top + y) * dst_row_pitch + src_box->left,
                 copy_width);
        }
      }
    } else {
      memcpy(dst_data, m_cpu_addr, dst_slice_pitch ? dst_slice_pitch : dst_row_pitch);
    }
    return S_OK;
  }
  if (m_desc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER) {
    UINT total = dst_slice_pitch ? dst_slice_pitch : dst_row_pitch * (m_desc.Height ? m_desc.Height : 1);
    if (total) memset(dst_data, 0, total);
    return S_OK;
  }
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12Resource::GetHeapProperties(D3D12_HEAP_PROPERTIES *heap_properties,
                                    D3D12_HEAP_FLAGS *flags) {
  if (heap_properties)
    *heap_properties = m_heap_properties;
  if (flags)
    *flags = D3D12_HEAP_FLAG_NONE;
  return S_OK;
}

} // namespace dxmt
