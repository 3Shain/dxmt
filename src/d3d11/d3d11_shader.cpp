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
              MTL_SHADER_REFLECTION &reflection,
              Obj<MTL::ArgumentEncoder> encoder, const void *pShaderBytecode,
              SIZE_T BytecodeLength)
      : MTLD3D11DeviceChild<typename tag::COM, IMTLD3D11Shader>(device),
        sm50(sm50), reflection(reflection), encoder_(std::move(encoder)),
        dump_len(BytecodeLength) {
    id = ++global_id;
    dump = malloc(BytecodeLength);
    memcpy(dump, pShaderBytecode, BytecodeLength);
  }

  ~TShaderBase() {
    if (sm50) {
      SM50Destroy(sm50);
      sm50 = nullptr;
    }
    free(dump);
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

  void GetCompiledShader(void *pArgs, IMTLCompiledShader **pShader) final;

  void GetArgumentEncoderRef(MTL::ArgumentEncoder **pEncoder) final;

  void GetReflection(MTL_SHADER_REFLECTION **pRefl) final {
    *pRefl = &reflection;
  }

  SM50Shader *sm50;
  MTL_SHADER_REFLECTION reflection;
  Com<IMTLCompiledShader> precompiled_;
  Obj<MTL::ArgumentEncoder> encoder_;
  uint64_t id;
  void *dump;
  uint64_t dump_len;
};

template <typename tag>
class ContextlessShader : public ComObject<IMTLCompiledShader> {
public:
  ContextlessShader(IMTLD3D11Device *pDevice, TShaderBase<tag> *shader)
      : ComObject<IMTLCompiledShader>(), device_(pDevice), shader_(shader) {
    shader_->AddRef(); // ???
    pDevice->SubmitThreadgroupWork(this, &work_state_);
  }

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
    std::fstream dump_out;
    dump_out.open("shader_dump_" + std::to_string(shader_->id) + ".cso",
                  std::ios::out | std::ios::binary);
    if (dump_out) {
      dump_out.write((char *)shader_->dump, shader_->dump_len);
    }
    dump_out.close();
    ERR("dumped to ./shader_dump_" + std::to_string(shader_->id) + ".cso");
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
      if (auto ret = SM50Compile(shader_->sm50, nullptr, func_name.c_str(),
                                 &compile_result, &sm50_err)) {
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
};

template <typename tag>
void TShaderBase<tag>::GetCompiledShader(void *pArgs,
                                         IMTLCompiledShader **pShader) {
  // pArgs not used at the moment
  if (precompiled_) {
    *pShader = precompiled_.ref();
    return;
  }
  *pShader = ref(new ContextlessShader(this->m_parent, this));
}

template <typename tag>
void TShaderBase<tag>::GetArgumentEncoderRef(MTL::ArgumentEncoder **pEncoder) {
  if (pEncoder) {
    *pEncoder = encoder_.ptr();
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
  auto encoder_ =
      transfer(SM50CreateArgumentEncoder(sm50, pDevice->GetMTLDevice()));
  *ppShader = ref(new TShaderBase<tag>(pDevice, sm50, reflection, encoder_,
                                       pShaderBytecode, BytecodeLength));
  // FIXME: this looks weird but don't change it for now
  ((TShaderBase<tag> *)*ppShader)
      ->GetCompiledShader(NULL, &((TShaderBase<tag> *)*ppShader)->precompiled_);
  return S_OK;
}

template <typename tag>
class TDummyShaderBase
    : public MTLD3D11DeviceChild<typename tag::COM, IMTLD3D11Shader> {
public:
  TDummyShaderBase(IMTLD3D11Device *device, const void *pShaderBytecode,
                   SIZE_T BytecodeLength)
      : MTLD3D11DeviceChild<typename tag::COM, IMTLD3D11Shader>(device),
        dump_len(BytecodeLength) {
    id = ++global_id;
    dump = malloc(BytecodeLength);
    memcpy(dump, pShaderBytecode, BytecodeLength);
  }

  ~TDummyShaderBase() { free(dump); }

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

  void GetCompiledShader(void *pArgs, IMTLCompiledShader **pShader) final {
    D3D11_ASSERT(0 && "should not call this function");
    *pShader = nullptr;
  };

  void GetArgumentEncoderRef(MTL::ArgumentEncoder **pEncoder) final {
    D3D11_ASSERT(0 && "should not call this function");
    *pEncoder = nullptr;
  };

  void GetReflection(MTL_SHADER_REFLECTION **pRefl) final {
    D3D11_ASSERT(0 && "should not call this function");
    *pRefl = &reflection;
  }

  MTL_SHADER_REFLECTION reflection{};
  uint64_t id;
  void *dump;
  uint64_t dump_len;
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