#pragma once

#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Metadata.h"
#include "llvm/Support/Error.h"

#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace llvm {
class Module;
class NamedMDNode;
} // namespace llvm

namespace dxmt::dxil {

inline llvm::Error
metadata_error(llvm::Twine message) {
  return llvm::make_error<llvm::StringError>(message, llvm::inconvertibleErrorCode());
}

class MetadataReader;

template <typename T> struct MetadataValue;

template <> struct MetadataValue<std::string> {
  static llvm::Expected<std::string>
  get(const llvm::Metadata *md) {
    if (const auto *s = llvm::dyn_cast_or_null<llvm::MDString>(md))
      return s->getString().str();
    return metadata_error("expected string metadata");
  }
};

template <> struct MetadataValue<uint32_t> {
  static llvm::Expected<uint32_t>
  get(const llvm::Metadata *md) {
    if (const auto *m = llvm::dyn_cast_or_null<llvm::ConstantAsMetadata>(md)) {
      if (const auto *c = llvm::dyn_cast<llvm::ConstantInt>(m->getValue())) {
        uint64_t v = c->getZExtValue();
        if (v <= UINT32_MAX)
          return static_cast<uint32_t>(v);
        return metadata_error("integer out of uint32 range");
      }
    }
    return metadata_error("expected integer metadata");
  }
};

template <> struct MetadataValue<uint64_t> {
  static llvm::Expected<uint64_t>
  get(const llvm::Metadata *md) {
    if (const auto *m = llvm::dyn_cast_or_null<llvm::ConstantAsMetadata>(md)) {
      if (const auto *c = llvm::dyn_cast<llvm::ConstantInt>(m->getValue()))
        return c->getZExtValue();
    }
    return metadata_error("expected integer metadata");
  }
};

template <> struct MetadataValue<int32_t> {
  static llvm::Expected<int32_t>
  get(const llvm::Metadata *md) {
    if (const auto *m = llvm::dyn_cast_or_null<llvm::ConstantAsMetadata>(md)) {
      if (const auto *c = llvm::dyn_cast<llvm::ConstantInt>(m->getValue())) {
        int64_t v = c->getSExtValue();
        if (v >= INT32_MIN && v <= INT32_MAX)
          return static_cast<int32_t>(v);
        return metadata_error("integer out of int32 range");
      }
    }
    return metadata_error("expected integer metadata");
  }
};

template <> struct MetadataValue<bool> {
  static llvm::Expected<bool>
  get(const llvm::Metadata *md) {
    if (const auto *m = llvm::dyn_cast_or_null<llvm::ConstantAsMetadata>(md)) {
      if (const auto *c = llvm::dyn_cast<llvm::ConstantInt>(m->getValue()))
        return c->isOneValue() || !c->isZeroValue();
    }
    return metadata_error("expected boolean metadata");
  }
};

struct Ignore {};

template <> struct MetadataValue<Ignore> {
  static llvm::Expected<Ignore>
  get(const llvm::Metadata *) {
    return Ignore{};
  }
};

template <> struct MetadataValue<std::vector<uint32_t>> {
  static llvm::Expected<std::vector<uint32_t>>
  get(const llvm::Metadata *md) {
    std::vector<uint32_t> out;
    if (const auto *m = llvm::dyn_cast_or_null<llvm::ConstantAsMetadata>(md)) {
      if (const auto *seq = llvm::dyn_cast<llvm::ConstantDataSequential>(m->getValue())) {
        out.reserve(seq->getNumElements());
        for (unsigned i = 0; i < seq->getNumElements(); ++i)
          out.push_back(static_cast<uint32_t>(seq->getElementAsInteger(i)));
        return out;
      }
    }
    if (const auto *tuple = llvm::dyn_cast_or_null<llvm::MDTuple>(md)) {
      out.reserve(tuple->getNumOperands());
      for (const llvm::MDOperand &op : tuple->operands()) {
        auto v = MetadataValue<uint32_t>::get(op.get());
        if (!v)
          return v.takeError();
        out.push_back(*v);
      }
      return out;
    }
    return metadata_error("expected array metadata");
  }
};

template <> struct MetadataValue<const llvm::Function *> {
  static llvm::Expected<const llvm::Function *>
  get(const llvm::Metadata *md) {
    if (const auto *m = llvm::dyn_cast_or_null<llvm::ConstantAsMetadata>(md))
      if (const auto *f = llvm::dyn_cast<llvm::Function>(m->getValue()))
        return f;
    return metadata_error("expected function metadata");
  }
};

class MetadataReader {
public:
  MetadataReader() : md_(static_cast<const llvm::Metadata *>(nullptr)) {}
  MetadataReader(MetadataReader &&) = default;
  MetadataReader &
  operator=(MetadataReader &&other) {
    if (!md_)
      llvm::consumeError(md_.takeError());
    md_ = std::move(other.md_);
    return *this;
  }
  ~MetadataReader() {
    if (!md_)
      llvm::consumeError(md_.takeError());
  }

  static bool hasNamed(const llvm::Module &mod, llvm::StringRef name);

  static llvm::Expected<size_t> namedOperandCount(const llvm::Module &mod, llvm::StringRef name);

  static MetadataReader fromNamed(const llvm::Module &mod, llvm::StringRef name, uint32_t operand_idx = 0);
  static MetadataReader of(const llvm::Metadata *md);

  bool
  isNull() {
    return md_ && *md_ == nullptr;
  }
  bool
  hasError() {
    return !md_;
  }
  bool
  isNullOrError() {
    return !md_ || *md_ == nullptr;
  }

  MetadataReader operand(unsigned idx);
  llvm::Expected<size_t> operandCount();
  MetadataReader assertOperandCount(unsigned count);
  MetadataReader slice(unsigned start, unsigned length);

  llvm::Expected<std::string>
  asString() {
    return asType<std::string>();
  }
  llvm::Expected<uint32_t>
  asUint() {
    return asType<uint32_t>();
  }
  llvm::Expected<uint64_t>
  asUint64() {
    return asType<uint64_t>();
  }

  llvm::Expected<const llvm::Function *>
  asFunction() {
    return asType<const llvm::Function *>();
  }

  template <typename... Ts> llvm::Expected<std::tuple<Ts...>> asTuple();

  template <typename... Ts, typename Fn> llvm::Expected<std::invoke_result_t<Fn, Ts...>> asMappedTuple(Fn &&fn);

  template <typename T> llvm::Expected<T> asType();

private:
  explicit MetadataReader(llvm::Error err) : md_(std::move(err)) {}
  explicit MetadataReader(const llvm::Metadata *md) : md_(md) {}

  static MetadataReader
  failed(llvm::Twine message) {
    return MetadataReader(metadata_error(message));
  }

  llvm::Expected<const llvm::Metadata *> md_;

  template <typename... Ts, size_t... Is>
  static llvm::Expected<std::tuple<Ts...>> buildTuple(const llvm::MDTuple *tuple, std::index_sequence<Is...>);
};

template <typename T>
llvm::Expected<T>
MetadataReader::asType() {
  if (!md_)
    return md_.takeError();
  return MetadataValue<T>::get(*md_);
}

template <typename... Ts>
llvm::Expected<std::tuple<Ts...>>
MetadataReader::asTuple() {
  if (!md_)
    return md_.takeError();
  if (!llvm::isa<llvm::MDTuple>(*md_))
    return metadata_error("expected tuple metadata");
  const auto *tuple = llvm::cast<llvm::MDTuple>(*md_);
  if (tuple->getNumOperands() != sizeof...(Ts))
    return metadata_error("tuple operand count mismatch");
  return buildTuple<Ts...>(tuple, std::index_sequence_for<Ts...>{});
}

template <typename... Ts, typename Fn>
llvm::Expected<std::invoke_result_t<Fn, Ts...>>
MetadataReader::asMappedTuple(Fn &&fn) {
  auto tuple = asTuple<Ts...>();
  if (!tuple)
    return tuple.takeError();
  return std::apply(std::forward<Fn>(fn), std::move(*tuple));
}

template <> struct MetadataValue<MetadataReader> {
  static llvm::Expected<MetadataReader>
  get(const llvm::Metadata *md) {
    return MetadataReader::of(md);
  }
};

template <typename... Ts, size_t... Is>
llvm::Expected<std::tuple<Ts...>>
MetadataReader::buildTuple(const llvm::MDTuple *tuple, std::index_sequence<Is...>) {
  using Tuple = std::tuple<Ts...>;
  Tuple out;
  std::optional<llvm::Error> err;
  auto one = [&](auto ic) {
    if (err)
      return;
    constexpr size_t I = decltype(ic)::value;
    auto v = MetadataValue<std::tuple_element_t<I, Tuple>>::get(tuple->getOperand(I).get());
    if (!v) {
      err = v.takeError();
      return;
    }
    std::get<I>(out) = std::move(*v);
  };
  (one(std::integral_constant<size_t, Is>{}), ...);
  if (err)
    return std::move(*err);
  return out;
}

} // namespace dxmt::dxil
