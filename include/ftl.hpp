#pragma once
#include <type_traits>
#include <utility>
#include <vector>
#include <algorithm>

template <class Tp, typename Func>
auto operator | (const std::vector<Tp> &vec, Func &&f)  {
  using R = decltype(f(std::declval<Tp>()));
  std::vector<R> ret;
  std::transform(vec.begin(), vec.end(), std::back_inserter(ret), std::forward<Func>(f));
  return ret;
};

template <class Tp, typename Func>
auto operator | (const std::vector<Tp> &vec, const Func &f)  {
  using R = decltype(f(std::declval<Tp>()));
  std::vector<R> ret;
  std::transform(vec.begin(), vec.end(), std::back_inserter(ret), f);
  return ret;
};

template<class T, std::enable_if_t<std::is_enum_v<T>, std::underlying_type_t<T>> = 0>
constexpr T operator|(T lhs, T rhs) 
{
  return static_cast<T>(
    static_cast<std::underlying_type<T>::type>(lhs) | 
    static_cast<std::underlying_type<T>::type>(rhs));
}

template<class T, std::enable_if_t<std::is_enum_v<T>, std::underlying_type_t<T>> = 0>
constexpr T operator&(T lhs, T rhs) 
{
  return static_cast<T>(
    static_cast<std::underlying_type<T>::type>(lhs) &
    static_cast<std::underlying_type<T>::type>(rhs));
}

template<class T, std::enable_if_t<std::is_enum_v<T>, std::underlying_type_t<T>> = 0>
constexpr T operator~(T lhs) 
{
  return static_cast<T>(~static_cast<std::underlying_type<T>::type>(lhs));
}

template<class T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
constexpr bool any_bit_set(T lhs) 
{
  return static_cast<std::underlying_type<T>::type>(lhs) != 0;
}

template<class T, std::enable_if_t<std::is_enum_v<T>, std::underlying_type_t<T>> = 0>
constexpr T& operator|=(T& lhs, T rhs)
{
  lhs = static_cast<T>(
    static_cast<std::underlying_type<T>::type>(lhs) | 
    static_cast<std::underlying_type<T>::type>(rhs));
  return lhs;
}
