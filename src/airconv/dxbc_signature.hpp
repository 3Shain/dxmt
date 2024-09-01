#pragma once

#include "DXBCParser/DXBCUtils.h"
#include "DXBCParser/ShaderBinary.h"
#include "airconv_public.h"
#include "dxbc_constants.hpp"
#include <string>

namespace dxmt::dxbc {

class Signature {

public:
  Signature(const microsoft::D3D11_SIGNATURE_PARAMETER &parameter)
      : parameter(parameter) {}

  std::string_view semanticName() { return parameter.SemanticName; }

  uint32_t semanticIndex() { return parameter.SemanticIndex; }

  std::string fullSemanticString() {
    return std::string(parameter.SemanticName) +
           std::to_string(parameter.SemanticIndex);
  }

  uint32_t stream() { return parameter.Stream; }

  uint32_t mask() { return parameter.Mask; }

  uint32_t reg() { return parameter.Register; }

  bool isSystemValue() {
    return parameter.SystemValue != microsoft::D3D10_SB_NAME_UNDEFINED;
  }

  RegisterComponentType componentType() {
    return (RegisterComponentType)parameter.ComponentType;
  }

private:
  const microsoft::D3D11_SIGNATURE_PARAMETER &parameter;
};

static_assert(
  sizeof(Signature) == sizeof(void *), "ensure inline implementation"
);

void handle_signature(
  microsoft::CSignatureParser &inputParser,
  microsoft::CSignatureParser &outputParser,
  microsoft::D3D10ShaderBinary::CInstruction &Inst, SM50Shader *sm50_shader,
  uint32_t phase
);
} // namespace dxmt::dxbc