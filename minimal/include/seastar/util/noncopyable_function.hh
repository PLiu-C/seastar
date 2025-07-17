#pragma once

#include <functional>
#include <memory>
#include <type_traits>

namespace seastar {

template <typename Signature>
class noncopyable_function;

template <typename R, typename... Args>
class noncopyable_function<R(Args...)> {
private:
    std::function<R(Args...)> _func;

public:
    noncopyable_function() = default;
    noncopyable_function(const noncopyable_function&) = delete;
    noncopyable_function& operator=(const noncopyable_function&) = delete;
    noncopyable_function(noncopyable_function&&) = default;
    noncopyable_function& operator=(noncopyable_function&&) = default;

    template <typename F>
    noncopyable_function(F&& f) : _func(std::forward<F>(f)) {}

    R operator()(Args... args) const {
        return _func(args...);
    }

    explicit operator bool() const {
        return bool(_func);
    }
};

} // namespace seastar 