#pragma once

#include <memory>
#include <cstddef>

namespace seastar {

template <typename T>
class circular_buffer {
private:
    std::unique_ptr<T[]> _buffer;
    size_t _capacity = 0;
    size_t _size = 0;
    size_t _begin = 0;

public:
    circular_buffer() = default;
    
    explicit circular_buffer(size_t capacity) 
        : _buffer(std::make_unique<T[]>(capacity)), _capacity(capacity) {}

    circular_buffer(const circular_buffer&) = delete;
    circular_buffer& operator=(const circular_buffer&) = delete;
    
    circular_buffer(circular_buffer&&) = default;
    circular_buffer& operator=(circular_buffer&&) = default;

    void push_back(T&& item) {
        if (_size == _capacity) {
            throw std::runtime_error("circular_buffer overflow");
        }
        size_t pos = (_begin + _size) % _capacity;
        _buffer[pos] = std::move(item);
        ++_size;
    }
    
    void push_back(const T& item) {
        if (_size == _capacity) {
            throw std::runtime_error("circular_buffer overflow");
        }
        size_t pos = (_begin + _size) % _capacity;
        _buffer[pos] = item;
        ++_size;
    }

    T& front() {
        return _buffer[_begin];
    }
    
    const T& front() const {
        return _buffer[_begin];
    }

    void pop_front() {
        if (_size == 0) {
            throw std::runtime_error("circular_buffer underflow");
        }
        _begin = (_begin + 1) % _capacity;
        --_size;
    }

    bool empty() const { return _size == 0; }
    size_t size() const { return _size; }
    size_t capacity() const { return _capacity; }
};

template <typename T, size_t N>
class circular_buffer_fixed_capacity {
private:
    T _buffer[N];
    size_t _size = 0;
    size_t _begin = 0;

public:
    void push_back(T&& item) {
        if (_size == N) {
            throw std::runtime_error("circular_buffer_fixed_capacity overflow");
        }
        size_t pos = (_begin + _size) % N;
        _buffer[pos] = std::move(item);
        ++_size;
    }
    
    void push_back(const T& item) {
        if (_size == N) {
            throw std::runtime_error("circular_buffer_fixed_capacity overflow");
        }
        size_t pos = (_begin + _size) % N;
        _buffer[pos] = item;
        ++_size;
    }

    T& front() {
        return _buffer[_begin];
    }
    
    const T& front() const {
        return _buffer[_begin];
    }

    void pop_front() {
        if (_size == 0) {
            throw std::runtime_error("circular_buffer_fixed_capacity underflow");
        }
        _begin = (_begin + 1) % N;
        --_size;
    }

    bool empty() const { return _size == 0; }
    size_t size() const { return _size; }
    static constexpr size_t capacity() { return N; }
};

} // namespace seastar 