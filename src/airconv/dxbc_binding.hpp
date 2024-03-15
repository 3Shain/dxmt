#pragma once

#include "air_constants.hpp"
#include "llvm/IR/Value.h"

namespace dxmt::dxbc {

class IDxbcBindingDeclare {

public:
  /* s[slot]*/
  virtual void DeclareSampler(uint32_t slot) = 0;
  /* cb[slot]*/
  virtual void DeclareConstantBuffer(uint32_t slot, uint32_t size) = 0;
  /* u[slot]*/
  virtual void DeclareTexture(uint32_t slot, air::ETextureType type,
                              air::ETextureAccessType accessType,
                              bool unorderedAccess) = 0;
  /* u[slot]*/
  virtual void DeclareBuffer(uint32_t slot, bool unorderedAccess, bool withCounter) = 0;

  virtual void MarkTextureAsDepth(uint32_t slot);

  // To extend SM 5.1 model: space and range

  virtual void Initialize() = 0;
};

class IDxbcBindingResolve {
  virtual llvm::Value *AccessConstantBuffer(uint32_t) = 0;
  virtual llvm::Value *AccessSampler(uint32_t) = 0;
  virtual llvm::Value *AccessTexture(uint32_t) = 0;
};

std::unique_ptr<IDxbcBindingDeclare> CreateSM50Binding();

// std::unique_ptr<IDxbcBinding> CreateSM51Binding(); // should provide
// reflection info for root sig

} // namespace dxmt::dxbc