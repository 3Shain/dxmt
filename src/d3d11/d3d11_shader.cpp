#include <atomic>
#include <fstream>
#include <string>
#define __METALCPP__ 1
#include "d3d11_private.h"
#include "d3d11_shader.hpp"
#include "Metal/MTLArgumentEncoder.hpp"
#include "Metal/MTLLibrary.hpp"
#include "airconv_public.h"
#include "d3d11_device_child.hpp"
#include "objc_pointer.hpp"

namespace dxmt {

struct tag_vertex_shader {
  using COM = ID3D11VertexShader;
};

struct tag_pixel_shader {
  using COM = ID3D11PixelShader;
};

struct tag_hull_shader {
  using COM = ID3D11HullShader;
};

struct tag_domain_shader {
  using COM = ID3D11DomainShader;
};

struct tag_geometry_shader {
  using COM = ID3D11GeometryShader;
};

struct tag_compute_shader {
  using COM = ID3D11ComputeShader;
};

static std::atomic_uint64_t global_id = 0;

template <typename tag>
class TShaderBase
    : public MTLD3D11DeviceChild<typename tag::COM, IMTLD3D11Shader> {
public:
  TShaderBase(IMTLD3D11Device *device, SM50Shader *sm50,
              MTL_SHADER_REFLECTION &reflection, const void *pShaderBytecode,
              SIZE_T BytecodeLength)
      : MTLD3D11DeviceChild<typename tag::COM, IMTLD3D11Shader>(device),
        sm50(sm50), reflection(reflection) {
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

  void GetCompiledShaderWithInputLayerFixup(uint64_t sign_mask,
                                            IMTLCompiledShader **pShader) final;

  void GetReflection(MTL_SHADER_REFLECTION **pRefl) final {
    *pRefl = &reflection;
  }

  SM50Shader *sm50;
  MTL_SHADER_REFLECTION reflection;
  Com<IMTLCompiledShader> precompiled_;
  std::unordered_map<uint64_t, Com<IMTLCompiledShader>>
      with_input_layout_fixup_;
  Obj<MTL::ArgumentEncoder> encoder_;
  uint64_t id;
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
  }

  void SubmitWork() { device_->SubmitThreadgroupWork(this, &work_state_); }

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

  bool IsReady() final { return ready_.load(std::memory_order_relaxed); }

  void GetShader(MTL_COMPILED_SHADER *pShaderData) final {
    ready_.wait(false, std::memory_order_acquire);
    *pShaderData = {function_.ptr(), &hash_, &shader_->reflection};
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

  void RunThreadpoolWork() {
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
              (SM50_SHADER_COMPILATION_ARGUMENT_DATA *)compilation_args,
              func_name.c_str(), &compile_result, &sm50_err)) {
        if (ret == 42) {
          ERR("Failed to compile shader due to failed assertation");
        } else {
          ERR("Failed to compile shader: ", SM50GetErrorMesssage(sm50_err));
          SM50FreeError(sm50_err);
        }
        Dump();
        return;
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
        return; // FIXME: ready_ always false? should propagate error
      }

      dispatch_release(dispatch_data);
      SM50DestroyBitcode(compile_result);
      function_ = transfer(library->newFunction(
          NS::String::string(func_name.c_str(), NS::UTF8StringEncoding)));
      if (function_ == nullptr) {
        ERR("Failed to create MTLFunction: ", func_name);
        return;
      }
    }

    TRACE("Compiled 1 shader ", shader_->id);

    /* workaround end: ensure shader bytecode is accessible */
    shader_->Release();

    ready_.store(true);
    ready_.notify_all();
  }

private:
  IMTLD3D11Device *device_;
  TShaderBase<tag> *shader_; // TODO: should hold strong reference?
  std::atomic_bool ready_;
  THREADGROUP_WORK_STATE work_state_;
  Sha1Hash hash_;
  Obj<MTL::Function> function_;
  void *compilation_args;
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

template <typename tag>
void TShaderBase<tag>::GetCompiledShader(IMTLCompiledShader **pShader) {
  // pArgs not used at the moment
  if (!precompiled_) {
    precompiled_ = new AirconvShader(this->m_parent, this, nullptr);
    precompiled_->SubmitWork();
  }
  *pShader = precompiled_.ref();
  return;
}

template <typename tag>
void TShaderBase<tag>::GetCompiledShaderWithInputLayerFixup(
    uint64_t sign_mask, IMTLCompiledShader **pShader) {
  if (sign_mask == 0) {
    return GetCompiledShader(pShader);
  }
  if (with_input_layout_fixup_.contains(sign_mask)) {
    *pShader = with_input_layout_fixup_[sign_mask].ref();
  } else {
    IMTLCompiledShader *shader = new AirconvShaderWithInputLayoutFixup<tag>(
        this->m_parent, this, sign_mask);
    shader->SubmitWork();
    with_input_layout_fixup_.emplace(sign_mask, shader);
    *pShader = ref(shader);
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
                                       pShaderBytecode, BytecodeLength));
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

  void GetCompiledShader(IMTLCompiledShader **ppShader) final {
    // D3D11_ASSERT(0 && "should not call this function");
    *ppShader = nullptr;
  };

  void GetCompiledShaderWithInputLayerFixup(uint64_t,
                                            IMTLCompiledShader **) final {
    // D3D11_ASSERT(0 && "should not call this function");
  };

  void GetReflection(MTL_SHADER_REFLECTION **pRefl) final {
    D3D11_ASSERT(0 && "should not call this function");
    *pRefl = &reflection;
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

HRESULT CreateDummyHullShader(IMTLD3D11Device *pDevice,
                              const void *pShaderBytecode,
                              SIZE_T BytecodeLength,
                              ID3D11HullShader **ppShader) {
  return CreateDummyShaderInternal<tag_hull_shader>(pDevice, pShaderBytecode,
                                                    BytecodeLength, ppShader);
}

HRESULT CreateDummyDomainShader(IMTLD3D11Device *pDevice,
                                const void *pShaderBytecode,
                                SIZE_T BytecodeLength,
                                ID3D11DomainShader **ppShader) {
  return CreateDummyShaderInternal<tag_domain_shader>(pDevice, pShaderBytecode,
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

} // namespace dxmt