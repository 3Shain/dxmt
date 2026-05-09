#pragma once

#include "llvm_bitcode.hpp"
#include "dxil_container.hpp"
#include <string>
#include <optional>

namespace dxmt::dxil {

struct MSLShader {
  std::string source;
  std::string entry_point;
};

class DXILToMSL {
public:
  static std::string opcodeName(LLVMInstruction::Opcode op);
  static std::string translateIntrinsic(const std::string &name,
                                         const std::vector<uint32_t> &operands,
                                         const LLVMModule &mod);
  static std::optional<MSLShader> convert(const LLVMModule &module,
                                           const DxilParsedShader &shader);

private:
  static std::string getTypeName(const LLVMType &t, const LLVMModule &mod);
};

}
