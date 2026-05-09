#include "dxil_to_msl.hpp"
#include <sstream>
#include <cstdio>

namespace dxmt::dxil {

static const char *kComputeHeader = R"(
#include <metal_stdlib>
using namespace metal;

)";

std::string DXILToMSL::opcodeName(LLVMInstruction::Opcode op) {
  switch (op) {
  case LLVMInstruction::Add: return "add";
  case LLVMInstruction::Sub: return "sub";
  case LLVMInstruction::Mul: return "mul";
  case LLVMInstruction::UDiv: return "udiv";
  case LLVMInstruction::SDiv: return "sdiv";
  case LLVMInstruction::FAdd: return "fadd";
  case LLVMInstruction::FSub: return "fsub";
  case LLVMInstruction::FMul: return "fmul";
  case LLVMInstruction::FDiv: return "fdiv";
  case LLVMInstruction::And: return "and";
  case LLVMInstruction::Or: return "or";
  case LLVMInstruction::Xor: return "xor";
  case LLVMInstruction::Shl: return "shl";
  case LLVMInstruction::LShr: return "lshr";
  case LLVMInstruction::AShr: return "ashr";
  case LLVMInstruction::Ret: return "ret";
  case LLVMInstruction::Br: return "br";
  case LLVMInstruction::Load: return "load";
  case LLVMInstruction::Store: return "store";
  case LLVMInstruction::Call: return "call";
  case LLVMInstruction::BitCast: return "bitcast";
  case LLVMInstruction::ZExt: return "zext";
  case LLVMInstruction::SExt: return "sext";
  case LLVMInstruction::Trunc: return "trunc";
  case LLVMInstruction::ICmp: return "icmp";
  case LLVMInstruction::FCmp: return "fcmp";
  case LLVMInstruction::Select: return "select";
  case LLVMInstruction::GetElementPtr: return "gep";
  default: return "unknown";
  }
}

std::string DXILToMSL::translateIntrinsic(const std::string &name,
                                            const std::vector<uint32_t> &operands,
                                            const LLVMModule &mod) {
  if (name.find("dx.op.") == std::string::npos)
    return name;

  if (name.find("threadId") != std::string::npos ||
      name.find("ThreadId") != std::string::npos) {
    return "thread_position_in_grid";
  }
  if (name.find("groupId") != std::string::npos ||
      name.find("GroupId") != std::string::npos) {
    return "threadgroup_position_in_grid";
  }
  if (name.find("bufferLoad") != std::string::npos) {
    return "dx_bufferLoad";
  }
  if (name.find("bufferStore") != std::string::npos) {
    return "dx_bufferStore";
  }
  if (name.find("barrier") != std::string::npos ||
      name.find("Barrier") != std::string::npos) {
    return "threadgroup_barrier(mem_flags::mem_threadgroup)";
  }
  if (name.find("createHandle") != std::string::npos) {
    return "dx_createHandle";
  }
  if (name.find("textureLoad") != std::string::npos) {
    return "dx_textureLoad";
  }
  if (name.find("textureStore") != std::string::npos) {
    return "dx_textureStore";
  }
  if (name.find("rawBufferLoad") != std::string::npos) {
    return "dx_rawBufferLoad";
  }
  if (name.find("rawBufferStore") != std::string::npos) {
    return "dx_rawBufferStore";
  }
  if (name.find("atomic") != std::string::npos) {
    return "dx_atomic";
  }

  return name;
}

std::optional<MSLShader> DXILToMSL::convert(const LLVMModule &module,
                                              const DxilParsedShader &shader) {
  std::ostringstream os;
  os << kComputeHeader;

  os << "kernel void " << shader.entry_point << "(\n";
  os << "  uint3 dtid [[thread_position_in_grid]],\n";
  os << "  uint3 gtidx [[thread_position_in_threadgroup]],\n";
  os << "  uint3 ggid [[threadgroup_position_in_grid]],\n";
  os << "  uint3 gsz [[threads_per_threadgroup]]\n";
  os << ") {\n";

  if (module.functions.empty()) {
    os << "  // No functions parsed from DXIL\n";
    os << "}\n";
    MSLShader result;
    result.source = os.str();
    result.entry_point = shader.entry_point;
    return result;
  }

  auto &fn = module.functions.back();

  for (auto &block : fn.blocks) {
    for (auto &inst : block.instructions) {
      os << "  // ";
      switch (inst.opcode) {
      case LLVMInstruction::Call:
        os << "call";
        if (inst.operands.size() >= 2) {
          os << " fn=" << inst.operands[0];
          os << " nargs=" << (inst.operands.size() - 1);
        }
        break;
      case LLVMInstruction::Ret:
        os << "ret";
        break;
      case LLVMInstruction::Load:
        os << "load ptr=" << (inst.operands.empty() ? 0 : inst.operands[0]);
        break;
      case LLVMInstruction::Store:
        os << "store ptr=" << (inst.operands.empty() ? 0 : inst.operands[0]);
        break;
      default:
        os << opcodeName(inst.opcode);
        for (auto &op : inst.operands)
          os << " " << op;
        break;
      }
      os << "\n";
    }
  }

  os << "}\n";

  MSLShader result;
  result.source = os.str();
  result.entry_point = shader.entry_point;
  return result;
}

}
