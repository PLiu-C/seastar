// include/seastar/net/multicast_udp_channel.hh
#pragma once

#include <seastar/core/future.hh>
#include <seastar/net/api.hh>
#include <seastar/net/socket_defs.hh>
#include <string>
#include <memory> // Include for unique_ptr

namespace seastar::net {

// Forward declarations for friend classes (adjust namespaces if needed)
class posix_network_stack;
class native_network_stack;

class multicast_udp_channel {
public:
    ~multicast_udp_channel();

    // Move-only
    multicast_udp_channel(multicast_udp_channel&&) noexcept;
    multicast_udp_channel& operator=(multicast_udp_channel&&) noexcept;

    future<> join(const std::string& interface_name, const socket_address& mcast_addr);
    future<> leave(const std::string& interface_name, const socket_address& mcast_addr);

    future<datagram> receive() {
        return _chan.receive();
    }

    socket_address local_address() const {
        return _chan.local_address();
    }

    future<> close() {
        _chan.close();
        return make_ready_future<>();
    }

    // --- Abstract Base Class for Implementation ---
    class impl {
    public:
        virtual ~impl() = default;
        // Pass the channel reference to implementation methods
        virtual future<> join(datagram_channel& chan, const std::string& interface_name, const socket_address& mcast_addr) = 0;
        virtual future<> leave(datagram_channel& chan, const std::string& interface_name, const socket_address& mcast_addr) = 0;
        // Add virtual close method if implementations need specific cleanup logic
        // virtual future<> close() { return make_ready_future<>(); }
    };
    // --- End Abstract Base Class ---

private:
    friend class posix_network_stack; // Allow stack to call private constructor
#ifdef SEASTAR_HAVE_DPDK
    // Conditionally declare friend if native_network_stack is conditional
    friend class native_network_stack;
#endif

    datagram_channel _chan;
    std::unique_ptr<impl> _impl;

    // --- Private Constructors ---
    // Constructor used by the factory methods in network_stack implementations
    multicast_udp_channel(datagram_channel chan, std::unique_ptr<impl> impl);
    multicast_udp_channel(const multicast_udp_channel&) = delete;            // Ensure no copy construction
    multicast_udp_channel& operator=(const multicast_udp_channel&) = delete; // Ensure no copy assignment
};

} // namespace seastar::net