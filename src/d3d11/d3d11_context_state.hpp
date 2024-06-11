#pragma once
#include "d3d11_device_child.hpp"

#include "com/com_pointer.hpp"
#include "d3d11_input_layout.hpp"
#include "d3d11_shader.hpp"
#include "d3d11_state_object.hpp"
#include "d3d11_view.hpp"
#include "log/log.hpp"
#include "mtld11_resource.hpp"
#include "util_string.hpp"
#include <unordered_map>

namespace dxmt {

struct UAV_B {
  Com<IMTLBindable> View;
  UINT InitialCountValue;
};

struct D3D11ComputeStageState {
  std::map<UINT, UAV_B> UAVs;
};

struct CONSTANT_BUFFER_B {
  Com<IMTLBindable> Buffer;
  UINT FirstConstant;
  UINT NumConstants;
};

struct D3D11ShaderStageState {
  std::unordered_map<UINT, Com<IMTLBindable>> SRVs;
  std::unordered_map<UINT, Com<IMTLD3D11SamplerState>> Samplers;
  std::unordered_map<UINT, CONSTANT_BUFFER_B> ConstantBuffers;
  Com<IMTLD3D11Shader> Shader;
};

struct VERTEX_BUFFER_B {
  Com<IMTLBindable> Buffer;
  UINT Stride;
  UINT Offset;
};

struct D3D11InputAssemblerStageState {
  Com<IMTLD3D11InputLayout> InputLayout;
  std::map<UINT, VERTEX_BUFFER_B> VertexBuffers;
  Com<IMTLBindable> IndexBuffer;
  /**
  either DXGI_FORMAT_R16_UINT or DXGI_FORMAT_R32_UINT
  */
  DXGI_FORMAT IndexBufferFormat;
  UINT IndexBufferOffset;
  D3D11_PRIMITIVE_TOPOLOGY Topology;
};

struct D3D11OutputMergerStageState {
  Com<IMTLD3D11RenderTargetView> RTVs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
  Com<IMTLD3D11DepthStencilView> DSV;
  UINT NumRTVs;

  std::map<UINT, UAV_B> UAVs;

  Com<IMTLD3D11DepthStencilState> DepthStencilState;
  UINT StencilRef;

  Com<IMTLD3D11BlendState> BlendState;
  FLOAT BlendFactor[4];

  // See https://github.com/gpuweb/gpuweb/issues/267
  // Currently ignored, should be able to emulate it as AND operation in shader
  UINT SampleMask;
};

struct D3D11StreamOutputStageState {
  Com<ID3D11Buffer> output_buffers[4];
  UINT output_buffer_offsets[4];
};

struct D3D11RasterizerStageState {
  D3D11_RECT
  scissor_rects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {
      {}};
  D3D11_VIEWPORT
  viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {{}};
  UINT NumScissorRects;
  UINT NumViewports;
  Com<IMTLD3D11RasterizerState> RasterizerState;
};

struct D3D11ContextState {
  D3D11ShaderStageState ShaderStages[6] = {{}};
  D3D11ComputeStageState ComputeStageUAV = {};
  D3D11StreamOutputStageState stream_output = {};
  D3D11InputAssemblerStageState InputAssembler = {};
  D3D11OutputMergerStageState OutputMerger = {};
  D3D11RasterizerStageState Rasterizer = {};

  Com<ID3D11Predicate> predicate = nullptr;
  BOOL predicate_value = FALSE;
};

class MTLD3D11DeviceContext;

// TODO: implemente it properly
class MTLD3D11DeviceContextState
    : public MTLD3D11DeviceChild<ID3DDeviceContextState> {
  friend class MTLD3D11DeviceContext;

public:
  MTLD3D11DeviceContextState(IMTLD3D11Device *pDevice)
      : MTLD3D11DeviceChild<ID3DDeviceContextState>(pDevice) {}

  ~MTLD3D11DeviceContextState() {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3DDeviceContextState)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3DDeviceContextState), riid)) {
      WARN("D3DDeviceContextState: Unknown interface query ",
           str::format(riid));
    }

    return E_NOINTERFACE;
  }
};

} // namespace dxmt