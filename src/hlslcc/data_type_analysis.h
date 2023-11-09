#pragma once

#include "shader_info.h"
#include <vector>

class HLSLCrossCompilerContext;
struct Instruction;

namespace dxmt {
namespace DataTypeAnalysis {
void SetDataTypes(HLSLCrossCompilerContext *psContext,
                  std::vector<Instruction> &instructions,
                  uint32_t ui32TempCount,
                  std::vector<SHADER_VARIABLE_TYPE> &results);
}
} // namespace dxmt
