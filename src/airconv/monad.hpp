#pragma once
#include "llvm/Support/Error.h"
#include <coroutine>
#include <functional>
#include <type_traits>
#include <utility>

template <typename T, typename G>
concept MoveAndInvocable =
  std::is_object_v<T> && std::move_constructible<T> && std::invocable<T, G>;

template <typename Test, template <typename...> class Ref>
struct is_specialization : std::false_type {};

template <template <typename...> class Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref> : std::true_type {};

template <typename Env, typename V> class ReaderIO {

  class BaseFunction {
  public:
    virtual llvm::Expected<V> invoke(Env env) = 0;
    virtual ~BaseFunction() {};
  };

  template <MoveAndInvocable<Env> Fn>
  class ErasureFunction : public BaseFunction {
  public:
    llvm::Expected<V> invoke(Env env) override { return std::invoke(fn, env); };
    ErasureFunction(Fn &&ff) : fn(std::forward<Fn>(ff)) {}
    ~ErasureFunction() = default;
    ErasureFunction(const ErasureFunction &copy) = delete;
    ErasureFunction &operator=(const ErasureFunction &copy_assign) = delete;

  private:
    Fn fn;
  };

public:
  template <std::invocable<Env> T> ReaderIO(T &&ff) {
    factory = new ErasureFunction<T>(std::forward<T>(ff));
  }
  ~ReaderIO() {
    if (factory != nullptr) {
      delete factory;
    }
  }

  ReaderIO(ReaderIO &&other) {
    factory = other.factory;
    other.factory = nullptr;
  };
  ReaderIO &operator=(ReaderIO &&move_assign) {
    factory = move_assign.factory;
    move_assign.factory = nullptr;
    return *this;
  };
  ReaderIO(const ReaderIO &copy) = delete;
  ReaderIO &operator=(const ReaderIO &copy_assign) = delete;

  llvm::Expected<V> build(Env ir) {
    assert(factory && "value has been consumed or moved.");
    auto ret = factory->invoke(ir);
    delete factory;
    factory = nullptr;
    return ret;
  };

  struct promise_type {
    Env *to_be_filled = nullptr;
    llvm::Expected<V> *return_value_ = nullptr;
    ReaderIO<Env, V> get_return_object() {
      return ReaderIO<Env, V>([this](Env ctx) {
        this->to_be_filled =
          &ctx; // I'm sure to_be_filled is only accessed within the scope?
        auto h = std::coroutine_handle<promise_type>::from_promise(*this);
        h.resume();
        assert(return_value_ && "no returnvalue");
        assert(
          (h.done() == bool(*return_value_)) &&
          "unexpected suspension of coroutine"
        );
        auto r = std::move(*return_value_);
        delete return_value_;
        return_value_ = nullptr;
        h.destroy(); // should I destroy?
        return r;
      });
    };

    std::suspend_always initial_suspend() { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
    void unhandled_exception() {}

    template <std::move_constructible ResumeVal> struct trivial_value_awaiter {
      llvm::Expected<ResumeVal> val;
      bool await_ready() noexcept { return bool(val); }
      void await_suspend(const std::coroutine_handle<promise_type> &h) {
        h.promise().return_value_ = new llvm::Expected<V>(val.takeError());
      }
      constexpr ResumeVal await_resume() const noexcept {
        return val.get(); // this function will be not called again?
        // thus val is not used anymore
      }
    };
    template <std::move_constructible S>
    trivial_value_awaiter<S> yield_value(ReaderIO<Env, S> &&s) {
      assert(to_be_filled);
      return {s.build(*to_be_filled)};
    };

    void return_value(V value) {
      return_value_ = new llvm::Expected<V>(value);
    };
  };

private:
  BaseFunction *factory;
};

/* bind */
template <typename Env, typename V, std::invocable<V> Func>
auto operator>>=(ReaderIO<Env, V> &&src, Func &&fn) {
  using ret_value_type =
    typename decltype(fn(std::declval<V>()).build(std::declval<Env>())
    )::value_type;
  using R = ReaderIO<Env, ret_value_type>;
  return R(
    [src0 = std::move(src), fn = std::forward<Func>(fn)](auto context
    ) mutable -> llvm::Expected<ret_value_type> {
      auto ret = src0.build(context);
      if (auto err = ret.takeError()) {
        return err;
      }
      return fn(ret.get()).build(context);
    }
  );
}

/* map */
template <typename Env, typename V, std::invocable<V> Func>
auto operator|(ReaderIO<Env, V> &&src, Func &&fn) {
  using ret_value_type = decltype(fn(std::declval<V>()));
  using R = ReaderIO<Env, ret_value_type>;
  return R(
    [src = std::move(src), fn = std::forward<Func>(fn)](auto context
    ) mutable -> llvm::Expected<ret_value_type> {
      auto ret = src.build(context);
      if (auto err = ret.takeError()) {
        return err;
      }
      return fn(ret.get());
    }
  );
}

/* in-place sequence: a = a then b */
template <typename Env, typename A>
ReaderIO<Env, A> &operator<<(ReaderIO<Env, A> &a, ReaderIO<Env, A> &&b) {
  a = ReaderIO<Env, A>(
    [a = std::move(a),
     b = std::move(b)](auto context) mutable -> llvm::Expected<A> {
      if (auto err = a.build(context).takeError()) {
        return err;
      }
      return b.build(context);
    }
  );
  return a;
};

/* in-place bind: a = fn(result of a) */
template <typename Env, typename V, std::invocable<V> Func>
ReaderIO<Env, V> &operator>>(ReaderIO<Env, V> &a, Func &&fn) {
  a = std::move(a) >>= std::forward<Func>(fn);
  return a;
};

template <typename Env, typename A, typename B, std::move_constructible Func>
auto lift(ReaderIO<Env, A> &&a, ReaderIO<Env, B> &&b, Func &&func) {
  return std::move(a) >>= [func = std::forward<Func>(func),
                           b = std::move(b)](auto a) mutable {
    return std::move(b) >>=
           [a, func = std::forward<Func>(func)](auto b) { return func(a, b); };
  };
};

template <typename Env, typename A, typename B, typename C, typename Func>
auto lift(
  ReaderIO<Env, A> &&a, ReaderIO<Env, B> &&b, ReaderIO<Env, C> &&c, Func &&func
) {
  return std::move(a) >>= [func = std::forward<Func>(func), b = std::move(b),
                           c = std::move(c)](auto a) mutable {
    return std::move(b) >>= [a, func = std::forward<Func>(func),
                             c = std::move(c)](auto b) mutable {
      return std::move(c) >>= [a, b, func = std::forward<Func>(func)](auto c) {
        return func(a, b, c);
      };
    };
  };
};

// template <typename Env, typename A, typename B, typename C, typename
// D,typename Func> auto lift(const ReaderIO<Env,A>& a, const ReaderIO<Env,B>&
// b, const ReaderIO<Env,C>& c,  const ReaderIO<Env,D>& d, Func&& func) {
//   return a >>= [=](auto a) {
//     return b >>= [=](auto b) {
//       return c >>= [=](auto c) {
//         return d >>= [=](auto d) {
//           return func(a,b,c,d);
//         };
//       };
//     };
//   };
// };
