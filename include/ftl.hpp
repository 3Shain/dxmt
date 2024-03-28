#pragma once
#include <utility>
#include <vector>
#include <functional>

template <class Tp, typename Func>
auto operator | (const std::vector<Tp> &vec, Func &&f)  {
  using R = decltype(f(std::declval<Tp>()));
  std::vector<R> ret;
  std::transform(vec.begin(), vec.end(), std::back_inserter(ret), f);
  return ret;
};

template <class Tp, typename Func>
auto operator | (const std::vector<Tp> &vec, const Func &f)  {
  using R = decltype(f(std::declval<Tp>()));
  std::vector<R> ret;
  std::transform(vec.begin(), vec.end(), std::back_inserter(ret), f);
  return ret;
};

template <typename Env, typename V> class ReaderIO {
public:
  // ReaderIO(ReaderIO &&other) = default;
  // ReaderIO(const ReaderIO &copy) = default;

  ReaderIO(std::function<V(Env)> &&ff) : run(ff) {}

  V build(Env ir) const { return run(ir); };

private:
  std::function<V(Env)> run;
};

/* bind */
template <typename Env, typename V, typename Func>
auto operator>>=(const ReaderIO<Env, V>& src, Func &&fn) {
  using R = ReaderIO<Env,
                  decltype(fn(std::declval<V>()).build(std::declval<Env>()))>;
  return R(
      [=, fn=std::function<R(V)>(fn)](auto context) { return fn(src.build(context)).build(context); });
}

/* map */
template <typename Env, typename V, typename Func>
auto operator | (const ReaderIO<Env, V>& src, Func &&fn) {
  using R = ReaderIO<Env,
                  decltype(fn(std::declval<V>()))>;
  return R(
      [=, fn=std::function<decltype(fn(std::declval<V>()))(V)>(fn)](auto context) { return fn(src.build(context)); });
}

template <typename Env, typename A>
ReaderIO<Env, A>& operator<<(ReaderIO<Env, A>& a, const ReaderIO<Env, A>& b) {
  a = a >>= [b=b](auto) { return b; };
  return a;
};


template <typename Env, typename A, typename B, typename Func>
auto lift(const ReaderIO<Env,A>& a, const ReaderIO<Env,B>& b, Func&& func) {
  return a >>= [=](auto a) {
    return b >>= [=](auto b) {
      return func(a,b);
    };
  };
};

template <typename Env, typename A, typename B, typename C,typename Func>
auto lift(const ReaderIO<Env,A>& a, const ReaderIO<Env,B>& b, const ReaderIO<Env,C>& c,  Func&& func) {
  return a >>= [=](auto a) {
    return b >>= [=](auto b) {
      return c >>= [=](auto c) {
        return func(a,b,c);
      };
    };
  };
};

template <typename Env, typename A, typename B, typename C, typename D,typename Func>
auto lift(const ReaderIO<Env,A>& a, const ReaderIO<Env,B>& b, const ReaderIO<Env,C>& c,  const ReaderIO<Env,D>& d, Func&& func) {
  return a >>= [=](auto a) {
    return b >>= [=](auto b) {
      return c >>= [=](auto c) {
        return d >>= [=](auto d) {
          return func(a,b,c,d);
        };
      };
    };
  };
};