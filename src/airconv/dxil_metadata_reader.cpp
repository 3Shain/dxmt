#include "dxil_metadata_reader.hpp"

#include "llvm/IR/Module.h"

using namespace llvm;

namespace dxmt::dxil {

bool
MetadataReader::hasNamed(const Module &mod, StringRef name) {
  return mod.getNamedMetadata(name) != nullptr;
}

Expected<size_t>
MetadataReader::namedOperandCount(const Module &mod, StringRef name) {
  if (const NamedMDNode *named = mod.getNamedMetadata(name))
    return named->getNumOperands();
  return metadata_error("missing named metadata '" + name + "'");
}

MetadataReader
MetadataReader::fromNamed(const Module &mod, StringRef name, uint32_t operand_idx) {
  const NamedMDNode *named = mod.getNamedMetadata(name);
  if (!named)
    return failed("missing named metadata '" + name + "'");
  if (operand_idx >= named->getNumOperands())
    return failed("named metadata '" + name + "' operand index out of range");
  return MetadataReader(named->getOperand(operand_idx));
}

MetadataReader
MetadataReader::of(const Metadata *md) {
  return MetadataReader(md);
}

MetadataReader
MetadataReader::operand(unsigned idx) {
  if (!md_)
    return MetadataReader(md_.takeError());
  if (*md_ == nullptr)
    return failed("cannot take operand of null metadata");
  if (!isa<MDTuple>(*md_))
    return failed("cannot take operand of non-tuple metadata");
  const MDTuple *tuple = cast<MDTuple>(*md_);
  if (idx >= tuple->getNumOperands())
    return failed("tuple operand index out of range");
  return MetadataReader(tuple->getOperand(idx).get());
}

Expected<size_t>
MetadataReader::operandCount() {
  if (!md_)
    return md_.takeError();
  if (*md_ == nullptr)
    return metadata_error("cannot get operand count of null metadata");
  if (isa<MDTuple>(*md_))
    return cast<MDTuple>(*md_)->getNumOperands();
  return metadata_error("expected tuple metadata");
}

MetadataReader
MetadataReader::assertOperandCount(unsigned count) {
  auto n = operandCount();
  if (!n)
    return failed(llvm::toString(n.takeError()));
  if (*n != count)
    return failed("unexpected operand count");
  return std::move(*this);
}

MetadataReader
MetadataReader::slice(unsigned start, unsigned length) {
  if (!md_)
    return MetadataReader(md_.takeError());
  if (!isa<MDTuple>(*md_))
    return failed("slice requires tuple metadata");
  const MDTuple *tuple = cast<MDTuple>(*md_);
  if (start + length > tuple->getNumOperands())
    return failed("slice range out of bounds");
  std::vector<Metadata *> operands;
  operands.reserve(length);
  for (unsigned i = start; i < start + length; ++i)
    operands.push_back(tuple->getOperand(i).get());
  return MetadataReader(MDTuple::get(tuple->getContext(), operands));
}

} // namespace dxmt::dxil
