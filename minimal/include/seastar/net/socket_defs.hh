#pragma once

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <functional>
#include <iostream>
#include <array>
#include <cstring>
#include <arpa/inet.h>

namespace seastar {

namespace net {
class inet_address;
}

struct ipv4_addr;
struct ipv6_addr;

class socket_address {
public:
    socklen_t addr_length; // actual size of the relevant 'u' member
    union {
        ::sockaddr_storage sas;
        ::sockaddr sa;
        ::sockaddr_in in;
        ::sockaddr_in6 in6;
    } u;

    socket_address(const sockaddr_in& sa) noexcept : addr_length{sizeof(::sockaddr_in)} {
        u.in = sa;
    }
    socket_address(const sockaddr_in6& sa) noexcept : addr_length{sizeof(::sockaddr_in6)} {
        u.in6 = sa;
    }
    socket_address(uint16_t port = 0) noexcept;
    socket_address(ipv4_addr) noexcept;
    socket_address(const ipv6_addr&) noexcept;
    socket_address(const net::inet_address&, uint16_t p = 0) noexcept;

    /** creates an uninitialized socket_address. this can be written into, or used as
     *  "unspecified" for such addresses as bind(addr) or local address in socket::connect
     *  (i.e. system picks)
     */
    socket_address() noexcept;

    ::sockaddr& as_posix_sockaddr() noexcept { return u.sa; }
    ::sockaddr_in& as_posix_sockaddr_in() noexcept { return u.in; }
    ::sockaddr_in6& as_posix_sockaddr_in6() noexcept { return u.in6; }
    const ::sockaddr& as_posix_sockaddr() const noexcept { return u.sa; }
    const ::sockaddr_in& as_posix_sockaddr_in() const noexcept { return u.in; }
    const ::sockaddr_in6& as_posix_sockaddr_in6() const noexcept { return u.in6; }

    socket_address(uint32_t, uint16_t p = 0) noexcept;

    socklen_t length() const noexcept { return addr_length; }

    bool is_unspecified() const noexcept;

    sa_family_t family() const noexcept {
        return u.sa.sa_family;
    }

    net::inet_address addr() const noexcept;
    ::in_port_t port() const noexcept;
    bool is_wildcard() const noexcept;

    bool operator==(const socket_address&) const noexcept;
    bool operator!=(const socket_address& a) const noexcept {
        return !(*this == a);
    }
};

std::ostream& operator<<(std::ostream&, const socket_address&);

enum class transport {
    TCP = IPPROTO_TCP,
    SCTP = IPPROTO_SCTP
};

struct ipv4_addr {
    uint32_t ip;
    uint16_t port;

    ipv4_addr() noexcept : ip(0), port(0) {}
    ipv4_addr(uint32_t ip, uint16_t port) noexcept : ip(ip), port(port) {}
    ipv4_addr(uint16_t port) noexcept : ip(0), port(port) {}
    ipv4_addr(const std::string &addr);
    ipv4_addr(const std::string &addr, uint16_t port);
    ipv4_addr(const net::inet_address&, uint16_t);
    ipv4_addr(const socket_address &) noexcept;

    bool is_ip_unspecified() const noexcept {
        return ip == 0;
    }
    bool is_port_unspecified() const noexcept {
        return port == 0;
    }
};

struct ipv6_addr {
    using ipv6_bytes = std::array<uint8_t, 16>;

    ipv6_bytes ip;
    uint16_t port;

    ipv6_addr(const ipv6_bytes&, uint16_t port = 0) noexcept;
    ipv6_addr(uint16_t port = 0) noexcept;
    ipv6_addr(const std::string&);
    ipv6_addr(const std::string&, uint16_t port);
    ipv6_addr(const net::inet_address&, uint16_t = 0) noexcept;
    ipv6_addr(const ::in6_addr&, uint16_t = 0) noexcept;
    ipv6_addr(const ::sockaddr_in6&) noexcept;
    ipv6_addr(const socket_address&) noexcept;

    bool is_ip_unspecified() const noexcept;
    bool is_port_unspecified() const noexcept {
        return port == 0;
    }
};

std::ostream& operator<<(std::ostream&, const ipv4_addr&);
std::ostream& operator<<(std::ostream&, const ipv6_addr&);

inline bool operator==(const ipv4_addr &lhs, const ipv4_addr& rhs) noexcept {
    return lhs.ip == rhs.ip && lhs.port == rhs.port;
}

}

namespace std {
template<>
struct hash<seastar::socket_address> {
    size_t operator()(const seastar::socket_address& a) const {
        switch (a.u.sa.sa_family) {
        case AF_INET:
            return std::hash<uint32_t>()(a.u.in.sin_addr.s_addr) ^
                   std::hash<uint16_t>()(a.u.in.sin_port);
        case AF_INET6: {
            auto& a6 = a.u.in6.sin6_addr.s6_addr;
            uint64_t h1, h2;
            std::memcpy(&h1, a6, 8);
            std::memcpy(&h2, a6 + 8, 8);
            return std::hash<uint64_t>()(h1) ^
                   std::hash<uint64_t>()(h2) ^
                   std::hash<uint16_t>()(a.u.in6.sin6_port);
        }
        default:
            return 0;
        }
    }
};

template<>
struct hash<seastar::ipv4_addr> {
    size_t operator()(const seastar::ipv4_addr& a) const {
        return std::hash<uint32_t>()(a.ip) ^ std::hash<uint16_t>()(a.port);
    }
};
} 