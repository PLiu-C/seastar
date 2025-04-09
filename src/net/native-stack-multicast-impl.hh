#pragma once

#include <seastar/net/multicast_udp_channel.hh>

// DPDK includes (ensure these are available in the build environment)
// Used for DPDK-specific multicast operations in native-stack.cc
extern "C" {
#include <rte_config.h>
#include <rte_ethdev.h>
#include <rte_ether.h> // For rte_ether_addr
#include <rte_errno.h> // For rte_errno and rte_strerror
}

namespace seastar::net {

class datagram_channel;

class native_multicast_udp_channel_impl : public multicast_udp_channel::impl {
public:
    future<> join(net::datagram_channel& chan, const std::string& interface_name, const socket_address& mcast_addr) override;
    future<> leave(datagram_channel& chan, const std::string& interface_name, const socket_address& mcast_addr) override;
};

} // namespace seastar::net