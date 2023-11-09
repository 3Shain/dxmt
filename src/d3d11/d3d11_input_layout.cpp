#include "d3d11_input_layout.hpp"
#include "Foundation/NSAutoreleasePool.hpp"
#include "d3d11_device_child.h"

#include "../dxgi/dxgi_format.hpp"

#include "../hlslcc/shader.h"

namespace dxmt {

struct Attribute {
  uint32_t metal_attribute;
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
      auto attr_desc = vertex_desc->attributes()->object(attr.metal_attribute);
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

  DXBCShader *shader =
      DecodeDXBCShader(pShaderBytecodeWithInputSignature); // FIXME: release!
  ShaderInfo *info;
  GetDXBCShaderInfo(shader, &info);

  auto find_metal_attribute = [&](const D3D11_INPUT_ELEMENT_DESC &desc) {
    for (auto &sig : info->inputSignatures) {
      if (sig.semanticName == desc.SemanticName) {
        if (sig.semanticIndex == desc.SemanticIndex) {
          return sig.vertexAttribute;
        }
      }
    }
    throw MTLD3DError("Unmatched signature");
    return 0xffffffff;
  };

  for (UINT i = 0; i < NumElements; i++) {
    auto &desc = pInputElementDescs[i];
    if (g_metal_format_map[desc.Format].vertex_format ==
        MTL::VertexFormatInvalid) {
      ERR("Unsupported Vertex Format ", desc.Format);
      return E_INVALIDARG;
    }
    if (desc.InputSlot >= 16) {
      ERR("InputSlot greater than 15 is not supported (yet)");
      return E_FAIL;
    }
    auto &attribute = elements[i];
    attribute.metal_attribute = find_metal_attribute(desc);
    if (attribute.metal_attribute > 31) {
      ERR("Unmatched input signature");
      return E_INVALIDARG;
    }
    attribute.format = g_metal_format_map[desc.Format].attribute_format;
    attribute.element_stride = g_metal_format_map[desc.Format].stride;
    attribute.slot = desc.InputSlot;

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

// Obj<MTL::StageInputOutputDescriptor>
// MTLD3D11InputLayout::CreateStageIODescriptor(
//     UINT strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT],
//     DXGI_FORMAT index_format) {
//   auto mtl_vertex_desc =
//       transfer(MTL::StageInputOutputDescriptor::stageInputOutputDescriptor());
//   if (index_format != DXGI_FORMAT_UNKNOWN) {
//     // indexBuffer exist, so bind it to 16 (convention)
//     mtl_vertex_desc->setIndexBufferIndex(16);
//     if (index_format == DXGI_FORMAT_R16_UINT) {
//       mtl_vertex_desc->setIndexType(MTL::IndexType::IndexTypeUInt16);
//     } else {
//       mtl_vertex_desc->setIndexType(MTL::IndexType::IndexTypeUInt32);
//     }
//   }

//   D3D11_INPUT_CLASSIFICATION
//   input_slot_classes[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
//   UINT instance_data_step_rates[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];

//   for (unsigned i = 0; i < num_elements_; i++) {
//     auto mtl_attr_desc = transfer(MTL::AttributeDescriptor::alloc()->init());
//     mtl_attr_desc->setBufferIndex(input_element_descs_[i].InputSlot);
//     mtl_attr_desc->setOffset(input_element_descs_[i].AlignedByteOffset);

//     mtl_attr_desc->setFormat(
//         g_metal_format_map[input_element_descs_[i].Format].attribute_format);
//     mtl_vertex_desc->attributes()->setObject(mtl_attr_desc.ptr(), i);

//     input_slot_classes[input_element_descs_[i].InputSlot] =
//         input_element_descs_[i].InputSlotClass;
//     instance_data_step_rates[input_element_descs_[i].InputSlot] =
//         input_element_descs_[i].InstanceDataStepRate;
//   }

//   for (unsigned slot = 0; slot < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
//        slot++) {
//     if (strides[slot] == 0)
//       continue;
//     auto mtl_buffer_layout_desc =
//         transfer(MTL::BufferLayoutDescriptor::alloc()->init());
//     mtl_buffer_layout_desc->setStride(strides[slot]);
//     if (input_slot_classes[slot] == D3D11_INPUT_PER_INSTANCE_DATA) {
//       mtl_buffer_layout_desc->setStepFunction(
//           MTL::StepFunctionThreadPositionInGridY);
//       mtl_buffer_layout_desc->setStepRate(instance_data_step_rates[slot]);
//     } else {
//       // D3D11_INPUT_PER_VERTEX_DATA
//       mtl_buffer_layout_desc->setStepFunction(
//           MTL::StepFunctionThreadPositionInGridX);
//     }
//     mtl_vertex_desc->layouts()->setObject(mtl_buffer_layout_desc.ptr(),
//     slot);
//   }
//   return mtl_vertex_desc;
// }

} // namespace dxmt