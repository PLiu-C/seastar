#include <seastar/net/socket_defs.hh>
#include <seastar/net/inet_address.hh>
#include <cstring>
#include <sstream>
#include <arpa/inet.h>
#include <algorithm>

namespace seastar {

socket_address::socket_address() noexcept : addr_length{sizeof(::sockaddr_in)} {
    std::memset(&u, 0, sizeof(u));
    u.sa.sa_family = AF_INET;
}

socket_address::socket_address(uint16_t port) noexcept : addr_length{sizeof(::sockaddr_in)} {
    std::memset(&u, 0, sizeof(u));
    u.in.sin_family = AF_INET;
    u.in.sin_port = htons(port);
    u.in.sin_addr.s_addr = INADDR_ANY;
}

socket_address::socket_address(uint32_t ip, uint16_t port) noexcept : addr_length{sizeof(::sockaddr_in)} {
    std::memset(&u, 0, sizeof(u));
    u.in.sin_family = AF_INET;
    u.in.sin_port = htons(port);
    u.in.sin_addr.s_addr = htonl(ip);
}

socket_address::socket_address(ipv4_addr addr) noexcept : addr_length{sizeof(::sockaddr_in)} {
    std::memset(&u, 0, sizeof(u));
    u.in.sin_family = AF_INET;
    u.in.sin_port = htons(addr.port);
    u.in.sin_addr.s_addr = htonl(addr.ip);
}

socket_address::socket_address(const ipv6_addr& addr) noexcept : addr_length{sizeof(::sockaddr_in6)} {
    std::memset(&u, 0, sizeof(u));
    u.in6.sin6_family = AF_INET6;
    u.in6.sin6_port = htons(addr.port);
    std::memcpy(&u.in6.sin6_addr, addr.ip.data(), 16);
}

socket_address::socket_address(const net::inet_address& addr, uint16_t port) noexcept {
    std::memset(&u, 0, sizeof(u));
    if (addr.in_family() == net::inet_address::family::INET) {
        addr_length = sizeof(::sockaddr_in);
        u.in.sin_family = AF_INET;
        u.in.sin_port = htons(port);
        u.in.sin_addr = addr.as_ipv4_address();
    } else {
        addr_length = sizeof(::sockaddr_in6);
        u.in6.sin6_family = AF_INET6;
        u.in6.sin6_port = htons(port);
        u.in6.sin6_addr = addr.as_ipv6_address();
        u.in6.sin6_scope_id = addr.scope();
    }
}

bool socket_address::is_unspecified() const noexcept {
    switch (u.sa.sa_family) {
    case AF_INET:
        return u.in.sin_addr.s_addr == INADDR_ANY && u.in.sin_port == 0;
    case AF_INET6:
        return IN6_IS_ADDR_UNSPECIFIED(&u.in6.sin6_addr) && u.in6.sin6_port == 0;
    default:
        return true;
    }
}

net::inet_address socket_address::addr() const noexcept {
    switch (u.sa.sa_family) {
    case AF_INET:
        return net::inet_address(u.in.sin_addr);
    case AF_INET6:
        return net::inet_address(u.in6.sin6_addr, u.in6.sin6_scope_id);
    default:
        return net::inet_address();
    }
}

::in_port_t socket_address::port() const noexcept {
    switch (u.sa.sa_family) {
    case AF_INET:
        return ntohs(u.in.sin_port);
    case AF_INET6:
        return ntohs(u.in6.sin6_port);
    default:
        return 0;
    }
}

bool socket_address::is_wildcard() const noexcept {
    switch (u.sa.sa_family) {
    case AF_INET:
        return u.in.sin_addr.s_addr == INADDR_ANY;
    case AF_INET6:
        return IN6_IS_ADDR_UNSPECIFIED(&u.in6.sin6_addr);
    default:
        return false;
    }
}

bool socket_address::operator==(const socket_address& other) const noexcept {
    if (u.sa.sa_family != other.u.sa.sa_family) {
        return false;
    }
    switch (u.sa.sa_family) {
    case AF_INET:
        return u.in.sin_addr.s_addr == other.u.in.sin_addr.s_addr &&
               u.in.sin_port == other.u.in.sin_port;
    case AF_INET6:
        return std::memcmp(&u.in6.sin6_addr, &other.u.in6.sin6_addr, 16) == 0 &&
               u.in6.sin6_port == other.u.in6.sin6_port &&
               u.in6.sin6_scope_id == other.u.in6.sin6_scope_id;
    default:
        return false;
    }
}

std::ostream& operator<<(std::ostream& os, const socket_address& sa) {
    switch (sa.u.sa.sa_family) {
    case AF_INET: {
        char buf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sa.u.in.sin_addr, buf, sizeof(buf));
        os << buf << ":" << ntohs(sa.u.in.sin_port);
        break;
    }
    case AF_INET6: {
        char buf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &sa.u.in6.sin6_addr, buf, sizeof(buf));
        os << "[" << buf << "]:" << ntohs(sa.u.in6.sin6_port);
        if (sa.u.in6.sin6_scope_id) {
            os << "%" << sa.u.in6.sin6_scope_id;
        }
        break;
    }
    default:
        os << "unknown_family(" << sa.u.sa.sa_family << ")";
    }
    return os;
}

// ipv4_addr implementations
ipv4_addr::ipv4_addr(const std::string &addr) {
    auto pos = addr.find(':');
    if (pos != std::string::npos) {
        auto ip_str = addr.substr(0, pos);
        auto port_str = addr.substr(pos + 1);
        
        struct in_addr in;
        if (inet_pton(AF_INET, ip_str.c_str(), &in) != 1) {
            throw std::invalid_argument("Invalid IPv4 address: " + ip_str);
        }
        ip = ntohl(in.s_addr);
        port = std::stoi(port_str);
    } else {
        struct in_addr in;
        if (inet_pton(AF_INET, addr.c_str(), &in) != 1) {
            throw std::invalid_argument("Invalid IPv4 address: " + addr);
        }
        ip = ntohl(in.s_addr);
        port = 0;
    }
}

ipv4_addr::ipv4_addr(const std::string &addr, uint16_t port) {
    struct in_addr in;
    if (inet_pton(AF_INET, addr.c_str(), &in) != 1) {
        throw std::invalid_argument("Invalid IPv4 address: " + addr);
    }
    this->ip = ntohl(in.s_addr);
    this->port = port;
}

ipv4_addr::ipv4_addr(const net::inet_address& addr, uint16_t port) {
    if (!addr.is_ipv4()) {
        throw std::invalid_argument("inet_address is not IPv4");
    }
    this->ip = ntohl(addr.as_ipv4_address().s_addr);
    this->port = port;
}

ipv4_addr::ipv4_addr(const socket_address& sa) noexcept {
    if (sa.u.sa.sa_family == AF_INET) {
        ip = ntohl(sa.u.in.sin_addr.s_addr);
        port = ntohs(sa.u.in.sin_port);
    } else {
        ip = 0;
        port = 0;
    }
}

std::ostream& operator<<(std::ostream& os, const ipv4_addr& addr) {
    struct in_addr in;
    in.s_addr = htonl(addr.ip);
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &in, buf, sizeof(buf));
    if (addr.port) {
        os << buf << ":" << addr.port;
    } else {
        os << buf;
    }
    return os;
}

// ipv6_addr implementations
ipv6_addr::ipv6_addr(const ipv6_bytes& bytes, uint16_t port) noexcept 
    : ip(bytes), port(port) {}

ipv6_addr::ipv6_addr(uint16_t port) noexcept : port(port) {
    ip.fill(0);
}

ipv6_addr::ipv6_addr(const std::string& addr) {
    auto pos = addr.rfind("]:");
    if (pos != std::string::npos) {
        // [::1]:8080 format
        auto ip_str = addr.substr(1, pos - 1); // Remove [ and ]
        auto port_str = addr.substr(pos + 2);
        
        struct in6_addr in6;
        if (inet_pton(AF_INET6, ip_str.c_str(), &in6) != 1) {
            throw std::invalid_argument("Invalid IPv6 address: " + ip_str);
        }
        std::memcpy(ip.data(), &in6, 16);
        port = std::stoi(port_str);
    } else {
        struct in6_addr in6;
        if (inet_pton(AF_INET6, addr.c_str(), &in6) != 1) {
            throw std::invalid_argument("Invalid IPv6 address: " + addr);
        }
        std::memcpy(ip.data(), &in6, 16);
        port = 0;
    }
}

ipv6_addr::ipv6_addr(const std::string& addr, uint16_t port) {
    struct in6_addr in6;
    if (inet_pton(AF_INET6, addr.c_str(), &in6) != 1) {
        throw std::invalid_argument("Invalid IPv6 address: " + addr);
    }
    std::memcpy(ip.data(), &in6, 16);
    this->port = port;
}

ipv6_addr::ipv6_addr(const net::inet_address& addr, uint16_t port) noexcept {
    if (addr.is_ipv6()) {
        auto in6 = addr.as_ipv6_address();
        std::memcpy(ip.data(), &in6, 16);
        this->port = port;
    } else {
        ip.fill(0);
        this->port = port;
    }
}

ipv6_addr::ipv6_addr(const ::in6_addr& in6, uint16_t port) noexcept : port(port) {
    std::memcpy(ip.data(), &in6, 16);
}

ipv6_addr::ipv6_addr(const ::sockaddr_in6& sa) noexcept {
    std::memcpy(ip.data(), &sa.sin6_addr, 16);
    port = ntohs(sa.sin6_port);
}

ipv6_addr::ipv6_addr(const socket_address& sa) noexcept {
    if (sa.u.sa.sa_family == AF_INET6) {
        std::memcpy(ip.data(), &sa.u.in6.sin6_addr, 16);
        port = ntohs(sa.u.in6.sin6_port);
    } else {
        ip.fill(0);
        port = 0;
    }
}

bool ipv6_addr::is_ip_unspecified() const noexcept {
    return std::all_of(ip.begin(), ip.end(), [](uint8_t b) { return b == 0; });
}

std::ostream& operator<<(std::ostream& os, const ipv6_addr& addr) {
    struct in6_addr in6;
    std::memcpy(&in6, addr.ip.data(), 16);
    char buf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &in6, buf, sizeof(buf));
    if (addr.port) {
        os << "[" << buf << "]:" << addr.port;
    } else {
        os << buf;
    }
    return os;
}

} 