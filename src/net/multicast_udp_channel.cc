#include <seastar/net/multicast_udp_channel.hh>
#include <seastar/core/reactor.hh>

#ifdef SEASTAR_HAVE_DPDK
#include <seastar/net/native-stack.hh>
#include <seastar/net/ip.hh>
#else
#include <seastar/net/posix-stack.hh>
#include <seastar/core/posix.hh>
#include <net/if.h>
#endif

namespace seastar {

class multicast_udp_channel::impl {
public:
    virtual ~impl() = default;
    virtual future<> join(const std::string& interface_name, const socket_address& mcast_addr) = 0;
    virtual future<> leave(const std::string& interface_name, const socket_address& mcast_addr) = 0;
};

#ifdef SEASTAR_HAVE_DPDK
// Native stack implementation
class native_multicast_impl : public multicast_udp_channel::impl {
private:
    net::datagram_channel& _chan;
    
public:
    explicit native_multicast_impl(net::datagram_channel& chan) : _chan(chan) {}
    
    future<> join(const std::string& interface_name, const socket_address& mcast_addr) override {
        // With native stack, we delegate to the datagram_channel to handle membership
        return _chan.add_membership(mcast_addr, interface_name);
    }
    
    future<> leave(const std::string& interface_name, const socket_address& mcast_addr) override {
        return _chan.drop_membership(mcast_addr, interface_name);
    }
};
#else
// POSIX stack implementation
class posix_multicast_impl : public multicast_udp_channel::impl {
private:
    file_desc _fd;
    
public:
    explicit posix_multicast_impl(file_desc fd) : _fd(std::move(fd)) {}
    
    future<> join(const std::string& interface_name, const socket_address& mcast_addr) override {
        struct ip_mreqn mr;
        memset(&mr, 0, sizeof(mr));
        
        mr.imr_multiaddr = mcast_addr.as_posix_sockaddr_in().sin_addr;
        mr.imr_ifindex = if_nametoindex(interface_name.c_str());
        
        if (mr.imr_ifindex == 0) {
            return make_exception_future<>(std::system_error(errno, std::system_category(), 
                "Could not find interface index for " + interface_name));
        }
        
        if (setsockopt(_fd.get(), IPPROTO_IP, IP_ADD_MEMBERSHIP, &mr, sizeof(mr)) < 0) {
            return make_exception_future<>(std::system_error(errno, std::system_category(), 
                "Failed to join multicast group"));
        }
        
        return make_ready_future<>();
    }
    
    future<> leave(const std::string& interface_name, const socket_address& mcast_addr) override {
        struct ip_mreqn mr;
        memset(&mr, 0, sizeof(mr));
        
        mr.imr_multiaddr = mcast_addr.as_posix_sockaddr_in().sin_addr;
        mr.imr_ifindex = if_nametoindex(interface_name.c_str());
        
        if (mr.imr_ifindex == 0) {
            return make_exception_future<>(std::system_error(errno, std::system_category(), 
                "Could not find interface index for " + interface_name));
        }
        
        if (setsockopt(_fd.get(), IPPROTO_IP, IP_DROP_MEMBERSHIP, &mr, sizeof(mr)) < 0) {
            return make_exception_future<>(std::system_error(errno, std::system_category(), 
                "Failed to leave multicast group"));
        }
        
        return make_ready_future<>();
    }
};
#endif

multicast_udp_channel::multicast_udp_channel(net::datagram_channel chan) 
    : _chan(std::move(chan)) {
#ifdef SEASTAR_HAVE_DPDK
    // Native stack implementation
    _impl = std::make_unique<native_multicast_impl>(_chan);
#else
    // POSIX stack implementation
    // We need to extract the file descriptor from the datagram channel
    // This assumes the channel is implemented using a posix_data_sink_impl with a file_desc
    auto* posix_impl = dynamic_cast<net::posix_data_source_impl*>(_chan.source().as_member().get_impl().get());
    if (!posix_impl) {
        throw std::runtime_error("Cannot create multicast channel: not a POSIX implementation");
    }
    
    // This is a bit of a hack to get the file descriptor from the implementation
    // Ideally, the Seastar API would provide cleaner access
    file_desc fd = posix_impl->get_file_desc();
    _impl = std::make_unique<posix_multicast_impl>(std::move(fd));
#endif
}

multicast_udp_channel::~multicast_udp_channel() = default;

multicast_udp_channel::multicast_udp_channel(multicast_udp_channel&&) noexcept = default;
multicast_udp_channel& multicast_udp_channel::operator=(multicast_udp_channel&&) noexcept = default;

future<> multicast_udp_channel::join(const std::string& interface_name, const socket_address& mcast_addr) {
    return _impl->join(interface_name, mcast_addr);
}

future<> multicast_udp_channel::leave(const std::string& interface_name, const socket_address& mcast_addr) {
    return _impl->leave(interface_name, mcast_addr);
}

future<multicast_udp_channel> make_multicast_udp_channel(const socket_address& local_addr) {
    return make_bound_datagram_channel(local_addr).then([](net::datagram_channel chan) {
        return make_ready_future<multicast_udp_channel>(multicast_udp_channel(std::move(chan)));
    });
}

} // namespace seastar 