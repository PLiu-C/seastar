#pragma once

#include <memory>
#include <cstddef>

namespace seastar {

template <typename CharType>
class temporary_buffer {
private:
    std::unique_ptr<CharType[]> _buffer;
    size_t _size;

public:
    temporary_buffer() : _size(0) {}
    
    explicit temporary_buffer(size_t size) 
        : _buffer(std::make_unique<CharType[]>(size)), _size(size) {}

    temporary_buffer(const temporary_buffer&) = delete;
    temporary_buffer& operator=(const temporary_buffer&) = delete;
    
    temporary_buffer(temporary_buffer&&) = default;
    temporary_buffer& operator=(temporary_buffer&&) = default;

    CharType* get_write() { return _buffer.get(); }
    const CharType* get() const { return _buffer.get(); }
    
    size_t size() const { return _size; }
    bool empty() const { return _size == 0; }

    void trim(size_t new_size) {
        if (new_size <= _size) {
            _size = new_size;
        }
    }
};

} // namespace seastar 