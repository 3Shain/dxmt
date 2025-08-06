

#include <cstddef>
#include <cstring>
#include <exception>
#include <limits>
#include <type_traits>
#include <variant>

namespace dxmt {

template <typename... Types> class PackedVariantList {
  static_assert(
    (std::is_trivially_copyable_v<Types> && ...),
    "all types must be trivially copyable"
  );

  using Tag = uint32_t;
  static constexpr size_t NumTypes = sizeof...(Types);
  static_assert(
    NumTypes <= std::numeric_limits<Tag>::max(),
    "too many types for Tag dispatch"
  );

  std::byte *data = nullptr;
  size_t capacity = 0;
  size_t size = 0;

  template <typename T, typename Tx, typename... Rest>
  static constexpr size_t variant_index() {
    if constexpr (std::is_same_v<T, Tx>)
      return 0;
    else
      return 1 + variant_index<T, Rest...>();
  }

  template <typename T> static constexpr Tag tag_of() {
    return static_cast<Tag>(variant_index<T, Types...>());
  }

  template <typename Visitor>
  auto dispatch(size_t &offset, Visitor &&visitor) const {
    Tag tag = *reinterpret_cast<Tag *>(data + offset);
    return dispatch_impl<0, Types...>(
      tag, offset, std::forward<Visitor>(visitor)
    );
  }

  template <size_t Index, typename T, typename... Rest, typename Visitor>
  auto dispatch_impl(Tag tag, size_t &offset, Visitor &&visitor) const {
    if (tag == Index) {
      size_t data_offset = align_up(offset + sizeof(Tag), alignof(T));
      offset = align_up(data_offset + sizeof(T), sizeof(Tag));
      return visitor(*reinterpret_cast<const T *>(data + data_offset));
    } else if constexpr (sizeof...(Rest) > 0) {
      return dispatch_impl<Index + 1, Rest...>(
        tag, offset, std::forward<Visitor>(visitor)
      );
    } else {
      std::terminate(); // bad tag
    }
  }

  static size_t align_up(size_t offset, size_t alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
  }

  void ensure_capacity(size_t required) {
    if (required <= capacity)
      return;

    size_t new_capacity = capacity ? capacity : 1024;
    while (new_capacity < required) {
      new_capacity = new_capacity + 1024;
    }

    std::byte *new_data = static_cast<std::byte *>(std::malloc(new_capacity));
    if (data) {
      std::memcpy(new_data, data, size);
      std::free(data);
    }

    data = new_data;
    capacity = new_capacity;
  }

public:
  using variant = std::variant<Types...>;

  PackedVariantList() = default;
  ~PackedVariantList() { std::free(data); }

  PackedVariantList(const PackedVariantList &) = delete;
  PackedVariantList &operator=(const PackedVariantList &) = delete;

  template <typename T> void push_back(const T &value) {
    static_assert((std::is_same_v<T, Types> || ...), "invalid type");
    size_t offset = size;
    size_t data_offset = align_up(offset + sizeof(Tag), alignof(T));
    size_t total_size = align_up(data_offset + sizeof(T), sizeof(Tag));

    ensure_capacity(total_size);

    *reinterpret_cast<Tag *>(data + offset) = tag_of<T>();
    *reinterpret_cast<T *>(data + data_offset) = value;

    size = total_size;
  }

  void push_back(const variant &var) {
    std::visit([this](const auto &val) { this->push_back(val); }, var);
  }

  template <typename Visitor> void for_each(Visitor &&visitor) const {
    size_t offset = 0;

    while (offset < size) {
      dispatch(offset, visitor);
    }
  }

  template <typename Visitor> bool any(Visitor &&visitor) const {
    size_t offset = 0;

    while (offset < size) {
      if (dispatch(offset, visitor))
        return true;
    }
    return false;
  }
};
} // namespace dxmt