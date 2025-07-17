#pragma once

#include <exception>
#include <memory>
#include <functional>

namespace seastar {

/// Exception type for broken promises
struct broken_promise : std::logic_error {
    broken_promise();
};

// Simple future implementation without complex templates
class future_base {
protected:
    enum class state { invalid, available, exception };
    state _state = state::invalid;
    std::exception_ptr _exception;

public:
    future_base() = default;
    virtual ~future_base() = default;
    
    bool available() const { return _state == state::available; }
    bool failed() const { return _state == state::exception; }
    
    void set_exception(std::exception_ptr ex) {
        _exception = ex;
        _state = state::exception;
    }
    
protected:
    void check_exception() const {
        if (_state == state::exception) {
            std::rethrow_exception(_exception);
        }
        if (_state != state::available) {
            throw std::runtime_error("Future not ready");
        }
    }
};

template <typename T>
class future : public future_base {
private:
    T _value;

public:
    future() = default;
    
    future(const future&) = default;  // Allow copy for simplicity
    future& operator=(const future&) = default;
    
    future(future&&) = default;
    future& operator=(future&&) = default;
    
    void set_value(const T& value) {
        _value = value;
        _state = state::available;
    }
    
    void set_value(T&& value) {
        _value = std::move(value);
        _state = state::available;
    }
    
    T get() {
        check_exception();
        return _value;
    }
    
    // Simplified then() - just immediate execution for ready futures
    template <typename Func>
    auto then(Func&& func) -> future<int> {  // Fixed return type for simplicity
        if (available()) {
            try {
                func(_value);
                future<int> result;
                result.set_value(0);  // Dummy value
                return result;
            } catch (...) {
                future<int> result;
                result.set_exception(std::current_exception());
                return result;
            }
        }
        
        future<int> result;
        result.set_exception(std::make_exception_ptr(
            std::runtime_error("Continuations not implemented")));
        return result;
    }
    
    template <typename Func>
    future<T> finally(Func&& func) {
        try {
            func();
        } catch (...) {
            // Ignore exceptions in finally blocks
        }
        return std::move(*this);
    }
};

// Specialization for void
template <>
class future<void> : public future_base {
public:
    future() = default;
    
    future(const future&) = default;  // Allow copy for simplicity
    future& operator=(const future&) = default;
    
    future(future&&) = default;
    future& operator=(future&&) = default;
    
    void set_value() {
        _state = state::available;
    }
    
    void get() {
        check_exception();
    }
    
    // Simplified then() - just immediate execution for ready futures
    template <typename Func>
    auto then(Func&& func) -> future<int> {  // Fixed return type for simplicity
        if (available()) {
            try {
                func();
                future<int> result;
                result.set_value(0);  // Dummy value
                return result;
            } catch (...) {
                future<int> result;
                result.set_exception(std::current_exception());
                return result;
            }
        }
        
        future<int> result;
        result.set_exception(std::make_exception_ptr(
            std::runtime_error("Continuations not implemented")));
        return result;
    }
    
    template <typename Func>
    future<void> finally(Func&& func) {
        try {
            func();
        } catch (...) {
            // Ignore exceptions in finally blocks
        }
        return std::move(*this);
    }
};

// Promise classes
template <typename T>
class promise {
private:
    std::shared_ptr<future<T>> _future;

public:
    promise() : _future(std::make_shared<future<T>>()) {}
    
    promise(const promise&) = delete;
    promise& operator=(const promise&) = delete;
    
    promise(promise&&) = default;
    promise& operator=(promise&&) = default;
    
    future<T> get_future() {
        return *_future;  // Copy constructor
    }
    
    void set_value(const T& value) {
        _future->set_value(value);
    }
    
    void set_value(T&& value) {
        _future->set_value(std::move(value));
    }
    
    void set_exception(std::exception_ptr ex) {
        _future->set_exception(ex);
    }
};

template <>
class promise<void> {
private:
    std::shared_ptr<future<void>> _future;

public:
    promise() : _future(std::make_shared<future<void>>()) {}
    
    promise(const promise&) = delete;
    promise& operator=(const promise&) = delete;
    
    promise(promise&&) = default;
    promise& operator=(promise&&) = default;
    
    future<void> get_future() {
        return *_future;  // Copy constructor
    }
    
    void set_value() {
        _future->set_value();
    }
    
    void set_exception(std::exception_ptr ex) {
        _future->set_exception(ex);
    }
};

/// Helper functions
inline future<void> make_ready_future() {
    future<void> f;
    f.set_value();
    return f;
}

template <typename T>
future<T> make_ready_future(T&& value) {
    future<T> f;
    f.set_value(std::forward<T>(value));
    return f;
}

template <typename T = void>
future<T> make_exception_future(std::exception_ptr ex) {
    future<T> f;
    f.set_exception(ex);
    return f;
}

// Note: Use future<void> and promise<void> instead of future<> and promise<>

} // namespace seastar 