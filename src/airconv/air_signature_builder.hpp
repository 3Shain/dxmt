#pragma once

#include "air_constants.hpp"
#include "air_metadata.hpp"
#include "air_type.hpp"
#include "llvm/IR/Metadata.h"
#include <cstdint>
#include <unordered_map>

namespace dxmt::air {

class AirFunctionSignatureBuilder {
public:
  /* For inputs, the return value is the index of llvm::Function* -> args() */

  uint32_t DefineAttributeInput(EInput input);
  uint32_t DefineUserInput();
  uint32_t DefineInputStructureBuffer(EBufferAddrSpace addrSpace, uint32_t slot,
                                      llvm::MDNode *metadata);

  /* For inputs, the return value is the element index of return struct type */

  uint32_t DefineAttributeOutput(EOutput output);
  uint32_t DefineUserVertexOutput(std::string id);
  uint32_t DefineUserFragmentOutput(uint32_t renderTargetIndex);

  auto Build() -> std::tuple<llvm::Type *, llvm::MDNode /* input */,
                             llvm::MDNode /* output */>;

  AirFunctionSignatureBuilder();

private:
  std::unordered_map<EInput, uint32_t> inputAttributeMap_;
  std::unordered_map<EOutput, uint32_t> outputAttributeMap_;
  uint32_t inputElementCount_ = 0;
  uint32_t outputElementCount_ = 0;
};

class AirStructureSignatureBuilder {
  /* the return value is the element index of structure (as well as in argument
   * buffer) */
public:
  uint32_t DefineBuffer(EBufferAddrSpace addrSpace);
  uint32_t DefineTexture(ETextureKind kind, ETextureAccessType access);
  uint32_t DefineSampler();
  uint32_t DefineInteger32();
  uint32_t DefineFloat32();

  auto Build() -> std::tuple<llvm::Type *, llvm::MDNode>;

  AirStructureSignatureBuilder(AirType &types, AirMetadata& metadata);

private:
  AirType &types;
  AirMetadata &metadata;
  std::vector<llvm::Type *> fieldsType;
  uint32_t elementCount_ = 0;
};
} // namespace dxmt::air