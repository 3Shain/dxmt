#include "d3d11_input_layout.hpp"
#include <atomic>
#include <string>
#define __METALCPP__ 1
#include "d3d11_private.h"
#include "d3d11_shader.hpp"
#include "Metal/MTLLibrary.hpp"
#include "airconv_public.h"
#include "d3d11_device_child.hpp"
#include "objc_pointer.hpp"
#include "DXBCParser/DXBCUtils.h"
#include "log/log.hpp"

namespace dxmt {

struct tag_vertex_shader {
  using COM = ID3D11VertexShader;
  using DATA =
      std::unordered_map<IMTLD3D11InputLayout *, Com<IMTLCompiledShader>>;
};

struct tag_pixel_shader {
  using COM = ID3D11PixelShader;
  using DATA = std::unordered_map<uint32_t, Com<IMTLCompiledShader>>;
};

struct tag_hull_shader {
  using COM = ID3D11HullShader;
  using DATA = void *;
};

struct tag_domain_shader {
  using COM = ID3D11DomainShader;
  using DATA = void *;
};

struct tag_geometry_shader {
  using COM = ID3D11GeometryShader;
  using DATA = void *;
};

struct tag_compute_shader {
  using COM = ID3D11ComputeShader;
  using DATA = void *;
};

struct EMULATED_VERTEX_SO_DATA {
  uint32_t NumSlots = 0;
  std::array<uint32_t, 4> Strides{{}};
  std::vector<SM50_STREAM_OUTPUT_ELEMENT> Elements;
};

struct tag_emulated_vertex_so {
  using COM = ID3D11GeometryShader;
  using DATA = EMULATED_VERTEX_SO_DATA;
};

static std::atomic_uint64_t global_id = 0;
static std::atomic_uint64_t global_variant_id = 1;

template <typename tag>
class TShaderBase
    : public MTLD3D11DeviceChild<typename tag::COM, IMTLD3D11Shader> {
public:
  TShaderBase(IMTLD3D11Device *device, SM50Shader *sm50,
              MTL_SHADER_REFLECTION &reflection, const void *pShaderBytecode,
              SIZE_T BytecodeLength, typename tag::DATA &&data)
      : MTLD3D11DeviceChild<typename tag::COM, IMTLD3D11Shader>(device),
        sm50(sm50), reflection(reflection),
        data(std::forward<typename tag::DATA>(data)) {
    id = ++global_id;
#ifdef DXMT_DEBUG
    dump_len = BytecodeLength;
    dump = malloc(BytecodeLength);
    memcpy(dump, pShaderBytecode, BytecodeLength);
#endif
  }

  ~TShaderBase() {
    if (sm50) {
      SM50Destroy(sm50);
      sm50 = nullptr;
    }
#ifdef DXMT_DEBUG
    free(dump);
#endif
  }

  bool dumped = false;

  void Dump() {
    if(dumped) {
      return;
    }
#ifdef DXMT_DEBUG
    std::fstream dump_out;
    dump_out.open("shader_dump_" + std::to_string(id) + ".cso",
                  std::ios::out | std::ios::binary);
    if (dump_out) {
      dump_out.write((char *)dump, dump_len);
    }
    dump_out.close();
    ERR("dumped to ./shader_dump_" + std::to_string(id) + ".cso");
#else
    WARN("shader dump disabled");
#endif
    dumped = true;
  }

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11View) || riid == __uuidof(typename tag::COM) ||
        riid == __uuidof(typename tag::COM)) {
      *ppvObject = ref_and_cast<typename tag::COM>(this);
      return S_OK;
    }

    if (riid == __uuidof(IMTLD3D11Shader)) {
      *ppvObject = ref_and_cast<IMTLD3D11Shader>(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(typename tag::COM), riid)) {
      WARN("D3D11Shader: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  void GetCompiledShader(IMTLCompiledShader **pShader) final;

  uint64_t GetUniqueId() final { return id; }

  void *GetAirconvHandle() final { return sm50; }

  void GetCompiledVertexShaderWithVertexPulling(IMTLD3D11InputLayout *,
                                                IMTLCompiledShader **) final;

  virtual void GetCompiledPixelShader(uint32_t sample_mask,
                                      bool dual_source_blending,
                                      IMTLCompiledShader **ppShader) final;

  const MTL_SHADER_REFLECTION *GetReflection() final { return &reflection; }

  SM50Shader *sm50;
  MTL_SHADER_REFLECTION reflection;
  Com<IMTLCompiledShader> precompiled_;
  std::unordered_map<uint64_t, Com<IMTLCompiledShader>>
      with_input_layout_fixup_;
  uint64_t id;
  typename tag::DATA data;
#ifdef DXMT_DEBUG
  void *dump;
  uint64_t dump_len;
#endif
};

template <typename tag>
class AirconvShader : public ComObject<IMTLCompiledShader> {
public:
  AirconvShader(IMTLD3D11Device *pDevice, TShaderBase<tag> *shader,
                void *compilation_args)
      : ComObject<IMTLCompiledShader>(), device_(pDevice), shader_(shader),
        compilation_args(compilation_args) {
    shader_->AddRef(); // ???
    identity_data.type = SM50_SHADER_DEBUG_IDENTITY;
    identity_data.id = shader_->id;
    identity_data.next = compilation_args;
    variant_id = global_variant_id++;
  }

  ~AirconvShader() {}

  void SubmitWork() { device_->SubmitThreadgroupWork(this); }

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(IMTLThreadpoolWork) ||
        riid == __uuidof(IMTLCompiledShader)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    return E_NOINTERFACE;
  }

  bool GetShader(MTL_COMPILED_SHADER *pShaderData) final {
    bool ret = false;
    if ((ret = ready_.load(std::memory_order_acquire))) {
      *pShaderData = {function_.ptr(), &hash_};
    }
    return ret;
  }

  void Dump() {
#ifdef DXMT_DEBUG
    std::fstream dump_out;
    dump_out.open("shader_dump_" + std::to_string(shader_->id) + ".cso",
                  std::ios::out | std::ios::binary);
    if (dump_out) {
      dump_out.write((char *)shader_->dump, shader_->dump_len);
    }
    dump_out.close();
    ERR("dumped to ./shader_dump_" + std::to_string(shader_->id) + ".cso");
#else
    WARN("shader dump disabled");
#endif
  }

  IMTLThreadpoolWork* RunThreadpoolWork() {
    D3D11_ASSERT(!ready_ && "?wtf"); // TODO: should use a lock?

    /**
    workaround: ensure shader bytecode is accessible
    FIXME: why not strong reference shader? (circular ref?)
     */
    // shader_->AddRef();

    TRACE("Start compiling 1 shader ", shader_->id);

    auto pool = transfer(NS::AutoreleasePool::alloc()->init());
    Obj<NS::Error> err;

    {
      SM50CompiledBitcode *compile_result = nullptr;
      SM50Error *sm50_err = nullptr;
      std::string func_name = "shader_main_" + std::to_string(shader_->id);
      if (auto ret = SM50Compile(
              shader_->sm50,
              (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)&identity_data,
              func_name.c_str(), &compile_result, &sm50_err)) {
        if (ret == 42) {
          ERR("Failed to compile shader due to failed assertation");
        } else {
          ERR("Failed to compile shader: ", SM50GetErrorMesssage(sm50_err));
          SM50FreeError(sm50_err);
        }
        Dump();
        return this;
      }
      MTL_SHADER_BITCODE bitcode;
      SM50GetCompiledBitcode(compile_result, &bitcode);
      hash_.compute(bitcode.Data, bitcode.Size);
      auto dispatch_data =
          dispatch_data_create(bitcode.Data, bitcode.Size, nullptr, nullptr);
      D3D11_ASSERT(dispatch_data);
      Obj<MTL::Library> library =
          transfer(device_->GetMTLDevice()->newLibrary(dispatch_data, &err));

      if (err) {
        ERR("Failed to create MTLLibrary: ",
            err->localizedDescription()->utf8String());
        return this;
      }

      dispatch_release(dispatch_data);
      SM50DestroyBitcode(compile_result);
      function_ = transfer(library->newFunction(
          NS::String::string(func_name.c_str(), NS::UTF8StringEncoding)));
      if (function_ == nullptr) {
        ERR("Failed to create MTLFunction: ", func_name);
        return this;
      }
    }

    TRACE("Compiled 1 shader ", shader_->id);

    /* workaround end: ensure shader bytecode is accessible */
    shader_->Release();

    return this;
  }

  bool GetIsDone() {
    return ready_;
  }

  void SetIsDone(bool state) {
    ready_.store(state);
  }

private:
  IMTLD3D11Device *device_;
  TShaderBase<tag> *shader_; // TODO: should hold strong reference?
  std::atomic_bool ready_;
  Sha1Hash hash_;
  Obj<MTL::Function> function_;
  void *compilation_args;
  SM50_SHADER_DEBUG_IDENTITY_DATA identity_data;
  uint64_t variant_id;
};

template <typename tag>
class AirconvShaderWithInputLayoutFixup : public AirconvShader<tag> {
public:
  AirconvShaderWithInputLayoutFixup(IMTLD3D11Device *pDevice,
                                    TShaderBase<tag> *shader,
                                    uint64_t sign_mask)
      : AirconvShader<tag>(pDevice, shader, &data) {
    data.type = SM50_SHADER_COMPILATION_INPUT_SIGN_MASK;
    data.next = nullptr;
    data.sign_mask = sign_mask;
  };

private:
  SM50_SHADER_COMPILATION_INPUT_SIGN_MASK_DATA data;
};

class AirconvVertexShaderWithVertexPulling
    : public AirconvShader<tag_vertex_shader> {
public:
  AirconvVertexShaderWithVertexPulling(IMTLD3D11Device *pDevice,
                                       TShaderBase<tag_vertex_shader> *shader,
                                       IMTLD3D11InputLayout *pInputLayout)
      : AirconvShader<tag_vertex_shader>(pDevice, shader, &data) {
    data.type = SM50_SHADER_IA_INPUT_LAYOUT;
    data.next = nullptr;
    data.num_elements = pInputLayout->GetInputLayoutElements(
        (MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC **)&data.elements);
  };

private:
  SM50_SHADER_IA_INPUT_LAYOUT_DATA data;
};

class AirconvPixelShader
    : public AirconvShader<tag_pixel_shader> {
public:
  AirconvPixelShader(IMTLD3D11Device *pDevice,
                                   TShaderBase<tag_pixel_shader> *shader,
                                   uint32_t sample_mask, bool dual_source_blending)
      : AirconvShader<tag_pixel_shader>(pDevice, shader, &data) {
    data.type = SM50_SHADER_PSO_PIXEL_SHADER;
    data.next = nullptr;
    data.sample_mask = sample_mask;
    data.dual_source_blending = dual_source_blending;
  };

private:
  SM50_SHADER_PSO_PIXEL_SHADER_DATA data;
};

class AirconvShaderEmulatedVertexSO
    : public AirconvShader<tag_emulated_vertex_so> {
public:
  AirconvShaderEmulatedVertexSO(IMTLD3D11Device *pDevice,
                                TShaderBase<tag_emulated_vertex_so> *shader,
                                uint64_t sign_mask_)
      : AirconvShader<tag_emulated_vertex_so>(pDevice, shader, &data) {
    data.type = SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT;
    data.next = &data_sign_mask;
    data.num_output_slots = shader->data.NumSlots;
    data.num_elements = shader->data.Elements.size();
    memcpy(data.strides, shader->data.Strides.data(), sizeof(data.strides));
    data.elements = shader->data.Elements.data();

    data_sign_mask.type = SM50_SHADER_COMPILATION_INPUT_SIGN_MASK;
    data_sign_mask.next = nullptr;
    data_sign_mask.sign_mask = sign_mask_;
  };

private:
  std::vector<uint32_t> entries;
  SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA data;
  SM50_SHADER_COMPILATION_INPUT_SIGN_MASK_DATA data_sign_mask;
};

class AirconvShaderEmulatedVertexSOWithVertexPulling
    : public AirconvShader<tag_emulated_vertex_so> {
public:
  AirconvShaderEmulatedVertexSOWithVertexPulling(
      IMTLD3D11Device *pDevice, TShaderBase<tag_emulated_vertex_so> *shader,
      IMTLD3D11InputLayout *pInputLayout)
      : AirconvShader<tag_emulated_vertex_so>(pDevice, shader, &data) {
    data.type = SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT;
    data.next = &data_vertex_pulling;
    data.num_output_slots = shader->data.NumSlots;
    data.num_elements = shader->data.Elements.size();
    memcpy(data.strides, shader->data.Strides.data(), sizeof(data.strides));
    data.elements = shader->data.Elements.data();

    data_vertex_pulling.type = SM50_SHADER_IA_INPUT_LAYOUT;
    data_vertex_pulling.next = nullptr;
    data_vertex_pulling.num_elements = pInputLayout->GetInputLayoutElements(
        (MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC **)&data_vertex_pulling.elements);
  };

private:
  std::vector<uint32_t> entries;
  SM50_SHADER_EMULATE_VERTEX_STREAM_OUTPUT_DATA data;
  SM50_SHADER_IA_INPUT_LAYOUT_DATA data_vertex_pulling;
};

template <>
void TShaderBase<tag_emulated_vertex_so>::GetCompiledShader(
    IMTLCompiledShader **pShader) {
  if (!precompiled_) {
    precompiled_ = new AirconvShaderEmulatedVertexSO(this->m_parent, this, 0);
    precompiled_->SubmitWork();
  }
  if (pShader) {
    *pShader = precompiled_.ref();
  }
  return;
}

template <typename tag>
void TShaderBase<tag>::GetCompiledShader(IMTLCompiledShader **pShader) {
  if (!precompiled_) {
    precompiled_ = new AirconvShader(this->m_parent, this, nullptr);
    precompiled_->SubmitWork();
  }
  if (pShader) {
    *pShader = precompiled_.ref();
  }
  return;
}

template <typename tag>
void TShaderBase<tag>::GetCompiledVertexShaderWithVertexPulling(
    IMTLD3D11InputLayout *pInputLayout, IMTLCompiledShader **pShader) {
  D3D11_ASSERT(0 && "should not call this function");
}

template <>
void TShaderBase<tag_vertex_shader>::GetCompiledVertexShaderWithVertexPulling(
    IMTLD3D11InputLayout *pInputLayout, IMTLCompiledShader **pShader) {
  if (data.contains(pInputLayout)) {
    *pShader = data[pInputLayout].ref();
  } else {
    IMTLCompiledShader *shader = new AirconvVertexShaderWithVertexPulling(
        this->m_parent, this, pInputLayout);
    data.emplace(pInputLayout, shader);
    *pShader = ref(shader);
    shader->SubmitWork();
  }
}

template <>
void TShaderBase<tag_emulated_vertex_so>::
    GetCompiledVertexShaderWithVertexPulling(IMTLD3D11InputLayout *pInputLayout,
                                             IMTLCompiledShader **pShader) {
  IMTLCompiledShader *shader =
      new AirconvShaderEmulatedVertexSOWithVertexPulling(this->m_parent, this,
                                                         pInputLayout);
  *pShader = ref(shader);
  shader->SubmitWork();
}

template <typename tag>
void TShaderBase<tag>::GetCompiledPixelShader(uint32_t sample_mask, bool,
                                              IMTLCompiledShader **pShader) {
  D3D11_ASSERT(0 && "should not call this function");
}

template <>
void TShaderBase<tag_pixel_shader>::GetCompiledPixelShader(
    uint32_t SampleMask, bool DualSourceBlending,
    IMTLCompiledShader **pShader) {
  if (!DualSourceBlending && data.contains(SampleMask)) {
    *pShader = data[SampleMask].ref();
  } else {
    IMTLCompiledShader *shader = new AirconvPixelShader(
        this->m_parent, this, SampleMask, DualSourceBlending);
    if (!DualSourceBlending)
      data.emplace(SampleMask, shader);
    *pShader = ref(shader);
    shader->SubmitWork();
  }
}

template <typename tag>
HRESULT CreateShaderInternal(IMTLD3D11Device *pDevice,
                             const void *pShaderBytecode, SIZE_T BytecodeLength,
                             typename tag::COM **ppShader) {
  SM50Shader *sm50;
  SM50Error *err;
  MTL_SHADER_REFLECTION reflection;
  if (SM50Initialize(pShaderBytecode, BytecodeLength, &sm50, &reflection,
                     &err)) {
    ERR("Failed to initialize shader: ", SM50GetErrorMesssage(err));
    SM50FreeError(err);
    return E_FAIL;
  }
  if (ppShader == nullptr) {
    SM50Destroy(sm50);
    return S_FALSE;
  }
  *ppShader = ref(new TShaderBase<tag>(pDevice, sm50, reflection,
                                       pShaderBytecode, BytecodeLength, {}));
  return S_OK;
}

template <typename tag>
class TDummyShaderBase
    : public MTLD3D11DeviceChild<typename tag::COM, IMTLD3D11Shader> {
public:
  TDummyShaderBase(IMTLD3D11Device *device, const void *pShaderBytecode,
                   SIZE_T BytecodeLength)
      : MTLD3D11DeviceChild<typename tag::COM, IMTLD3D11Shader>(device) {
    id = ++global_id;
#ifdef DXMT_DEBUG
    dump_len = BytecodeLength;
    dump = malloc(BytecodeLength);
    memcpy(dump, pShaderBytecode, BytecodeLength);
#endif
  }

  ~TDummyShaderBase() {
#ifdef DXMT_DEBUG
    free(dump);
#endif
  }

  bool dumped = false;

  void Dump() {
    if(dumped) {
      return;
    }
#ifdef DXMT_DEBUG
    std::fstream dump_out;
    dump_out.open("shader_dump_" + std::to_string(id) + ".cso",
                  std::ios::out | std::ios::binary);
    if (dump_out) {
      dump_out.write((char *)dump, dump_len);
    }
    dump_out.close();
    ERR("dumped to ./shader_dump_" + std::to_string(id) + ".cso");
#else
    WARN("shader dump disabled");
#endif
  dumped = true;
  }

  HRESULT QueryInterface(REFIID riid, void **ppvObject) {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11View) || riid == __uuidof(typename tag::COM) ||
        riid == __uuidof(typename tag::COM)) {
      *ppvObject = ref_and_cast<typename tag::COM>(this);
      return S_OK;
    }

    if (riid == __uuidof(IMTLD3D11Shader)) {
      *ppvObject = ref_and_cast<IMTLD3D11Shader>(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(typename tag::COM), riid)) {
      WARN("D3D11Shader: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  }

  uint64_t GetUniqueId() final { return id; }

  void *GetAirconvHandle() final { return nullptr; }

  void GetCompiledShader(IMTLCompiledShader **ppShader) final {
    // D3D11_ASSERT(0 && "should not call this function");
    *ppShader = nullptr;
  };

  void GetCompiledVertexShaderWithVertexPulling(IMTLD3D11InputLayout *,
                                                IMTLCompiledShader **) final {
    // D3D11_ASSERT(0 && "should not call this function");
  };

  void GetCompiledPixelShader(uint32_t, bool, IMTLCompiledShader **) final {
    D3D11_ASSERT(0 && "should not call this function");
  };

  const MTL_SHADER_REFLECTION *GetReflection() final {
    D3D11_ASSERT(0 && "should not call this function");
    return nullptr;
  }

  MTL_SHADER_REFLECTION reflection{};
  uint64_t id;
#ifdef DXMT_DEBUG
  void *dump;
  uint64_t dump_len;
#endif
};

template <typename tag>
HRESULT
CreateDummyShaderInternal(IMTLD3D11Device *pDevice, const void *pShaderBytecode,
                          SIZE_T BytecodeLength, typename tag::COM **ppShader) {
  *ppShader =
      ref(new TDummyShaderBase<tag>(pDevice, pShaderBytecode, BytecodeLength));
  return S_OK;
}

HRESULT CreateVertexShader(IMTLD3D11Device *pDevice,
                           const void *pShaderBytecode, SIZE_T BytecodeLength,
                           ID3D11VertexShader **ppShader) {
  return CreateShaderInternal<tag_vertex_shader>(pDevice, pShaderBytecode,
                                                 BytecodeLength, ppShader);
}

HRESULT CreatePixelShader(IMTLD3D11Device *pDevice, const void *pShaderBytecode,
                          SIZE_T BytecodeLength, ID3D11PixelShader **ppShader) {
  return CreateShaderInternal<tag_pixel_shader>(pDevice, pShaderBytecode,
                                                BytecodeLength, ppShader);
}

HRESULT CreateHullShader(IMTLD3D11Device *pDevice, const void *pShaderBytecode,
                         SIZE_T BytecodeLength, ID3D11HullShader **ppShader) {
  return CreateShaderInternal<tag_hull_shader>(pDevice, pShaderBytecode,
                                               BytecodeLength, ppShader);
}

HRESULT CreateDomainShader(IMTLD3D11Device *pDevice,
                           const void *pShaderBytecode, SIZE_T BytecodeLength,
                           ID3D11DomainShader **ppShader) {
  return CreateShaderInternal<tag_domain_shader>(pDevice, pShaderBytecode,
                                                 BytecodeLength, ppShader);
}

HRESULT CreateDummyGeometryShader(IMTLD3D11Device *pDevice,
                                  const void *pShaderBytecode,
                                  SIZE_T BytecodeLength,
                                  ID3D11GeometryShader **ppShader) {
  return CreateDummyShaderInternal<tag_geometry_shader>(
      pDevice, pShaderBytecode, BytecodeLength, ppShader);
}

HRESULT CreateComputeShader(IMTLD3D11Device *pDevice,
                            const void *pShaderBytecode, SIZE_T BytecodeLength,
                            ID3D11ComputeShader **ppShader) {
  return CreateShaderInternal<tag_compute_shader>(pDevice, pShaderBytecode,
                                                  BytecodeLength, ppShader);
}

HRESULT CreateEmulatedVertexStreamOutputShader(
    IMTLD3D11Device *pDevice, const void *pShaderBytecode,
    SIZE_T BytecodeLength, ID3D11GeometryShader **ppShader, UINT NumEntries,
    const D3D11_SO_DECLARATION_ENTRY *pEntries, UINT NumStrides,
    const UINT *pStrides) {
  using namespace microsoft;
  EMULATED_VERTEX_SO_DATA data{};
  std::array<uint32_t, 4> &strides = data.Strides;
  std::array<uint32_t, 4> &offsets = strides;
  CSignatureParser parser;
  HRESULT hr = DXBCGetOutputSignature(pShaderBytecode, &parser);
  if (FAILED(hr)) {
    return hr;
  }
  const D3D11_SIGNATURE_PARAMETER *pParamters;
  auto numParameteres = parser.GetParameters(&pParamters);
  for (unsigned i = 0; i < NumEntries; i++) {
    auto entry = pEntries[i];
    if (entry.Stream != 0) {
      ERR("CreateEmulatedVertexStreamOutputShader: stream must be 0");
      return E_INVALIDARG;
    }
    if (entry.OutputSlot > 3 || entry.OutputSlot < 0) {
      ERR("CreateEmulatedVertexStreamOutputShader: invalid output slot ",
          entry.OutputSlot);
      return E_INVALIDARG;
    }
    // FIXME: support more than 1 output slot
    if (entry.OutputSlot != 0) {
      ERR("CreateEmulatedVertexStreamOutputShader: only slot 0 supported");
      return E_INVALIDARG;
    }
    if ((entry.ComponentCount - entry.StartComponent) < 0 ||
        (entry.ComponentCount + entry.StartComponent) > 4) {
      ERR("CreateEmulatedVertexStreamOutputShader: invalid components");
      return E_INVALIDARG;
    }
    if ((entry.ComponentCount - entry.StartComponent) == 0) {
      continue;
    }
    if (entry.SemanticName == nullptr) {
      // skip hole
      for (unsigned i = 0; i < entry.ComponentCount; i++) {
        auto offset = offsets[entry.OutputSlot];
        offsets[entry.OutputSlot] += sizeof(float);
        auto component = entry.StartComponent + i;
        data.Elements.push_back({.reg_id = 0xffffffff,
                                 .component = component,
                                 .output_slot = entry.OutputSlot,
                                 .offset = offset});
      }
      continue;
    }
    auto pDesc = std::find_if(
        pParamters, pParamters + numParameteres,
        [&](const D3D11_SIGNATURE_PARAMETER &Ele) {
          return Ele.SemanticIndex == entry.SemanticIndex &&
                 strcasecmp(Ele.SemanticName, entry.SemanticName) == 0;
        });
    if (pDesc == pParamters + numParameteres) {
      ERR("CreateEmulatedVertexStreamOutputShader: output parameter not found");
      return E_INVALIDARG;
    }
    auto reg_id = pDesc->Register;
    for (unsigned i = 0; i < entry.ComponentCount; i++) {
      auto offset = offsets[entry.OutputSlot];
      offsets[entry.OutputSlot] += sizeof(float);
      auto component = entry.StartComponent + i;
      data.Elements.push_back({.reg_id = reg_id,
                               .component = component,
                               .output_slot = entry.OutputSlot,
                               .offset = offset});
    }
  }

  for (unsigned slot = 0; slot < NumStrides; slot++) {
    strides[slot] = pStrides[slot];
  }

  SM50Shader *sm50;
  SM50Error *err;
  MTL_SHADER_REFLECTION reflection;
  if (SM50Initialize(pShaderBytecode, BytecodeLength, &sm50, &reflection,
                     &err)) {
    ERR("Failed to initialize shader: ", SM50GetErrorMesssage(err));
    SM50FreeError(err);
    return E_FAIL;
  }
  if (ppShader == nullptr) {
    SM50Destroy(sm50);
    return S_FALSE;
  }
  *ppShader = ref(new TShaderBase<tag_emulated_vertex_so>(
      pDevice, sm50, reflection, pShaderBytecode, BytecodeLength,
      std::move(data)));
  return S_OK;
}

} // namespace dxmt