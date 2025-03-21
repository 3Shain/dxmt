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
      : semantic_name_(parameter.SemanticName),
        semantic_index_(parameter.SemanticIndex), stream_(parameter.Stream),
        mask_(parameter.Mask), register_(parameter.Register),
        system_value_(parameter.SystemValue),
        component_type_((RegisterComponentType)parameter.ComponentType) {}

  std::string_view semanticName() { return semantic_name_; }

  uint32_t semanticIndex() { return semantic_index_; }

  std::string fullSemanticString() {
    return semantic_name_ + std::to_string(semantic_index_);
  }

  uint32_t stream() { return stream_; }

  uint32_t mask() { return mask_; }

  uint32_t reg() { return register_; }

  bool isSystemValue() {
    return system_value_ != microsoft::D3D10_SB_NAME_UNDEFINED;
  }

  RegisterComponentType componentType() {
    return (RegisterComponentType)component_type_;
  }

private:
  std::string semantic_name_;
  uint8_t semantic_index_;
  uint8_t stream_;
  uint8_t mask_;
  uint8_t register_;
  microsoft::D3D10_SB_NAME system_value_;
  RegisterComponentType component_type_;
};

void handle_signature(
  microsoft::CSignatureParser &inputParser,
  microsoft::CSignatureParser5 &outputParser,
  microsoft::D3D10ShaderBinary::CInstruction &Inst, SM50Shader *sm50_shader,
  uint32_t phase
);
} // namespace dxmt::dxbc