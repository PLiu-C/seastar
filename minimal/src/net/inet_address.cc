#include <seastar/net/inet_address.hh>
#include <cstring>
#include <arpa/inet.h>
#include <iostream>

namespace seastar {
namespace net {

// inet_address implementation
inet_address::inet_address() noexcept : _in_family(family::INET), _scope(invalid_scope) {
    _in.s_addr = INADDR_ANY;
}

inet_address::inet_address(family f) noexcept : _in_family(f), _scope(invalid_scope) {
    if (f == family::INET) {
        _in.s_addr = INADDR_ANY;
    } else {
        std::memset(&_in6, 0, sizeof(_in6));
    }
}

inet_address::inet_address(::in_addr i) noexcept : _in_family(family::INET), _in(i), _scope(invalid_scope) {
}

inet_address::inet_address(::in6_addr i, uint32_t scope) noexcept 
    : _in_family(family::INET6), _in6(i), _scope(scope) {
}

inet_address::inet_address(const ipv4_address& addr) noexcept 
    : _in_family(family::INET), _scope(invalid_scope) {
    _in.s_addr = htonl(addr.ip);
}

inet_address::inet_address(const ipv6_address& addr, uint32_t scope) noexcept 
    : _in_family(family::INET6), _scope(scope) {
    std::memcpy(&_in6, addr.ip.data(), 16);
}

inet_address::inet_address(const sstring& addr) {
    // Try IPv4 first
    ::in_addr in4;
    if (inet_pton(AF_INET, addr.c_str(), &in4) == 1) {
        _in_family = family::INET;
        _in = in4;
        _scope = invalid_scope;
        return;
    }
    
    // Try IPv6
    ::in6_addr in6;
    if (inet_pton(AF_INET6, addr.c_str(), &in6) == 1) {
        _in_family = family::INET6;
        _in6 = in6;
        _scope = invalid_scope;
        return;
    }
    
    throw unknown_host("Invalid IP address: " + addr);
}

inet_address::inet_address(const char* addr) : inet_address(sstring(addr)) {
}

size_t inet_address::size() const noexcept {
    return _in_family == family::INET ? 4 : 16;
}

const void* inet_address::data() const noexcept {
    return _in_family == family::INET ? 
        static_cast<const void*>(&_in) : 
        static_cast<const void*>(&_in6);
}

bool inet_address::operator==(const inet_address& other) const noexcept {
    if (_in_family != other._in_family) {
        return false;
    }
    if (_in_family == family::INET) {
        return _in.s_addr == other._in.s_addr;
    } else {
        return std::memcmp(&_in6, &other._in6, 16) == 0 && _scope == other._scope;
    }
}

ipv4_address inet_address::as_ipv4_address() {
    if (_in_family != family::INET) {
        throw std::invalid_argument("inet_address is not IPv4");
    }
    return ipv4_address(ntohl(_in.s_addr));
}

ipv6_address inet_address::as_ipv6_address() {
    if (_in_family != family::INET6) {
        throw std::invalid_argument("inet_address is not IPv6");
    }
    ipv6_address::bytes bytes;
    std::memcpy(bytes.data(), &_in6, 16);
    return ipv6_address(bytes);
}

std::ostream& operator<<(std::ostream& os, const inet_address& addr) {
    if (addr.is_ipv4()) {
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, addr.data(), buf, sizeof(buf));
        os << buf;
    } else {
        char buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, addr.data(), buf, sizeof(buf));
        os << buf;
        if (addr.scope() != 0) {
            os << "%" << addr.scope();
        }
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, inet_address::family f) {
    switch (f) {
    case inet_address::family::INET:
        os << "INET";
        break;
    case inet_address::family::INET6:
        os << "INET6";
        break;
    }
    return os;
}

// ipv4_address implementation
std::ostream& operator<<(std::ostream& os, const ipv4_address& addr) {
    struct in_addr in;
    in.s_addr = htonl(addr.ip);
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &in, buf, sizeof(buf));
    os << buf;
    return os;
}

// ipv6_address implementation
ipv6_address::ipv6_address() noexcept {
    ip.fill(0);
}

ipv6_address::ipv6_address(const bytes& bytes) noexcept : ip(bytes) {
}

ipv6_address::ipv6_address(const ::in6_addr& in6) noexcept {
    std::memcpy(ip.data(), &in6, 16);
}

ipv6_address::operator ::in6_addr() const noexcept {
    ::in6_addr result;
    std::memcpy(&result, ip.data(), 16);
    return result;
}

std::ostream& operator<<(std::ostream& os, const ipv6_address& addr) {
    ::in6_addr in6;
    std::memcpy(&in6, addr.ip.data(), 16);
    char buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &in6, buf, sizeof(buf));
    os << buf;
    return os;
}

// DNS resolution functions (simplified placeholders)
future<inet_address> inet_address_type_resolve(sstring name) {
    try {
        return make_ready_future<inet_address>(inet_address(name));
    } catch (...) {
        return make_exception_future<inet_address>(std::current_exception());
    }
}

future<std::vector<inet_address>> inet_address_type_resolve_all(sstring name) {
    try {
        std::vector<inet_address> result;
        result.push_back(inet_address(name));
        return make_ready_future<std::vector<inet_address>>(std::move(result));
    } catch (...) {
        return make_exception_future<std::vector<inet_address>>(std::current_exception());
    }
}

}
} 