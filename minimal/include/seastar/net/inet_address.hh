#pragma once

#include <iosfwd>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdexcept>
#include <vector>
#include <seastar/core/future.hh>
#include <seastar/core/sstring.hh>

namespace seastar {
namespace net {

struct ipv4_address;
struct ipv6_address;

class unknown_host : public std::invalid_argument {
public:
    using invalid_argument::invalid_argument;
};

class inet_address {
public:
    enum class family : sa_family_t {
        INET = AF_INET, INET6 = AF_INET6
    };
private:
    family _in_family;
    union {
        ::in_addr _in;
        ::in6_addr _in6;
    };
    uint32_t _scope = 0;
    static constexpr uint32_t invalid_scope = 0;

public:
    inet_address() noexcept;
    inet_address(family f) noexcept;
    inet_address(::in_addr i) noexcept;
    inet_address(::in6_addr i, uint32_t scope = invalid_scope) noexcept;
    inet_address(const ipv4_address&) noexcept;
    inet_address(const ipv6_address&, uint32_t scope = invalid_scope) noexcept;
    
    // throws if not a valid ip address
    inet_address(const sstring&);
    inet_address(const char*);

    family in_family() const noexcept {
        return _in_family;
    }

    bool is_ipv6() const noexcept {
        return _in_family == family::INET6;
    }

    bool is_ipv4() const noexcept {
        return _in_family == family::INET;
    }

    const ::in_addr& as_ipv4_address() const noexcept {
        return _in;
    }

    const ::in6_addr& as_ipv6_address() const noexcept {
        return _in6;
    }

    uint32_t scope() const noexcept {
        return _scope;
    }

    size_t size() const noexcept;
    const void* data() const noexcept;

    bool operator==(const inet_address&) const noexcept;
    bool operator!=(const inet_address& a) const noexcept {
        return !(*this == a);
    }

    // throws if this is not an ipv4 address
    ipv4_address as_ipv4_address();
    // throws if this is not an ipv6 address  
    ipv6_address as_ipv6_address();
};

std::ostream& operator<<(std::ostream&, const inet_address&);
std::ostream& operator<<(std::ostream&, inet_address::family);

struct ipv4_address {
    uint32_t ip;

    ipv4_address() noexcept : ip(0) {}
    explicit ipv4_address(uint32_t ip) noexcept : ip(ip) {}
    ipv4_address(const ::in_addr& in) noexcept : ip(in.s_addr) {}
    
    operator uint32_t() const noexcept { return ip; }
    operator ::in_addr() const noexcept { return {ip}; }
    
    bool operator==(const ipv4_address& o) const noexcept {
        return ip == o.ip;
    }
    bool operator!=(const ipv4_address& o) const noexcept {
        return ip != o.ip;
    }
};

struct ipv6_address {
    using bytes = std::array<uint8_t, 16>;
    bytes ip;

    ipv6_address() noexcept;
    ipv6_address(const bytes&) noexcept;
    ipv6_address(const ::in6_addr&) noexcept;
    
    operator ::in6_addr() const noexcept;
    
    bool operator==(const ipv6_address& o) const noexcept {
        return ip == o.ip;
    }
    bool operator!=(const ipv6_address& o) const noexcept {
        return ip != o.ip;
    }
};

std::ostream& operator<<(std::ostream&, const ipv4_address&);
std::ostream& operator<<(std::ostream&, const ipv6_address&);

// DNS resolution functions (simplified versions)
future<inet_address> inet_address_type_resolve(sstring name);
future<std::vector<inet_address>> inet_address_type_resolve_all(sstring name);

}
} 