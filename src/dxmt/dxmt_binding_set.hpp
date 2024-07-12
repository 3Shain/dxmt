#pragma once

#include "util_bit.hpp"

namespace dxmt {

template <typename E> struct redunant_binding_trait {
  static bool is_redunant(const E &left, const E &right);
};

/**
Element is required to be move-assignable
 */
template <typename Element, size_t NumElements> class BindingSet {
  bit::bitset<NumElements> dirty;
  bit::bitset<NumElements> bound;
  std::array<Element, NumElements> storage;

public:
  BindingSet() {};
  BindingSet(const BindingSet &copy) = delete; // doesn't make sense here
  BindingSet(BindingSet &&move) : storage(std::move(move.storage)) {
    bound = move.bound;
    dirty = move.bound; // intended behavior
  };

  BindingSet &operator=(BindingSet &&move) {
    storage = std::move(move.storage);
    bound = move.bound;
    dirty = move.bound; // intended behavior
    return *this;
  }

  constexpr Element &at(uint64_t index) noexcept { return storage[index]; };

  constexpr Element &operator[](uint64_t index) noexcept {
    assert(bound.get(index));
    return storage[index];
  };

  constexpr bool test_bound(size_t slot) const noexcept {
    return bound.get(slot);
  };

  constexpr bool test_dirty(size_t slot) const noexcept {
    return dirty.get(slot);
  };

  constexpr bool any_dirty() const noexcept { return dirty.any(); }

  constexpr bool any_dirty_masked(uint16_t mask) noexcept {
    return (dirty.qword(0) & (uint64_t)mask) != 0;
  }

  constexpr bool any_dirty_masked(uint64_t mask) noexcept {
    return (dirty.qword(0) & mask) != 0;
  }

  constexpr bool any_dirty_masked(uint64_t mask_hi, uint64_t mask_lo) noexcept {
    return ((dirty.qword(0) & mask_lo) | (dirty.qword(1) & mask_hi)) != 0;
  }

  inline void clear_dirty() { dirty.clearAll(); }

  inline void clear_dirty(size_t slot) { dirty.set(slot, false); }

  inline void set_dirty() { dirty = bound; };

  inline void set_dirty(size_t slot) { dirty.set(slot, true); };

  /**
  try to bind element at specific slot, and return a reference to the
  corresponding element storage it also tells if a replacement does happen
  this is useful because we don't have to fully initalize the element
  (so no initialization overhead if no replacement)
  */
  inline Element &bind(size_t slot, Element &&element, bool &replacement) {
    if (bound.get(slot)) {
      if (redunant_binding_trait<Element>::is_redunant(storage[slot],
                                                       element)) {
        return storage[slot];
      }
    } else {
      bound.set(slot, true);
    }
    // new (storage.data() + slot) Element(std::forward<Element>(element));
    // std::construct_at(storage.data() + slot, std::forward<Element>(element));
    // idk why placement construction kills performance
    storage[slot] = std::forward<Element>(element);
    dirty.set(slot, true);
    replacement = true;
    return storage[slot];
  };

  inline void unbind(size_t slot) {
    if (bound.get(slot)) {
      // storage[slot].~Element();
      // yeah that's weird but since we don't use placement construction
      // (because it's slow-as-f for some weird reason)
      // and Com<T> (typically inside Element) doesn't reset pointer at
      // destruction then a redunant Release occur when trying to move Element
      // at bind()
      storage[slot] = {};
      bound.set(slot, false);
      dirty.set(slot, true);
    }
  }
};
} // namespace dxmt