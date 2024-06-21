#pragma once

#include <bitset>

namespace dxmt {

template <typename E> struct redunant_binding_trait {
  static bool is_redunant(const E &left, const E &right);
};

/**
Element is required to be move-assignable
 */
template <typename Element, size_t NumElements> class BindingSet {
  std::bitset<NumElements> dirty;
  std::bitset<NumElements> bound;
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

  inline Element &at(uint64_t index) noexcept { return storage[index]; };

  inline Element &operator[](uint64_t index) noexcept {
    assert(bound.test(index));
    return storage[index];
  };

  inline bool test_bound(size_t slot) const noexcept {
    return bound.test(slot);
  };

  inline bool test_dirty(size_t slot) const noexcept {
    return dirty.test(slot);
  };

  inline bool any_dirty() const noexcept { return dirty.any(); }

  inline void clear_dirty() { dirty.reset(); }

  inline void clear_dirty(size_t slot) { dirty.reset(slot); }

  inline void set_dirty() { dirty = bound; };

  inline void set_dirty(size_t slot) { dirty.set(slot); };

  /**
  try to bind element at specific slot, and return a reference to the
  corresponding element storage it also tells if a replacement does happen
  this is useful because we don't have to fully initalize the element
  (so no initialization overhead if no replacement)
  */
  inline Element &bind(size_t slot, Element &&element, bool &replacement) {
    if (bound.test(slot)) {
      if (redunant_binding_trait<Element>::is_redunant(storage[slot],
                                                       element)) {
        return storage[slot];
      }
    } else {
      bound.set(slot);
    }
    // new (storage.data() + slot) Element(std::forward<Element>(element));
    // std::construct_at(storage.data() + slot, std::forward<Element>(element));
    // idk why placement construction kills performance
    storage[slot] = std::forward<Element>(element);
    dirty.set(slot);
    replacement = true;
    return storage[slot];
  };

  inline void unbind(size_t slot) {
    if (bound.test(slot)) {
      // storage[slot].~Element();
      // yeah that's weird but since we don't use placement construction
      // (because it's slow-as-f for some weird reason)
      // and Com<T> (typically inside Element) doesn't reset pointer at
      // destruction then a redunant Release occur when trying to move Element
      // at bind()
      storage[slot] = {};
      bound.reset(slot);
      dirty.set(slot);
    }
  }
};
} // namespace dxmt