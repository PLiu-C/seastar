#pragma once

#include <seastar/net/multicast_udp_channel.hh>
#include <seastar/net/posix-stack.hh> // For pollable_fd forward declaration if needed, or include fully

namespace seastar {

class pollable_fd;

namespace net {

class datagram_channel;

class posix_multicast_udp_channel_impl : public multicast_udp_channel::impl {
public:
    future<> join(datagram_channel& chan, const std::string& interface_name, const socket_address& mcast_addr) override;
    future<> leave(datagram_channel& chan, const std::string& interface_name, const socket_address& mcast_addr) override;
private:
    pollable_fd& get_fd(datagram_channel& chan);
};

} // namespace net

} // namespace seastar 