#pragma once

#include "llvm_bitcode.hpp"
#include "dxil_container.hpp"
#include <string>
#include <sstream>
#include <optional>
#include <vector>
#include <unordered_map>

namespace dxmt::dxil {

struct MSLShader {
  std::string source;
  std::string entry_point;
  uint32_t tg_size[3] = {1, 1, 1};
  uint32_t num_uavs = 0;
  uint32_t num_srvs = 0;
  uint32_t num_cbuffers = 0;
  uint32_t num_samplers = 0;
  uint32_t unsupported_intrinsics = 0;
  uint32_t unsupported_opcodes = 0;
  std::vector<std::string> diagnostics;
};

struct ResourceBinding {
  uint32_t register_space;
  uint32_t register_index;
  uint32_t count;
  enum class Kind { SRV, UAV, CBuffer, Sampler } kind;
  std::string name;
};

class DXILToMSL {
public:
  static std::optional<MSLShader> convert(const LLVMModule &module,
                                           const DxilParsedShader &shader);

private:
  struct EmitContext {
    std::ostringstream &os;
    const LLVMModule &mod;
    const DxilParsedShader &shader;
    std::vector<std::string> value_table;
    std::unordered_map<std::string, std::string> local_values;
    std::vector<ResourceBinding> resource_bindings;
    std::vector<std::string> diagnostics;
    uint32_t next_binding = 0;
    uint32_t unsupported_intrinsics = 0;
    uint32_t unsupported_opcodes = 0;
    bool uses_thread_id = false;
    bool uses_group_id = false;
    bool uses_group_thread_id = false;
    bool uses_group_size = false;
  };

  static std::string getTypeName(const LLVMType &t, const LLVMModule &mod);
  static std::string getVectorTypeName(const LLVMType &elem_type, uint32_t count, const LLVMModule &mod);
  static uint32_t getTypeSize(const LLVMType &t, const LLVMModule &mod);
  static std::string emitValue(uint32_t idx);
  static std::string emitConstant(const std::vector<uint64_t> &ops, uint32_t type_id, const LLVMModule &mod);
  static void emitFunctionPrologue(EmitContext &ctx);
  static void emitBindings(EmitContext &ctx);
  static void emitInstruction(EmitContext &ctx, const LLVMInstruction &inst, uint32_t &value_counter);
  static std::string translateDXIntrinsic(EmitContext &ctx, uint32_t intrinsic_id,
                                           const std::vector<uint32_t> &args);
  static void recordDiagnostic(EmitContext &ctx, const char *fmt, ...);
};

}
