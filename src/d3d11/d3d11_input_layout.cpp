#include "d3d11_input_layout.hpp"
#include "DXBCParser/d3d12tokenizedprogramformat.hpp"
#include "Foundation/NSAutoreleasePool.hpp"
#include "d3d11_device_child.h"

#include "../dxgi/dxgi_format.hpp"

#include "DXBCParser/DXBCUtils.h"
#include <algorithm>

namespace dxmt {

struct Attribute {
  uint32_t index;
  uint32_t slot;
  uint32_t offset;
  MTL::AttributeFormat format; // the same as MTL::VertexFormat
  uint32_t element_stride;
};

struct Layout {
  // uint32_t stride;
  D3D11_INPUT_CLASSIFICATION step_function;
  uint32_t step_rate = 0;
};

class MTLD3D11InputLayout final
    : public MTLD3D11DeviceChild<IMTLD3D11InputLayout> {
public:
  MTLD3D11InputLayout(IMTLD3D11Device *device,
                      std::vector<Attribute> &&attributes,
                      std::vector<Layout> &&layouts)
      : MTLD3D11DeviceChild<IMTLD3D11InputLayout>(device),
        attributes_(attributes), layouts_(layouts) {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11InputLayout)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11InputLayout), riid)) {
      WARN("Unknown interface query", str::format(riid));
    }

    return E_NOINTERFACE;
  };

  virtual void STDMETHODCALLTYPE
  Bind(MTL::RenderPipelineDescriptor *desc,
       const std::array<UINT, 16> &strides) final {
    auto pool = transfer(NS::AutoreleasePool::alloc()->init());
    auto vertex_desc = (MTL::VertexDescriptor::vertexDescriptor());
    for (auto &attr : attributes_) {
      auto attr_desc = vertex_desc->attributes()->object(attr.index);
      attr_desc->setBufferIndex(attr.slot);
      attr_desc->setFormat((MTL::VertexFormat)attr.format);
      attr_desc->setOffset(attr.offset);
    }

    unsigned _slot = 0;
    for (auto &layout : layouts_) {
      unsigned slot = _slot++;
      if (layout.step_rate == 0)
        continue;
      auto layout_desc = vertex_desc->layouts()->object(slot);
      layout_desc->setStepRate(layout.step_rate);
      layout_desc->setStepFunction(layout.step_function ==
                                           D3D11_INPUT_PER_INSTANCE_DATA
                                       ? MTL::VertexStepFunctionPerInstance
                                       : MTL::VertexStepFunctionPerVertex);
      layout_desc->setStride(strides[slot]);
    }

    desc->setVertexDescriptor(vertex_desc);
  };
  virtual void STDMETHODCALLTYPE
  Bind(MTL::ComputePipelineDescriptor *desc,
       const std::array<UINT, 16> &strides) final{

  };

private:
  std::vector<Attribute> attributes_;
  std::vector<Layout> layouts_;
};

HRESULT CreateInputLayout(IMTLD3D11Device *device,
                          const void *pShaderBytecodeWithInputSignature,
                          const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
                          UINT NumElements, ID3D11InputLayout **ppInputLayout) {
  std::vector<Attribute> elements(NumElements);
  std::vector<Layout> layout(16);

  CSignatureParser parser;
  HRESULT hr =
      DXBCGetInputSignature(pShaderBytecodeWithInputSignature, &parser);
  if (FAILED(hr)) {
    return hr;
  }
  const D3D11_SIGNATURE_PARAMETER *pParamters;
  auto numParameteres = parser.GetParameters(&pParamters);

  UINT attributeCount = 0;
  for (UINT i = 0; i < numParameteres; i++) {
    auto &inputSig = pParamters[i];
    if (inputSig.SystemValue != D3D10_SB_NAME_UNDEFINED) {
      continue; // ignore SIV & SGV
    }
    auto pDesc = std::find_if(
        pInputElementDescs, pInputElementDescs + NumElements,
        [&](const D3D11_INPUT_ELEMENT_DESC &Ele) {
          return Ele.SemanticIndex == inputSig.SemanticIndex &&
                 std::strcmp(Ele.SemanticName, inputSig.SemanticName) == 0;
        });
    if (pDesc == pInputElementDescs + NumElements) {
      // Unmatched shader input signature
      ERR("CreateInputLayout: Vertex shader expects ", inputSig.SemanticName,
          "_", inputSig.SemanticIndex, " but it's not in pInputElementDescs");
      return E_FAIL;
    }
    auto &desc = *pDesc;
    auto &attribute = elements[attributeCount++];

    if (g_metal_format_map[desc.Format].vertex_format ==
        MTL::VertexFormatInvalid) {
      ERR("CreateInputLayout: Unsupported Vertex Format ", desc.Format);
      return E_INVALIDARG;
    }
    attribute.format = g_metal_format_map[desc.Format].attribute_format;
    attribute.element_stride = g_metal_format_map[desc.Format].stride;
    attribute.slot = desc.InputSlot;
    attribute.index = inputSig.Register;
    if (attribute.slot >= 16) {
      ERR("CreateInputLayout: InputSlot greater than 15 is not supported "
          "(yet)");
      return E_FAIL;
    }
    if (attribute.index > 30) {
      ERR("CreateInputLayout: FIXME: Too many elements.");
      return E_INVALIDARG;
    }

    if (desc.AlignedByteOffset == D3D11_APPEND_ALIGNED_ELEMENT) {
      Attribute last_attr_in_same_slot = {
          .slot = 0xffffffff, .offset = 0, .element_stride = 0};
      for (signed j = i - 1; j >= 0; j--) {
        if (elements[j].slot == //
            attribute.slot) {
          last_attr_in_same_slot = elements[j];
          break;
        }
      }
      uint32_t unaligned_offset =
          last_attr_in_same_slot.offset + last_attr_in_same_slot.element_stride;
      attribute.offset = unaligned_offset; // FIXME: this is problematic
    } else {
      attribute.offset = desc.AlignedByteOffset;
    }
    // the layout stride is provided in IASetVertexBuffer
    layout.at(attribute.slot).step_function = desc.InputSlotClass;
    layout.at(attribute.slot).step_rate =
        desc.InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA
            ? desc.InstanceDataStepRate
            : 1;
  }
  if (ppInputLayout == NULL) {
    return S_FALSE;
  }
  *ppInputLayout = ref(
      new MTLD3D11InputLayout(device, std::move(elements), std::move(layout)));
  return S_OK;
};

} // namespace dxmt