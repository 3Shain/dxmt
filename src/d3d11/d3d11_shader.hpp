#pragma once

#include "airconv_public.h"
#include "com/com_guid.hpp"
#include "d3d11_device.hpp"
#include "d3d11_device_child.hpp"
#include "objc-wrapper/dispatch.h"
#include "sha1/sha1_util.hpp"
#include "log/log.hpp"

struct MTL_COMPILED_SHADER {
  /**
  NOTE: it's not retained by design
  */
  MTL::Function *Function;
  dxmt::Sha1Hash *MetallibHash;
};

DEFINE_COM_INTERFACE("a8bfeef7-a453-4bce-90c1-912b02cf5cdf", IMTLCompiledShader)
    : public IMTLThreadpoolWork {
  virtual ~IMTLCompiledShader(){};
  virtual void SubmitWork() = 0;
  /**
  return false if it's not ready
   */
  virtual bool GetShader(MTL_COMPILED_SHADER * pShaderData) = 0;

  virtual void Dump() = 0;
};

namespace dxmt {
class Shader;
};
typedef dxmt::Shader *ManagedShader;

DEFINE_COM_INTERFACE("e95ba1c7-e43f-49c3-a907-4ac669c9fb42", IMTLD3D11Shader)
    : public IUnknown {
  virtual ManagedShader GetManagedShader() = 0;
};

namespace dxmt {

template <typename Interface>
class TShaderBase : public MTLD3D11DeviceChild<Interface, IMTLD3D11Shader> {
private:
  ManagedShader shader;

public:
  TShaderBase(IMTLD3D11Device *device, ManagedShader shader)
      : MTLD3D11DeviceChild<Interface, IMTLD3D11Shader>(device),
        shader(shader) {}

  ~TShaderBase() {}

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11View) || riid == __uuidof(Interface)) {
      *ppvObject = ref_and_cast<Interface>(this);
      return S_OK;
    }

    if (riid == __uuidof(IMTLD3D11Shader)) {
      *ppvObject = ref_and_cast<IMTLD3D11Shader>(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(Interface), riid)) {
      WARN("D3D11Shader: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  ManagedShader GetManagedShader() { return shader; }
};

struct ShaderVariantVertex {
  using this_type = ShaderVariantVertex;
  uint64_t input_layout_handle;
  uint32_t gs_passthrough;
  bool operator==(const this_type &rhs) const {
    return input_layout_handle == rhs.input_layout_handle &&
           gs_passthrough == rhs.gs_passthrough;
  }
};

struct ShaderVariantPixel {
  using this_type = ShaderVariantPixel;
  uint32_t sample_mask;
  bool dual_source_blending;
  bool disable_depth_output;
  bool operator==(const this_type &rhs) const {
    return sample_mask == rhs.sample_mask &&
           dual_source_blending == rhs.dual_source_blending &&
           disable_depth_output == rhs.disable_depth_output;
  }
};

struct ShaderVariantTessellationVertex {
  using this_type = ShaderVariantTessellationVertex;
  uint64_t input_layout_handle;
  uint64_t hull_shader_handle;
  SM50_INDEX_BUFFER_FORAMT index_buffer_format;
  bool operator==(const this_type &rhs) const {
    return input_layout_handle == rhs.input_layout_handle &&
           hull_shader_handle == rhs.hull_shader_handle &&
           index_buffer_format == rhs.index_buffer_format;
  }
};

struct ShaderVariantTessellationHull {
  using this_type = ShaderVariantTessellationHull;
  uint64_t vertex_shader_handle;
  bool operator==(const this_type &rhs) const {
    return vertex_shader_handle == rhs.vertex_shader_handle;
  }
};

struct ShaderVariantTessellationDomain {
  using this_type = ShaderVariantTessellationDomain;
  uint64_t hull_shader_handle;
  uint32_t gs_passthrough;
  bool operator==(const this_type &rhs) const {
    return hull_shader_handle == rhs.hull_shader_handle &&
           gs_passthrough == rhs.gs_passthrough;
  }
};

struct ShaderVariantDefault {
  using this_type = ShaderVariantDefault;
  bool operator==(const this_type &rhs) const { return true; }
};

struct ShaderVariantVertexStreamOutput {
  using this_type = ShaderVariantVertexStreamOutput;
  uint64_t input_layout_handle;
  uint64_t stream_output_layout_handle;
  bool operator==(const this_type &rhs) const {
    return input_layout_handle == rhs.input_layout_handle &&
           stream_output_layout_handle == rhs.stream_output_layout_handle;
  }
};

using ShaderVariant =
    std::variant<ShaderVariantDefault, ShaderVariantVertex, ShaderVariantPixel,
                 ShaderVariantTessellationVertex, ShaderVariantTessellationHull,
                 ShaderVariantTessellationDomain, ShaderVariantVertexStreamOutput>;

enum class ShaderType {
  Vertex = 0,
  Pixel = 1,
  Geometry = 2,
  Hull = 3,
  Domain = 4,
  Compute = 5
};

class Shader {
public:
  virtual ~Shader() {};
  /* FIXME: exposed implementation detail */
  virtual SM50Shader *handle();
  /* FIXME: exposed implementation detail */
  virtual MTL_SHADER_REFLECTION &reflection();
  virtual Com<IMTLCompiledShader> get_shader(ShaderVariant variant);
  virtual uint64_t id();
};

template <typename Variant>
std::unique_ptr<IMTLCompiledShader>
CreateVariantShader(IMTLD3D11Device *, ManagedShader, Variant);

} // namespace dxmt

namespace std {
template <> struct hash<dxmt::ShaderVariant> {
  size_t operator()(const dxmt::ShaderVariant &v) const noexcept {
    return v.index(); // FIXME:
  };
};
} // namespace std
