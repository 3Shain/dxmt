#pragma once

#include "com/com_guid.hpp"
#include "d3d11_device.hpp"
#include "util_hash.hpp"

struct MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC {
  uint32_t Index = 0xffffffff;
  uint32_t Slot;
  uint32_t Offset;
  uint32_t Format; // the same as WMTAttributeFormat
  D3D11_INPUT_CLASSIFICATION StepFunction : 1;
  uint32_t InstanceStepRate : 31 = 0;
};

namespace dxmt {
class InputLayout;
};
typedef dxmt::InputLayout *ManagedInputLayout;

DEFINE_COM_INTERFACE("b56c6a99-80cf-4c7f-a756-9e9ceb38730f",
                     IMTLD3D11InputLayout)
    : public ID3D11InputLayout {
  virtual ManagedInputLayout GetManagedInputLayout() = 0;
};

#ifdef DXMT_NATIVE
DEFINE_GUID(IID_IMTLD3D11InputLayout, 0xb56c6a99, 0x80cf, 0x4c7f, 0xa7, 0x56, 0x9e, 0x9c, 0xeb, 0x38, 0x73, 0x0f);
__CRT_UUID_DECL(IMTLD3D11InputLayout, 0xb56c6a99, 0x80cf, 0x4c7f, 0xa7, 0x56, 0x9e, 0x9c, 0xeb, 0x38, 0x73, 0x0f);
#endif

struct MTL_SHADER_STREAM_OUTPUT_ELEMENT_DESC {
  uint32_t Register;
  uint32_t Component;
  uint32_t OutputSlot;
  uint32_t Offset;
};

DEFINE_COM_INTERFACE("fd58f76b-7c22-4605-b43c-28048c8b4a64",
                     IMTLD3D11StreamOutputLayout)
    : public ID3D11GeometryShader {
  virtual uint32_t STDMETHODCALLTYPE GetStreamOutputElements(
      MTL_SHADER_STREAM_OUTPUT_ELEMENT_DESC * *ppElements,
      uint32_t Strides[4]) = 0;
};

#ifdef DXMT_NATIVE
DEFINE_GUID(IID_IMTLD3D11StreamOutputLayout, 0xfd58f76b, 0x7c22, 0x4605, 0xb4, 0x3c, 0x28, 0x04, 0x8c, 0x8b, 0x4a, 0x64);
__CRT_UUID_DECL(IMTLD3D11StreamOutputLayout, 0xfd58f76b, 0x7c22, 0x4605, 0xb4, 0x3c, 0x28, 0x04, 0x8c, 0x8b, 0x4a, 0x64);
#endif

namespace dxmt {

class InputLayout {
public:
  virtual uint32_t input_slot_mask() = 0;
  virtual uint32_t
  input_layout_element(MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC **ppElements) = 0;
};

HRESULT ExtractMTLInputLayoutElements(
    MTLD3D11Device *device, const void *pShaderBytecodeWithInputSignature,
    const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, uint32_t NumElements,
    MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC *pInputLayoutElementOut,
    uint32_t *pNumElementsOut);

HRESULT ExtractMTLStreamOutputElements(
    MTLD3D11Device *pDevice, const void *pShaderBytecode, UINT NumEntries,
    const D3D11_SO_DECLARATION_ENTRY *pEntries,
    MTL_SHADER_STREAM_OUTPUT_ELEMENT_DESC *pStreamOut,
    uint32_t *pNumElementsOut);

} // namespace dxmt

using MTL_INPUT_LAYOUT_DESC = std::vector<MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC>;

struct MTL_STREAM_OUTPUT_DESC {
  uint32_t Strides[4];
  std::vector<MTL_SHADER_STREAM_OUTPUT_ELEMENT_DESC> Elements;
};

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

template <> struct hash<MTL_STREAM_OUTPUT_DESC> {
  size_t operator()(const MTL_STREAM_OUTPUT_DESC &v) const noexcept {
    dxmt::HashState state;
    state.add(v.Strides[0]);
    state.add(v.Strides[1]);
    state.add(v.Strides[2]);
    state.add(v.Strides[3]);
    for (auto &element : v.Elements) {
      state.add(std::hash<string_view>{}(
          {reinterpret_cast<const char *>(&element), sizeof(element)}));
    }
    return state;
  };
};

template <> struct equal_to<MTL_STREAM_OUTPUT_DESC> {
  bool operator()(const MTL_STREAM_OUTPUT_DESC &x,
                  const MTL_STREAM_OUTPUT_DESC &y) const {
    auto binsize =
        x.Elements.size() * sizeof(MTL_SHADER_STREAM_OUTPUT_ELEMENT_DESC);
    return x.Strides[0] == y.Strides[0] && x.Strides[1] == y.Strides[1] &&
           x.Strides[2] == y.Strides[2] && x.Strides[3] == y.Strides[3] &&
           x.Elements.size() == y.Elements.size() &&
           (std::string_view(
                {reinterpret_cast<const char *>(x.Elements.data()), binsize}) ==
            std::string_view(
                {reinterpret_cast<const char *>(y.Elements.data()), binsize}));
  };
};

} // namespace std
