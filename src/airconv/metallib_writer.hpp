#pragma once
#include "llvm/IR/Module.h"

namespace dxmt {

class MetallibWriter {

public:
void Write(const llvm::Module &module);

};

}