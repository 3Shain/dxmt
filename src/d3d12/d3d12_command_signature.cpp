/*
 * Copyright 2026 Feifan He for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "com/com_pointer.hpp"
#include "d3d12_device.hpp"
#include "d3d12_pageable.hpp"

namespace dxmt {

constexpr auto kSharedHeader = R"(
#include <metal_stdlib>

using namespace metal;

struct dxmt_compute_command_data {
  command_buffer cmd_buf;
  ulong max_count;
  device uint * max_count_buffer;
  device char * argument_buffer;
  device ulong * static_samplers;
  device ulong * rootsig_qwords;
  uint rootsig_qwords_stride;
  packed_uint3 tgsize;
};

)";

class MTLD3D12CommandSignatureImpl : public MTLD3D12Pageable<MTLD3D12CommandSignature> {

public:
  MTLD3D12CommandSignatureImpl(MTLD3D12Device *pDevice) : MTLD3D12Pageable<MTLD3D12CommandSignature>(pDevice) {}

  HRESULT
  Initialize(const D3D12_COMMAND_SIGNATURE_DESC *pDesc, ID3D12RootSignature *pRootSignature) {
    std::stringstream source;
    D3D12_INDIRECT_ARGUMENT_TYPE side_effect = ~(D3D12_INDIRECT_ARGUMENT_TYPE){};

    source << kSharedHeader;

    source << "struct __attribute__ ((packed)) d3d12_arguments {\n";

    for (unsigned i = 0; i < pDesc->NumArgumentDescs; i++) {
      if (~side_effect != 0u)
        return E_INVALIDARG;
      auto &arg = pDesc->pArgumentDescs[i];
      switch (arg.Type) {
      case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH: {
        side_effect = arg.Type;
        source << "packed_uint3 dispatch;\n";
        break;
      }
      case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW:
      case D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED:
      case D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW:
      case D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW:
      case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS:
      case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH:
        IMPLEMENT_ME
        return E_NOTIMPL;
      default:
        return E_INVALIDARG;
      }
    }
    source << "};\n\n";

    if (~side_effect == 0)
      return E_INVALIDARG;
    CommandType = side_effect;

    source
        << "[[kernel]] void resolve_indirect_commands([[thread_position_in_grid]] uint x, constant dxmt_compute_command_data";
    source << " &command_data [[buffer(30)]]) {\n";

    source << "if (x !=0 ) return;\n";

    source << "uint count = command_data.max_count_buffer ? "
              "command_data.max_count_buffer[0] : command_data.max_count;\n";
    source << "for (uint i = 0; i < command_data.max_count; i++) {\n";
    source << "device d3d12_arguments& arg = reinterpret_cast<device d3d12_arguments *>("
              "command_data.argument_buffer + i * "
           << pDesc->ByteStride << ")[0];\n";
    source << "compute_command cmd(command_data.cmd_buf, i);\n";
    source << "cmd.reset();\n";
    source << "if (i >= count) continue;\n";

    for (unsigned i = 0; i < pDesc->NumArgumentDescs; i++) {
      auto &arg = pDesc->pArgumentDescs[i];
      switch (arg.Type) {
      case D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH: {
        source << "cmd.concurrent_dispatch_threadgroups(arg.dispatch, command_data.tgsize);\n";
        break;
      }
      default:
        return E_INVALIDARG;
      }
    }

    source << "}\n"
              "};\n";

    WMT::Reference<WMT::Error> err;
    auto lib = device_->GetMTLDevice().newLibraryWithSource(source.view(), err);

    if (!lib) {
      ERR("Failed to compile command signature resolve shader: ", err.description().getUTF8String());
      return E_FAIL;
    }

    auto function = lib.newFunction("resolve_indirect_commands");

    if (!function) {
      ERR("Failed to create command signature resolve shader");
      return E_FAIL;
    }

    compute_resolver = device_->GetMTLDevice().newComputePipelineState(function, err);

    if (err) {
      ERR("Failed to compile command signature resolve pso: ", err.description().getUTF8String());
      return E_FAIL;
    }

    return S_OK;
  };

  ~MTLD3D12CommandSignatureImpl() {}

  HRESULT
  STDMETHODCALLTYPE
  QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D12Object) || riid == __uuidof(ID3D12DeviceChild) ||
        riid == __uuidof(ID3D12Pageable) || riid == __uuidof(ID3D12CommandSignature)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D12Resource), riid)) {
      WARN("D3D12CommandSignature: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }
};

HRESULT
CreateCommandSignature(
    MTLD3D12Device *pDevice, const D3D12_COMMAND_SIGNATURE_DESC *pDesc, ID3D12RootSignature *pRootSignature,
    REFIID riid, void **ppCommandSignature
) {
  auto sig = Com(new MTLD3D12CommandSignatureImpl(pDevice));
  HRESULT hr = sig->Initialize(pDesc, pRootSignature);
  if (FAILED(hr))
    return hr;
  return sig->QueryInterface(riid, ppCommandSignature);
}

} // namespace dxmt