#pragma once

#include "llvm/ADT/StringRef.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/MDBuilder.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include <cstdint>
#include <vector>

#include "air_constants.h"

using String = llvm::StringRef;

namespace dxmt {

class AirMetadata {

public:
  AirMetadata(llvm::LLVMContext &context);

  void SetAirVersion();
  void SetLanguageVersion();

  // llvm::MDNode*

  // private:
  llvm::LLVMContext &context;
  llvm::MDBuilder builder;

  llvm::ConstantAsMetadata *createUnsignedInteger(uint32_t value);
  llvm::MDString *createString(llvm::StringRef string, llvm::StringRef prefix);
  // llvm::MDString *createString(llvm::StringRef string);
  llvm::MDString *createString(llvm::Twine string);
  llvm::MDNode *createInput(uint32_t index, air::EInput input,
                            llvm::StringRef argName);
  llvm::MDNode *createInput(uint32_t index, air::EInput input, air::EType type,
                            llvm::StringRef argName);

  llvm::MDNode *createUserVertexInput(uint32_t index, uint32_t location,
                                      air::EType type, llvm::StringRef argName);
  llvm::MDNode *createUserKernelInput(uint32_t index, uint32_t location,
                                      air::EType type, llvm::StringRef argName);

  llvm::MDNode *createTextureBinding(uint32_t index, uint32_t location,
                                     uint32_t arraySize, air::EType type,
                                     air::ETextureAccess access,
                                     llvm::StringRef argName);

  llvm::MDNode *createBufferBinding(uint32_t index, uint32_t location,
                                    uint32_t arraySize,
                                    air::EBufferAccess access,
                                    air::EBufferAddrSpace addrSpace,
                                    air::EType elementType,
                                    llvm::StringRef argName);

  /* always assume read access on constant addrspace */
  llvm::MDNode *createIndirectBufferBinding(
      uint32_t index, uint32_t bufferSize, uint32_t location,
      uint32_t arraySize,
      air::EBufferAccess access, air::EBufferAddrSpace addrSpace,
      llvm::MDNode *structTypeInfo, uint32_t structAlignSize,
      llvm::StringRef structTypeName, llvm::StringRef argName);

  llvm::MDNode *
  createBufferBinding(uint32_t index, uint32_t location, uint32_t arraySize,
                      air::EBufferAccess access,
                      air::EBufferAddrSpace addrSpace, uint32_t argTypeSize,
                      uint32_t argTypeAlignSize, llvm::StringRef typeName,
                      llvm::StringRef argName);

  llvm::MDNode *createSamplerBinding(uint32_t index, uint32_t location,
                                     uint32_t arraySize,
                                     llvm::StringRef argName);

  llvm::MDNode *createOutput(air::EOutput output, llvm::StringRef argName);
  llvm::MDNode *createOutput(air::EOutput output, air::EType type,
                             llvm::StringRef argName);

  llvm::MDNode *createUserVertexOutput(llvm::StringRef key, air::EType type,
                                       llvm::StringRef argName);

  llvm::MDNode *createRenderTargetOutput(uint32_t attachmentIndex,
                                         uint32_t idkIndex, air::EType type,
                                         llvm::StringRef argName);
  llvm::MDNode *createDepthTargetOutput(air::EDepthArgument depthArgument,
                                        llvm::StringRef argName);
  llvm::MDNode *createUserFragmentInput(uint32_t index, llvm::StringRef key,
                                        air::ESampleInterpolation interpolation,
                                        air::EType type, llvm::StringRef name);

  llvm::MDNode *createIndirectConstantBinding(uint32_t index, uint32_t location,
                                              uint32_t arraySize,
                                              llvm::StringRef typeName,
                                              llvm::StringRef argName);

  void appendStructFieldInfo(std::vector<llvm::Metadata *> metadata,
                             uint32_t byteOffset, uint32_t elementSize,
                             /* 0 if it's not an array */ uint32_t arraySize,
                             llvm::StringRef typeName,
                             llvm::StringRef fieldName,
                             llvm::MDNode *bindingInfo);
};

} // namespace dxmt