#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>

namespace dxmt::dxil {

struct LLVMType {
  enum Kind {
    Void,
    Float,
    Double,
    Integer,
    Pointer,
    Struct,
    Array,
    Vector,
    Function,
  } kind;
  uint32_t bit_width = 0;
  std::vector<LLVMType> subtypes;
  std::vector<uint32_t> type_refs;
};

struct LLVMValue {
  enum Kind {
    Undef,
    Constant,
    Instruction,
    Argument,
    BasicBlock,
    Function,
  } kind;
  uint32_t type_id = 0;
  uint32_t id = 0;
  std::string name;
  std::string constant_data;
};

struct LLVMInstruction {
  enum Opcode {
    Ret = 1,
    Br = 2,
    Switch = 3,
    Invoke = 4,
    Unreachable = 8,
    Add = 9,
    Sub = 11,
    Mul = 13,
    UDiv = 15,
    SDiv = 17,
    URem = 19,
    SRem = 21,
    And = 23,
    Or = 24,
    Xor = 25,
    Shl = 26,
    LShr = 27,
    AShr = 28,
    FAdd = 29,
    FSub = 30,
    FMul = 31,
    FDiv = 32,
    FRem = 33,
    FNeg = 34,
    ExtractValue = 42,
    InsertValue = 43,
    ExtractElement = 44,
    InsertElement = 45,
    ShuffleVector = 46,
    BitCast = 53,
    ZExt = 55,
    SExt = 56,
    Trunc = 57,
    FPToUI = 58,
    FPToSI = 59,
    UIToFP = 60,
    SIToFP = 61,
    FPTrunc = 62,
    FPExt = 63,
    PtrToInt = 64,
    IntToPtr = 65,
    ICmp = 68,
    FCmp = 69,
    PHI = 71,
    Call = 72,
    Select = 73,
    GEP = 76,
    Load = 81,
    Store = 82,
    Alloca = 83,
    GetElementPtr = 84,
  } opcode;

  uint32_t type_id = 0;
  uint32_t result_id = 0;
  std::vector<uint32_t> operands;
};

struct LLVMBasicBlock {
  std::string name;
  std::vector<LLVMInstruction> instructions;
};

struct LLVMFunction {
  std::string name;
  uint32_t value_id = 0;
  uint32_t type_id = 0;
  uint32_t calling_conv = 0;
  bool is_declaration = true;
  uint32_t param_count = 0;
  uint32_t instruction_start_value = 0;
  std::vector<LLVMType> param_types;
  LLVMType return_type;
  std::vector<LLVMBasicBlock> blocks;
  std::vector<std::string> attributes;
};

struct LLVMModule {
  std::vector<LLVMType> types;
  std::vector<LLVMValue> constants;
  std::vector<LLVMFunction> functions;
  std::unordered_map<std::string, size_t> function_map;
  std::string source_filename;
  std::string target_triple;
};

class BitcodeReader {
public:
  static std::optional<LLVMModule> parse(const uint8_t *data, uint32_t size);

private:
  BitcodeReader() = default;
};

}
