#pragma once
#include "d3d11_private.h"
#include "d3d11_device_child.h"
#include "../util/sha1/sha1_util.h"
#include <cstdlib>

namespace dxmt {
template <typename Base, typename Reflection>
class MTLBaseShader : public MTLD3D11DeviceChild<Base> {
public:
  friend class MTLD3D11DeviceContext;

  MTLBaseShader(IMTLD3D11Device *pDevice, Sha1Hash Hash, const void *bytecode,
                SIZE_T bytecodeLength)
      : MTLD3D11DeviceChild<Base>(pDevice), hash(Hash) {
    m_bytecode = malloc(bytecodeLength);
    assert(m_bytecode);
    memcpy(m_bytecode, bytecode, bytecodeLength);
    m_bytecodeLength = bytecodeLength;
  }
  ~MTLBaseShader() { free(m_bytecode); }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) {
    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(Base)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(Base), riid)) {
      WARN("Unknown interface query", str::format(riid));
    }

    return E_NOINTERFACE;
  }

private:
  void *m_bytecode;
  SIZE_T m_bytecodeLength;
  Sha1Hash hash;

public:
};

using MTLD3D11VertexShader = MTLBaseShader<ID3D11VertexShader, nullptr_t>;
using MTLD3D11HullShader = MTLBaseShader<ID3D11HullShader, nullptr_t>;
using MTLD3D11DomainShader = MTLBaseShader<ID3D11DomainShader, nullptr_t>;
using MTLD3D11GeometryShader = MTLBaseShader<ID3D11GeometryShader, nullptr_t>;
using MTLD3D11PixelShader = MTLBaseShader<ID3D11PixelShader, nullptr_t>;
using MTLD3D11ComputeShader = MTLBaseShader<ID3D11ComputeShader, nullptr_t>;
} // namespace dxmt