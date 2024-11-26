// include/seastar/net/multicast_udp_channel.hh
#pragma once

#include <seastar/core/future.hh>
#include <seastar/net/api.hh>
#include <seastar/net/socket_defs.hh>
#include <string>

namespace seastar {

class multicast_udp_channel {
private:
    net::datagram_channel _chan;
    class impl;
    std::unique_ptr<impl> _impl;

public:
    explicit multicast_udp_channel(net::datagram_channel chan);
    ~multicast_udp_channel();
    
    // Move-only
    multicast_udp_channel(multicast_udp_channel&&) noexcept;
    multicast_udp_channel& operator=(multicast_udp_channel&&) noexcept;

    future<> join(const std::string& interface_name, const socket_address& mcast_addr);

    future<> leave(const std::string& interface_name, const socket_address& mcast_addr);

    future<net::datagram> receive() {
        return _chan.receive();
    }

    socket_address local_address() const {
        return _chan.local_address();
    }

    future<> close() {
        _chan.close();
        return make_ready_future<>();
    }
};

} // namespace seastar