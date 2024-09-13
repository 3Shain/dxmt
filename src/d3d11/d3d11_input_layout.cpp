#include "d3d11_input_layout.hpp"
#include "d3d11_device_child.hpp"
#include "DXBCParser/DXBCUtils.h"
#include "d3d11_state_object.hpp"
#include "util_math.hpp"
#include "log/log.hpp"

namespace dxmt {

class MTLD3D11InputLayout final
    : public ManagedDeviceChild<IMTLD3D11InputLayout> {
public:
  MTLD3D11InputLayout(
      IMTLD3D11Device *device,
      std::vector<MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC> &&attributes,
      uint32_t input_slot_mask)
      : ManagedDeviceChild<IMTLD3D11InputLayout>(device),
        attributes_(attributes), input_slot_mask_(input_slot_mask) {}

  ~MTLD3D11InputLayout() {}

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid,
                                           void **ppvObject) final {
    if (ppvObject == nullptr)
      return E_POINTER;

    *ppvObject = nullptr;

    if (riid == __uuidof(IUnknown) || riid == __uuidof(ID3D11DeviceChild) ||
        riid == __uuidof(ID3D11InputLayout) ||
        riid == __uuidof(IMTLD3D11InputLayout)) {
      *ppvObject = ref(this);
      return S_OK;
    }

    if (logQueryInterfaceError(__uuidof(ID3D11InputLayout), riid)) {
      WARN("D3D311InputLayout: Unknown interface query ", str::format(riid));
    }

    return E_NOINTERFACE;
  };

  virtual uint32_t STDMETHODCALLTYPE GetInputSlotMask() final {
    return input_slot_mask_;
  }

  virtual uint32_t STDMETHODCALLTYPE GetInputLayoutElements(
      MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC **ppElements) final {
    *ppElements = attributes_.data();
    return attributes_.size();
  }

private:
  std::vector<MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC> attributes_;
  uint64_t sign_mask_;
  uint32_t input_slot_mask_;
};

HRESULT ExtractMTLInputLayoutElements(
    IMTLD3D11Device *device, const void *pShaderBytecodeWithInputSignature,
    const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, uint32_t NumElements,
    MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC *pInputLayout,
    uint32_t *pNumElementsOut) {

  using namespace microsoft;
  uint16_t append_offset[32] = {0};

  CSignatureParser parser;
  HRESULT hr =
      DXBCGetInputSignature(pShaderBytecodeWithInputSignature, &parser);
  if (FAILED(hr)) {
    return hr;
  }
  const D3D11_SIGNATURE_PARAMETER *pParamters;
  auto num_parameters = parser.GetParameters(&pParamters);

  UINT attribute_count = 0;
  for (UINT i = 0; i < num_parameters; i++) {
    auto &inputSig = pParamters[i];
    if (inputSig.SystemValue != D3D10_SB_NAME_UNDEFINED) {
      continue; // ignore SIV & SGV
    }
    auto pDesc = std::find_if(
        pInputElementDescs, pInputElementDescs + NumElements,
        [&](const D3D11_INPUT_ELEMENT_DESC &Ele) {
          return Ele.SemanticIndex == inputSig.SemanticIndex &&
                 strcasecmp(Ele.SemanticName, inputSig.SemanticName) == 0;
        });
    if (pDesc == pInputElementDescs + NumElements) {
      // Unmatched shader input signature
      ERR("CreateInputLayout: Vertex shader expects ", inputSig.SemanticName,
          "_", inputSig.SemanticIndex, " but it's not in pInputElementDescs");
      return E_FAIL;
    }
    auto &desc = *pDesc;
    D3D11_ASSERT(attribute_count < NumElements);
    auto &attribute = pInputLayout[attribute_count++];

    Com<IMTLDXGIAdatper> dxgi_adapter;
    device->GetAdapter(&dxgi_adapter);
    MTL_FORMAT_DESC metal_format;
    if (FAILED(dxgi_adapter->QueryFormatDesc(pDesc->Format, &metal_format))) {
      ERR("CreateInputLayout: Unsupported vertex format ", desc.Format);
      return E_FAIL;
    }

    if (metal_format.VertexFormat == MTL::VertexFormatInvalid) {
      ERR("CreateInputLayout: Unsupported vertex format ", desc.Format);
      return E_INVALIDARG;
    }
    attribute.format = metal_format.AttributeFormat;

    attribute.slot = desc.InputSlot;
    attribute.index = inputSig.Register;

    if (desc.AlignedByteOffset == D3D11_APPEND_ALIGNED_ELEMENT) {
      attribute.offset = align(append_offset[attribute.slot],
                               std::min(4u, metal_format.BytesPerTexel));
    } else {
      attribute.offset = desc.AlignedByteOffset;
    }
    append_offset[attribute.slot] =
        attribute.offset + metal_format.BytesPerTexel;
    // the layout stride is provided in IASetVertexBuffer
    attribute.step_function = desc.InputSlotClass;
    attribute.step_rate = desc.InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA
                              ? desc.InstanceDataStepRate
                              : 1;
  }
  *pNumElementsOut = attribute_count;

  return S_OK;
}

template <>
HRESULT StateObjectCache<MTL_INPUT_LAYOUT_DESC, IMTLD3D11InputLayout>::
    CreateStateObject(const MTL_INPUT_LAYOUT_DESC *pInputLayoutDesc,
                      IMTLD3D11InputLayout **ppInputLayout) {
  std::lock_guard<dxmt::mutex> lock(mutex_cache);
  InitReturnPtr(ppInputLayout);

  if (!pInputLayoutDesc)
    return E_INVALIDARG;

  if (!ppInputLayout)
    return S_FALSE;

  if (cache.contains(*pInputLayoutDesc)) {
    cache.at(*pInputLayoutDesc)->QueryInterface(IID_PPV_ARGS(ppInputLayout));
    return S_OK;
  }

  std::vector<MTL_SHADER_INPUT_LAYOUT_ELEMENT_DESC> elements =
      *pInputLayoutDesc;
  uint32_t input_slot_mask = 0;
  for (auto &element : elements) {
    input_slot_mask |= (1 << element.slot);
  }

  cache.emplace(*pInputLayoutDesc,
                std::make_unique<MTLD3D11InputLayout>(
                    device, std::move(elements), input_slot_mask));
  cache.at(*pInputLayoutDesc)->QueryInterface(IID_PPV_ARGS(ppInputLayout));

  return S_OK;
}

} // namespace dxmt