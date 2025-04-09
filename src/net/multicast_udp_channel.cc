#include <seastar/net/multicast_udp_channel.hh>
#include <seastar/core/reactor.hh> // For engine()
#include <seastar/net/api.hh>      // For network_stack, datagram_channel
#include <seastar/util/log.hh>     // For logger
#include <stdexcept>               // For runtime_error
#include <typeinfo>                // For dynamic_cast (optional, but good practice to include)

/* TBR
// Include POSIX stack unconditionally
#include <seastar/net/posix-stack.hh>
#include "posix-stack-multicast-impl.hh"

// Conditionally include native stack headers only if DPDK is enabled
#ifdef SEASTAR_HAVE_DPDK
#include <seastar/net/native-stack.hh>
#include "native-stack-multicast-impl.hh"
#endif
*/

namespace seastar::net {

// Use the standard logger type
seastar::logger nmc_log("multicast_channel"); // Changed net_logger to logger

// --- Private Constructor Implementation ---
multicast_udp_channel::multicast_udp_channel(datagram_channel chan, std::unique_ptr<impl> impl)
    : _chan(std::move(chan))
    , _impl(std::move(impl)) 
{}

// Destructor, move constructor/assignment
multicast_udp_channel::~multicast_udp_channel() = default; // ~impl() is virtual
multicast_udp_channel::multicast_udp_channel(multicast_udp_channel&&) noexcept = default;
multicast_udp_channel& multicast_udp_channel::operator=(multicast_udp_channel&&) noexcept = default;

// Forwarding methods
future<> multicast_udp_channel::join(const std::string& interface_name, const socket_address& mcast_addr) {
    return _impl->join(_chan, interface_name, mcast_addr);
}

future<> multicast_udp_channel::leave(const std::string& interface_name, const socket_address& mcast_addr) {
    return _impl->leave(_chan, interface_name, mcast_addr);
}

} // namespace seastar::net