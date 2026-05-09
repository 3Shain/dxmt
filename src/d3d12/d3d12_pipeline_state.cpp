#include "d3d12_pipeline_state.hpp"
#include "d3d12_device.hpp"
#include "log/log.hpp"
#include "util_string.hpp"
#include "Metal.hpp"
#include "airconv_public.h"
#include "dxmt_format.hpp"
#include "dxil/dxil_container.hpp"
#include "dxil/llvm_bitcode.hpp"
#include "dxil/dxil_to_msl.hpp"
#include "../../libs/DXBCParser/BlobContainer.h"
#include <cstring>
#include <unistd.h>
#include <vector>
#include <process.h>

#define PSTRACE(fmt, ...) do { FILE *_tf = fopen("Z:\\tmp\\dxmt_dxgi_trace.log", "a"); if (_tf) { fprintf(_tf, fmt "\n", ##__VA_ARGS__); fclose(_tf); } } while(0)

namespace dxmt {

MTLD3D12PipelineState::MTLD3D12PipelineState(MTLD3D12Device *device,
                                             bool is_compute)
    : m_device(device), m_is_compute(is_compute) {
  m_device->AddRef();
}

MTLD3D12PipelineState::~MTLD3D12PipelineState() {
  if (m_root_sig)
    m_root_sig->Release();
  m_render_pso = nullptr;
  m_compute_pso = nullptr;
  m_device->Release();
}

WMTPixelFormat MTLD3D12PipelineState::DXGIToMTLPixelFormat(DXGI_FORMAT format) {
  switch (format) {
  case DXGI_FORMAT_R8G8B8A8_UNORM: return WMTPixelFormatRGBA8Unorm;
  case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return WMTPixelFormatRGBA8Unorm_sRGB;
  case DXGI_FORMAT_B8G8R8A8_UNORM: return WMTPixelFormatBGRA8Unorm;
  case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return WMTPixelFormatBGRA8Unorm_sRGB;
  case DXGI_FORMAT_R16G16B16A16_FLOAT: return WMTPixelFormatRGBA16Float;
  case DXGI_FORMAT_R32G32B32A32_FLOAT: return WMTPixelFormatRGBA32Float;
  case DXGI_FORMAT_R10G10B10A2_UNORM: return WMTPixelFormatRGB10A2Unorm;
  case DXGI_FORMAT_R11G11B10_FLOAT: return WMTPixelFormatRG11B10Float;
  case DXGI_FORMAT_R8_UNORM: return WMTPixelFormatR8Unorm;
  case DXGI_FORMAT_R16_FLOAT: return WMTPixelFormatR16Float;
  case DXGI_FORMAT_R32_FLOAT: return WMTPixelFormatR32Float;
  case DXGI_FORMAT_D32_FLOAT: return WMTPixelFormatDepth32Float;
  case DXGI_FORMAT_D24_UNORM_S8_UINT: return WMTPixelFormatDepth24Unorm_Stencil8;
  case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return WMTPixelFormatDepth32Float_Stencil8;
  case DXGI_FORMAT_D16_UNORM: return WMTPixelFormatDepth16Unorm;
  case DXGI_FORMAT_R16G16_FLOAT: return WMTPixelFormatRG16Float;
  case DXGI_FORMAT_R16G16_UNORM: return WMTPixelFormatRG16Unorm;
  case DXGI_FORMAT_R8G8_UNORM: return WMTPixelFormatRG8Unorm;
  default: return WMTPixelFormatInvalid;
  }
}

bool MTLD3D12PipelineState::CompileShader(const void *bytecode, SIZE_T size,
                                          ShaderType type,
                                          const char *func_name,
                                          WMT::Reference<WMT::Function> &out_func) {
  if (bytecode && size >= 4) {
    auto *magic = (const uint32_t *)bytecode;
    PSTRACE("CompileShader: %s size=%zu magic=0x%08x (DXBC=0x43425844 DXIL=0x4C495844)", func_name, size, *magic);
    if (*magic == 0x43425844 && size >= 20) {
      auto *chunks = (const uint32_t *)bytecode;
      uint32_t num_chunks = chunks[4];
      PSTRACE("  DXBC: num_chunks=%u", num_chunks);
      for (uint32_t i = 0; i < num_chunks && i < 16; i++) {
        uint32_t offset = chunks[5 + i];
        if (offset + 8 <= size) {
          char tag[5] = {};
          memcpy(tag, (const char *)bytecode + offset, 4);
          uint32_t chunk_size = *((const uint32_t *)bytecode + offset/4 + 1);
          PSTRACE("  chunk[%u]: tag='%s' offset=%u size=%u", i, tag, offset, chunk_size);
        }
      }
    }
  }
  sm50_error_t sm50_err = nullptr;
  sm50_shader_t shader = nullptr;
  MTL_SHADER_REFLECTION reflection = {};

  if (SM50Initialize(bytecode, size, &shader, &reflection, &sm50_err)) {
    char err_buf[256] = {};
    SM50GetErrorMessage(sm50_err, err_buf, sizeof(err_buf));
    SM50FreeError(sm50_err);

    bool has_dxil = false;
    using namespace microsoft;
    CDXBCParser dxbcParser;
    if (SUCCEEDED(dxbcParser.ReadDXBC(bytecode, size))) {
      for (UINT32 i = 0; i < dxbcParser.GetBlobCount(); i++) {
        if (dxbcParser.GetBlobFourCC(i) == dxmt::dxil::DXIL_FOURCC) {
          has_dxil = true;
          const void *blob = dxbcParser.GetBlob(i);
          UINT32 blob_size = dxbcParser.GetBlobSize(i);
          PSTRACE("DXIL blob found index=%u size=%u", i, blob_size);

          auto wmt_device = m_device->GetDXMTDevice().device();

          char tmp_dxil[] = "/tmp/dxmt_dxil_XXXXXX";
          char tmp_metallib[] = "/tmp/dxmt_dxil_lib_XXXXXX";
          int fd_dxil = mkstemp(tmp_dxil);
          int fd_metallib = mkstemp(tmp_metallib);
          if (fd_dxil >= 0 && fd_metallib >= 0) {
            write(fd_dxil, bytecode, size);
            close(fd_dxil);
            close(fd_metallib);

            char win_dxil[256], win_metallib[256];
            snprintf(win_dxil, sizeof(win_dxil), "Z:\\tmp\\%s", strrchr(tmp_dxil, '/') + 1);
            snprintf(win_metallib, sizeof(win_metallib), "Z:\\tmp\\%s", strrchr(tmp_metallib, '/') + 1);

            intptr_t pid = _spawnlp(_P_WAIT, "C:\\windows\\system32\\metal-shaderconverter.exe",
              "metal-shaderconverter",
              "--output-reflection-file=Z:\\tmp\\dxmt_msc_reflection.json",
              "-o", win_metallib, win_dxil, NULL);
            int rc = (int)pid;
            PSTRACE("  metal-shaderconverter rc=%d", rc);

            if (rc == 0) {
              FILE *f = fopen(tmp_metallib, "rb");
              if (f) {
                fseek(f, 0, SEEK_END);
                long lib_size = ftell(f);
                fseek(f, 0, SEEK_SET);
                PSTRACE("  metallib size=%ld", lib_size);
                if (lib_size > 0) {
                  std::vector<uint8_t> lib_data(lib_size);
                  fread(lib_data.data(), 1, lib_size, f);
                  fclose(f);
                  auto dispatch_data = WMT::MakeDispatchData(lib_data.data(), lib_size);
                  WMT::Reference<WMT::Error> err;
                  auto library = wmt_device.newLibrary(dispatch_data, err);
                  if (err.handle) {
                    PSTRACE("  WMT newLibrary FAILED");
                  } else {
                    char actual_entry[256] = {};
                    FILE *rf = fopen("Z:\\tmp\\dxmt_msc_reflection.json", "r");
                    if (rf) {
                      char rbuf[4096] = {};
                      fread(rbuf, 1, sizeof(rbuf)-1, rf);
                      fclose(rf);
                      char *ep = strstr(rbuf, "\"EntryPoint\"");
                      if (ep) {
                        char *q1 = strchr(ep + 13, '"');
                        char *q2 = q1 ? strchr(q1+1, '"') : nullptr;
                        if (q1 && q2) {
                          size_t len = q2 - q1 - 1;
                          if (len < sizeof(actual_entry)) {
                            memcpy(actual_entry, q1+1, len);
                            actual_entry[len] = 0;
                          }
                        }
                      }
                      unlink("Z:\\tmp\\dxmt_msc_reflection.json");
                    }
                    const char *fn_name = actual_entry[0] ? actual_entry : func_name;
                    PSTRACE("  trying newFunction(%s)", fn_name);
                    out_func = library.newFunction(fn_name);
                    if (!out_func.handle && actual_entry[0]) {
                      PSTRACE("  retry with func_name=%s", func_name);
                      out_func = library.newFunction(func_name);
                    }
                    if (out_func.handle) {
                      PSTRACE("  DXIL compiled via metal-shaderconverter OK! entry=%s", fn_name);
                      unlink(tmp_dxil);
                      unlink(tmp_metallib);
                      return true;
                    } else {
                      PSTRACE("  WMT newFunction returned null for both names");
                    }
                  }
                } else {
                  fclose(f);
                }
              }
            } else {
              FILE *ef = fopen("/tmp/dxmt_msc_stderr.log", "r");
              if (ef) {
                char errbuf[256] = {};
                fgets(errbuf, sizeof(errbuf), ef);
                PSTRACE("  MSC stderr: %s", errbuf);
                fclose(ef);
              }
            }
            unlink(tmp_dxil);
            unlink(tmp_metallib);
          }
          break;
        }
      }
    }
    if (!has_dxil) {
      PSTRACE("SM50Init FAILED for %s: %s (no DXIL chunk)", func_name, err_buf);
    }
    return false;
  }

  SM50_SHADER_COMMON_DATA common = {};
  common.next = nullptr;
  common.type = SM50_SHADER_COMMON;
  common.metal_version = SM50_SHADER_METAL_310;
  common.flags = {};

  sm50_bitcode_t compile_result = nullptr;
  if (SM50Compile(shader, (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&common,
                  func_name, &compile_result, &sm50_err)) {
    char err_buf[256] = {};
    SM50GetErrorMessage(sm50_err, err_buf, sizeof(err_buf));
    Logger::err(str::format("SM50Compile failed for ", func_name, ": ", err_buf));
    SM50FreeError(sm50_err);
    SM50Destroy(shader);
    return false;
  }

  SM50_COMPILED_BITCODE bitcode = {};
  SM50GetCompiledBitcode(compile_result, &bitcode);

  auto wmt_device = m_device->GetDXMTDevice().device();
  WMT::Reference<WMT::Error> err;
  auto lib_data = WMT::MakeDispatchData(bitcode.Data, bitcode.Size);
  auto library = wmt_device.newLibrary(lib_data, err);

  if (err.handle) {
    Logger::err(str::format("Failed to create Metal library for ", func_name));
    SM50DestroyBitcode(compile_result);
    SM50Destroy(shader);
    return false;
  }

  out_func = library.newFunction(func_name);
  SM50DestroyBitcode(compile_result);
  SM50Destroy(shader);

  if (!out_func.handle) {
    Logger::err(str::format("Failed to get function ", func_name));
    return false;
  }

  Logger::info(str::format("  Compiled ", func_name, " OK"));
  return true;
}

bool MTLD3D12PipelineState::Compile() {
  if (m_compiled)
    return true;

  auto wmt_device = m_device->GetDXMTDevice().device();
  WMT::Reference<WMT::Error> err;

  if (m_is_compute) {
    if (m_cs.empty()) {
      Logger::err("Compute PSO has no CS bytecode");
      return false;
    }

    WMT::Reference<WMT::Function> cs_func;
    if (!CompileShader(m_cs.data(), m_cs.size(), ShaderType::Compute,
                       "cs_main", cs_func))
      return false;

    WMTComputePipelineInfo info = {};
    WMT::InitializeComputePipelineInfo(info);
    info.compute_function = cs_func.handle;

    m_compute_pso = wmt_device.newComputePipelineState(info, err);
    if (!m_compute_pso.handle) {
      Logger::err("Failed to create compute PSO");
      return false;
    }

    m_compiled = true;
    Logger::info("Compute PSO compiled successfully");
    return true;
  }

  WMT::Reference<WMT::Function> vs_func, ps_func;

  if (!m_vs.empty()) {
    if (!CompileShader(m_vs.data(), m_vs.size(), ShaderType::Vertex,
                       "vs_main", vs_func))
      return false;
  }

  if (!m_ps.empty()) {
    if (!CompileShader(m_ps.data(), m_ps.size(), ShaderType::Pixel,
                       "ps_main", ps_func))
      return false;
  }

  WMTRenderPipelineInfo info;
  WMT::InitializeRenderPipelineInfo(info);

  if (vs_func.handle)
    info.vertex_function = vs_func.handle;
  if (ps_func.handle)
    info.fragment_function = ps_func.handle;

  info.rasterization_enabled = (m_rasterizer_desc.FillMode != D3D12_FILL_MODE_WIREFRAME);
  info.raster_sample_count = m_sample_count ? m_sample_count : 1;

  for (UINT i = 0; i < m_num_render_targets && i < 8; i++) {
    auto fmt = DXGIToMTLPixelFormat(m_rtv_formats[i]);
    if (fmt != WMTPixelFormatInvalid)
      info.colors[i].pixel_format = fmt;
  }

  auto depth_fmt = DXGIToMTLPixelFormat(m_dsv_format);
  if (depth_fmt != WMTPixelFormatInvalid) {
    info.depth_pixel_format = depth_fmt;
    if (m_dsv_format == DXGI_FORMAT_D24_UNORM_S8_UINT ||
        m_dsv_format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
      info.stencil_pixel_format = depth_fmt;
  }

  if (m_blend_desc.RenderTarget[0].BlendEnable) {
    for (UINT i = 0; i < m_num_render_targets && i < 8; i++) {
      auto &rt = m_blend_desc.RenderTarget[i];
      info.colors[i].blending_enabled = rt.BlendEnable ? true : false;
      info.colors[i].write_mask = rt.RenderTargetWriteMask;

      auto map_blend = [](D3D12_BLEND b) -> WMTBlendFactor {
        switch (b) {
        case D3D12_BLEND_ZERO: return WMTBlendFactorZero;
        case D3D12_BLEND_ONE: return WMTBlendFactorOne;
        case D3D12_BLEND_SRC_COLOR: return WMTBlendFactorSourceColor;
        case D3D12_BLEND_INV_SRC_COLOR: return WMTBlendFactorOneMinusSourceColor;
        case D3D12_BLEND_SRC_ALPHA: return WMTBlendFactorSourceAlpha;
        case D3D12_BLEND_INV_SRC_ALPHA: return WMTBlendFactorOneMinusSourceAlpha;
        case D3D12_BLEND_DEST_ALPHA: return WMTBlendFactorDestinationAlpha;
        case D3D12_BLEND_INV_DEST_ALPHA: return WMTBlendFactorOneMinusDestinationAlpha;
        case D3D12_BLEND_DEST_COLOR: return WMTBlendFactorDestinationColor;
        case D3D12_BLEND_INV_DEST_COLOR: return WMTBlendFactorOneMinusDestinationColor;
        case D3D12_BLEND_SRC_ALPHA_SAT: return WMTBlendFactorSourceAlphaSaturated;
        case D3D12_BLEND_BLEND_FACTOR: return WMTBlendFactorBlendColor;
        case D3D12_BLEND_INV_BLEND_FACTOR: return WMTBlendFactorOneMinusBlendColor;
        default: return WMTBlendFactorOne;
        }
      };

      auto map_op = [](D3D12_BLEND_OP op) -> WMTBlendOperation {
        switch (op) {
        case D3D12_BLEND_OP_ADD: return WMTBlendOperationAdd;
        case D3D12_BLEND_OP_SUBTRACT: return WMTBlendOperationSubtract;
        case D3D12_BLEND_OP_REV_SUBTRACT: return WMTBlendOperationReverseSubtract;
        case D3D12_BLEND_OP_MIN: return WMTBlendOperationMin;
        case D3D12_BLEND_OP_MAX: return WMTBlendOperationMax;
        default: return WMTBlendOperationAdd;
        }
      };

      info.colors[i].src_rgb_blend_factor = map_blend(rt.SrcBlend);
      info.colors[i].dst_rgb_blend_factor = map_blend(rt.DestBlend);
      info.colors[i].rgb_blend_operation = map_op(rt.BlendOp);
      info.colors[i].src_alpha_blend_factor = map_blend(rt.SrcBlendAlpha);
      info.colors[i].dst_alpha_blend_factor = map_blend(rt.DestBlendAlpha);
      info.colors[i].alpha_blend_operation = map_op(rt.BlendOpAlpha);
    }
  }

  switch (m_topology) {
  case D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT: info.input_primitive_topology = WMTPrimitiveTopologyClassPoint; break;
  case D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE: info.input_primitive_topology = WMTPrimitiveTopologyClassLine; break;
  case D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE: info.input_primitive_topology = WMTPrimitiveTopologyClassTriangle; break;
  default: info.input_primitive_topology = WMTPrimitiveTopologyClassUnspecified; break;
  }

  info.immutable_vertex_buffers = (1 << 16) | (1 << 29) | (1 << 30);
  info.immutable_fragment_buffers = (1 << 29) | (1 << 30);

  m_render_pso = wmt_device.newRenderPipelineState(info, err);
  if (!m_render_pso.handle) {
    Logger::err("Failed to create render PSO");
    return false;
  }

  m_compiled = true;
  Logger::info(str::format("Graphics PSO compiled: RTs=", m_num_render_targets,
                            " DSV=", (int)m_dsv_format,
                            " samples=", m_sample_count));
  return true;
}

void MTLD3D12PipelineState::SetGraphicsDesc(
    const D3D12_GRAPHICS_PIPELINE_STATE_DESC &desc) {
  if (desc.pRootSignature) {
    m_root_sig = desc.pRootSignature;
    m_root_sig->AddRef();
  }

  if (desc.VS.pShaderBytecode && desc.VS.BytecodeLength) {
    m_vs.resize(desc.VS.BytecodeLength);
    memcpy(m_vs.data(), desc.VS.pShaderBytecode, desc.VS.BytecodeLength);
  }
  if (desc.PS.pShaderBytecode && desc.PS.BytecodeLength) {
    m_ps.resize(desc.PS.BytecodeLength);
    memcpy(m_ps.data(), desc.PS.pShaderBytecode, desc.PS.BytecodeLength);
  }
  if (desc.GS.pShaderBytecode && desc.GS.BytecodeLength) {
    m_gs.resize(desc.GS.BytecodeLength);
    memcpy(m_gs.data(), desc.GS.pShaderBytecode, desc.GS.BytecodeLength);
  }
  if (desc.HS.pShaderBytecode && desc.HS.BytecodeLength) {
    m_hs.resize(desc.HS.BytecodeLength);
    memcpy(m_hs.data(), desc.HS.pShaderBytecode, desc.HS.BytecodeLength);
  }
  if (desc.DS.pShaderBytecode && desc.DS.BytecodeLength) {
    m_ds.resize(desc.DS.BytecodeLength);
    memcpy(m_ds.data(), desc.DS.pShaderBytecode, desc.DS.BytecodeLength);
  }

  m_blend_desc = desc.BlendState;
  m_rasterizer_desc = desc.RasterizerState;
  m_depth_stencil_desc = desc.DepthStencilState;
  m_input_layout = desc.InputLayout;
  m_strip_cut_value = desc.IBStripCutValue;
  m_topology = desc.PrimitiveTopologyType;
  m_num_render_targets = desc.NumRenderTargets;
  memcpy(m_rtv_formats, desc.RTVFormats, sizeof(m_rtv_formats));
  m_dsv_format = desc.DSVFormat;
  m_sample_mask = desc.SampleMask;
  m_sample_count = desc.SampleDesc.Count ? desc.SampleDesc.Count : 1;
}

void MTLD3D12PipelineState::SetComputeDesc(
    const D3D12_COMPUTE_PIPELINE_STATE_DESC &desc) {
  if (desc.pRootSignature) {
    m_root_sig = desc.pRootSignature;
    m_root_sig->AddRef();
  }
  if (desc.CS.pShaderBytecode && desc.CS.BytecodeLength) {
    m_cs.resize(desc.CS.BytecodeLength);
    memcpy(m_cs.data(), desc.CS.pShaderBytecode, desc.CS.BytecodeLength);
  }
}

HRESULT STDMETHODCALLTYPE
MTLD3D12PipelineState::QueryInterface(REFIID riid, void **ppvObject) {
  if (!ppvObject)
    return E_POINTER;
  *ppvObject = nullptr;

  if (riid == IID_IUnknown || riid == IID_ID3D12Object ||
      riid == IID_ID3D12DeviceChild || riid == IID_ID3D12Pageable ||
      riid == IID_ID3D12PipelineState) {
    *ppvObject = ref(this);
    return S_OK;
  }
  return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE
MTLD3D12PipelineState::AddRef() { return ++m_refCount; }

ULONG STDMETHODCALLTYPE MTLD3D12PipelineState::Release() {
  uint32_t rc = --m_refCount;
  if (!rc)
    delete this;
  return rc;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12PipelineState::GetPrivateData(REFGUID guid, UINT *data_size,
                                      void *data) {
  return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12PipelineState::SetPrivateData(REFGUID guid, UINT data_size,
                                      const void *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12PipelineState::SetPrivateDataInterface(REFGUID guid,
                                               const IUnknown *data) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE MTLD3D12PipelineState::SetName(LPCWSTR name) {
  return S_OK;
}

HRESULT STDMETHODCALLTYPE
MTLD3D12PipelineState::GetDevice(REFIID riid, void **device) {
  return m_device->QueryInterface(riid, device);
}

HRESULT STDMETHODCALLTYPE
MTLD3D12PipelineState::GetCachedBlob(ID3DBlob **blob) {
  return E_NOTIMPL;
}

} // namespace dxmt
