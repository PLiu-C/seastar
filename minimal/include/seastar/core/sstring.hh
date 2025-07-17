#pragma once

#include <string>
#include <iosfwd>
#include <functional>
#include <string_view>

namespace seastar {

using sstring = std::string;

}  // namespace seastar

namespace std {
template <>
struct hash<seastar::sstring> {
    size_t operator()(const seastar::sstring& s) const {
        return std::hash<std::string>{}(s);
    }
};
} 