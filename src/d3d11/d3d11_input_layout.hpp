#pragma once

#include "com/com_guid.hpp"
#include "d3d11_device.hpp"
#include "util_hash.hpp"

struct MTL_SHADER_INPUT_LAYOUT_FIXUP {
  uint64_t sign_mask;
};

struct MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC {
  uint32_t index = 0xffffffff;
  uint32_t slot;
  uint32_t offset;
  uint32_t format; // the same as MTL::VertexFormat
  D3D11_INPUT_CLASSIFICATION step_function : 1;
  uint32_t step_rate : 31 = 0;
};

DEFINE_COM_INTERFACE("b56c6a99-80cf-4c7f-a756-9e9ceb38730f",
                     IMTLD3D11InputLayout)
    : public ID3D11InputLayout {
  virtual uint32_t STDMETHODCALLTYPE GetInputSlotMask() = 0;
  virtual uint32_t STDMETHODCALLTYPE GetInputLayoutElements(
      MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC * *ppElements) = 0;
};

namespace dxmt {

HRESULT ExtractMTLInputLayoutElements(
    IMTLD3D11Device *device, const void *pShaderBytecodeWithInputSignature,
    const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, uint32_t NumElements,
    MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC *pInputLayoutElementOut,
    uint32_t *pNumElementsOut);

} // namespace dxmt

using MTL_INPUT_LAYOUT_DESC = std::vector<MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC>;

namespace std {
template <> struct hash<MTL_INPUT_LAYOUT_DESC> {
  size_t operator()(const MTL_INPUT_LAYOUT_DESC &v) const noexcept {
    dxmt::HashState state;
    for (auto &element : v) {
      state.add(std::hash<string_view>{}(
          {reinterpret_cast<const char *>(&element), sizeof(element)}));
    }
    return state;
  };
};

template <> struct equal_to<MTL_INPUT_LAYOUT_DESC> {
  bool operator()(const MTL_INPUT_LAYOUT_DESC &x,
                  const MTL_INPUT_LAYOUT_DESC &y) const {
    auto binsize = x.size() * sizeof(MTL_INPUT_LAYOUT_DESC::value_type);
    return x.size() == y.size() &&
           (std::string_view(
                {reinterpret_cast<const char *>(x.data()), binsize}) ==
            std::string_view(
                {reinterpret_cast<const char *>(y.data()), binsize}));
  };
};

} // namespace std
