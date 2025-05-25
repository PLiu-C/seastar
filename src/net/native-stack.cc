/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright (C) 2014 Cloudius Systems, Ltd.
 */

#ifdef SEASTAR_MODULE
module;
#endif

#include <chrono>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <queue>

#include <seastar/util/assert.hh>

#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <unistd.h>

#ifdef SEASTAR_MODULE
module seastar;
#else
#include <seastar/net/native-stack.hh>
#include "net/native-stack-impl.hh"
#include <seastar/net/net.hh>
#include <seastar/net/ip.hh>
#include <seastar/net/tcp-stack.hh>
#include <seastar/net/tcp.hh>
#include <seastar/net/udp.hh>
#include <seastar/net/virtio.hh>
#include <seastar/net/dpdk.hh>
#include <seastar/net/proxy.hh>
#include <seastar/net/dhcp.hh>
#include <seastar/net/config.hh>
#include <seastar/core/reactor.hh>
#include <seastar/net/api.hh>
#ifdef SEASTAR_HAVE_DPDK
#include <seastar/net/multicast_udp_channel.hh>
#include "native-stack-multicast-impl.hh"
#include <rte_ethdev.h>
#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <fmt/ostream.h>
#include <cstdio> // For stderr
#endif
#endif

namespace seastar {

namespace net {

using namespace seastar;

void create_native_net_device(const native_stack_options& opts) {

    bool deprecated_config_used = true;

    std::stringstream net_config;

    if ( opts.net_config) {
        deprecated_config_used = false;
        net_config << opts.net_config.get_value();
    }
    if ( opts.net_config_file) {
        deprecated_config_used = false;
        std::fstream fs(opts.net_config_file.get_value());
        net_config << fs.rdbuf();
    }

    std::unique_ptr<device> dev;

    if ( deprecated_config_used) {
#ifdef SEASTAR_HAVE_DPDK
        if ( opts.dpdk_pmd) {
             dev = create_dpdk_net_device(opts.dpdk_opts.dpdk_port_index.get_value(), smp::count,
                !(opts.lro && opts.lro.get_value() == "off"),
                !(opts.dpdk_opts.hw_fc && opts.dpdk_opts.hw_fc.get_value() == "off"));
       } else
#endif
        dev = create_virtio_net_device(opts.virtio_opts, opts.lro);
    }
    else {
        auto device_configs = parse_config(net_config);

        if ( device_configs.size() > 1) {
            std::runtime_error("only one network interface is supported");
        }

        for ( auto&& device_config : device_configs) {
            auto& hw_config = device_config.second.hw_cfg;
#ifdef SEASTAR_HAVE_DPDK
            if ( hw_config.port_index || !hw_config.pci_address.empty() ) {
	            dev = create_dpdk_net_device(hw_config);
	        } else
#endif
            {
                (void)hw_config;
                std::runtime_error("only DPDK supports new configuration format");
            }
        }
    }

    auto sem = std::make_shared<semaphore>(0);
    std::shared_ptr<device> sdev(dev.release());
    // set_local_queue on all shard in the background,
    // signal when done.
    // FIXME: handle exceptions
    for (unsigned i = 0; i < smp::count; i++) {
        (void)smp::submit_to(i, [&opts, sdev] {
            uint16_t qid = this_shard_id();
            if (qid < sdev->hw_queues_count()) {
                auto qp = sdev->init_local_queue(opts, qid);
                std::map<unsigned, float> cpu_weights;
                for (unsigned i = sdev->hw_queues_count() + qid % sdev->hw_queues_count(); i < smp::count; i+= sdev->hw_queues_count()) {
                    cpu_weights[i] = 1;
                }
                cpu_weights[qid] = opts.hw_queue_weight.get_value();
                qp->configure_proxies(cpu_weights);
                sdev->set_local_queue(std::move(qp));
            } else {
                auto master = qid % sdev->hw_queues_count();
                sdev->set_local_queue(create_proxy_net_device(master, sdev.get()));
            }
        }).then([sem] {
            sem->signal();
        });
    }
    // wait for all shards to set their local queue,
    // then when link is ready, communicate the native_stack to the caller
    // via `create_native_stack` (that sets the ready_promise value)
    (void)sem->wait(smp::count).then([&opts, sdev] {
        // FIXME: future is discarded
        (void)sdev->link_ready().then([&opts, sdev] {
            for (unsigned i = 0; i < smp::count; i++) {
                // FIXME: future is discarded
                (void)smp::submit_to(i, [&opts, sdev] {
                    create_native_stack(opts, sdev);
                });
            }
        });
    });
}

// native_network_stack
class native_network_stack : public network_stack {
public:
    static thread_local promise<std::unique_ptr<network_stack>> ready_promise;
#ifdef SEASTAR_HAVE_DPDK
    // Public method to be called by multicast_udp_channel_impl
    future<> dispatch_igmp_packet_for_dpdk_multicast(ipv4_address destination_ip, ipv4_address group_address, uint8_t igmp_type, const std::string& interface_name);
    std::optional<uint16_t> get_dpdk_port_id() const { return _dpdk_port_id; }
#endif
private:
    interface _netif;
    ipv4 _inet;
    bool _dhcp = false;
    promise<> _config;
    timer<> _timer;
#ifdef SEASTAR_HAVE_DPDK
    std::optional<uint16_t> _dpdk_port_id;
    // Low-level packet sender
    future<> send_igmp_packet_low_level(ipv4_address source_ip, ipv4_address destination_ip, packet p);
#endif

    future<> run_dhcp(bool is_renew = false, const dhcp::lease & res = dhcp::lease());
    void on_dhcp(std::optional<dhcp::lease> lease, bool is_renew);
    void set_ipv4_packet_filter(ip_packet_filter* filter) {
        _inet.set_packet_filter(filter);
    }
    using tcp4 = tcp<ipv4_traits>;
public:
    explicit native_network_stack(const native_stack_options& opts, std::shared_ptr<device> dev);
    virtual server_socket listen(socket_address sa, listen_options opt) override;
    virtual ::seastar::socket socket() override;
    virtual udp_channel make_udp_channel(const socket_address& addr) override;
    virtual net::datagram_channel make_unbound_datagram_channel(sa_family_t) override;
    virtual net::datagram_channel make_bound_datagram_channel(const socket_address& local) override;
    virtual future<> initialize() override;
    static future<std::unique_ptr<network_stack>> create(const program_options::option_group& opts) {
        auto ns_opts = dynamic_cast<const native_stack_options*>(&opts);
        SEASTAR_ASSERT(ns_opts);
        if (this_shard_id() == 0) {
            create_native_net_device(*ns_opts);
        }
        return ready_promise.get_future();
    }
    virtual bool has_per_core_namespace() override { return true; };
    void arp_learn(ethernet_address l2, ipv4_address l3) {
        _inet.learn(l2, l3);
    }
    friend class native_server_socket_impl<tcp4>;

    class native_network_interface;
    friend class native_network_interface;

    std::vector<network_interface> network_interfaces() override;

    virtual statistics stats(unsigned scheduling_group_id) override {
        return statistics{
            internal::native_stack_net_stats::bytes_sent[scheduling_group_id],
            internal::native_stack_net_stats::bytes_received[scheduling_group_id],
        };
    }

    virtual void clear_stats(unsigned scheduling_group_id) override {
        internal::native_stack_net_stats::bytes_sent[scheduling_group_id] = 0;
        internal::native_stack_net_stats::bytes_received[scheduling_group_id] = 0;
    }

    virtual multicast_udp_channel make_multicast_udp_channel(const socket_address& local_addr) override {
        // Create the underlying datagram channel for the native stack
        net::datagram_channel chan = this->make_bound_datagram_channel(local_addr);
        // Create the specific multicast implementation
        auto impl = std::make_unique<native_multicast_udp_channel_impl>();
        // Construct and return using the private constructor (possible because of friend declaration)
        return multicast_udp_channel(std::move(chan), std::move(impl));
    }
};

thread_local promise<std::unique_ptr<network_stack>> native_network_stack::ready_promise;

udp_channel
native_network_stack::make_udp_channel(const socket_address& addr) {
    return _inet.get_udp().make_channel(addr);
}

net::datagram_channel native_network_stack::make_unbound_datagram_channel(sa_family_t family) {
    if (family != AF_INET) {
        throw std::runtime_error("Unsupported address family");
    }

    return _inet.get_udp().make_channel({});
}

net::datagram_channel native_network_stack::make_bound_datagram_channel(const socket_address& local) {
    return _inet.get_udp().make_channel(local);
}

native_network_stack::native_network_stack(const native_stack_options& opts, std::shared_ptr<device> dev)
    : _netif(std::move(dev))
    , _inet(&_netif) {
    _inet.get_udp().set_queue_size(opts.udpv4_queue_size.get_value());
    _dhcp = opts.host_ipv4_addr.defaulted()
            && opts.gw_ipv4_addr.defaulted()
            && opts.netmask_ipv4_addr.defaulted() && opts.dhcp.get_value();
    if (!_dhcp) {
        _inet.set_host_address(ipv4_address(opts.host_ipv4_addr.get_value()));
        _inet.set_gw_address(ipv4_address(opts.gw_ipv4_addr.get_value()));
        _inet.set_netmask_address(ipv4_address(opts.netmask_ipv4_addr.get_value()));
    }
#ifdef SEASTAR_HAVE_DPDK
    if (_netif.get_device()) {
        _dpdk_port_id = _netif.get_device()->get_dpdk_port_id();
    }
#endif
}

server_socket
native_network_stack::listen(socket_address sa, listen_options opts) {
    SEASTAR_ASSERT(sa.family() == AF_INET || sa.is_unspecified());
    return tcpv4_listen(_inet.get_tcp(), ntohs(sa.as_posix_sockaddr_in().sin_port), opts);
}

seastar::socket native_network_stack::socket() {
    return tcpv4_socket(_inet.get_tcp());
}

using namespace std::chrono_literals;

future<> native_network_stack::run_dhcp(bool is_renew, const dhcp::lease& res) {
    dhcp d(_inet);
    // Hijack the ip-stack.
    auto f = d.get_ipv4_filter();
    return smp::invoke_on_all([f] {
        auto & ns = static_cast<native_network_stack&>(engine().net());
        ns.set_ipv4_packet_filter(f);
    }).then([this, d = std::move(d), is_renew, res = res]() mutable {
        net::dhcp::result_type fut = is_renew ? d.renew(res) : d.discover();
        return fut.then([this, is_renew](std::optional<dhcp::lease> lease) {
            return smp::invoke_on_all([] {
                auto & ns = static_cast<native_network_stack&>(engine().net());
                ns.set_ipv4_packet_filter(nullptr);
            }).then(std::bind(&net::native_network_stack::on_dhcp, this, lease, is_renew));
        }).finally([d = std::move(d)] {});
    });
}

void native_network_stack::on_dhcp(std::optional<dhcp::lease> lease, bool is_renew) {
    if (lease) {
        auto& res = *lease;
        _inet.set_host_address(res.ip);
        _inet.set_gw_address(res.gateway);
        _inet.set_netmask_address(res.netmask);
    }
    // Signal waiters.
    if (!is_renew) {
        _config.set_value();
    }

    if (this_shard_id() == 0) {
        // And the other cpus, which, in the case of initial discovery,
        // will be waiting for us.
        for (unsigned i = 1; i < smp::count; i++) {
            (void)smp::submit_to(i, [lease, is_renew]() {
                auto & ns = static_cast<native_network_stack&>(engine().net());
                ns.on_dhcp(lease, is_renew);
            });
        }
        if (lease) {
            // And set up to renew the lease later on.
            auto& res = *lease;
            _timer.set_callback(
                    [this, res]() {
                        _config = promise<>();
                        // callback ignores future result
                        (void)run_dhcp(true, res);
                    });
            _timer.arm(
                    std::chrono::duration_cast<steady_clock_type::duration>(
                            res.lease_time));
        }
    }
}

future<> native_network_stack::initialize() {
    return network_stack::initialize().then([this]() {
        if (!_dhcp) {
            return make_ready_future();
        }

        // Only run actual discover on main cpu.
        // All other cpus must simply for main thread to complete and signal them.
        if (this_shard_id() == 0) {
            // FIXME: future is discarded
            (void)run_dhcp();
        }
        return _config.get_future();
    });
}

void arp_learn(ethernet_address l2, ipv4_address l3)
{
    // Run arp_learn on all shard in the background
    (void)smp::invoke_on_all([l2, l3] {
        auto & ns = static_cast<native_network_stack&>(engine().net());
        ns.arp_learn(l2, l3);
    });
}

void create_native_stack(const native_stack_options& opts, std::shared_ptr<device> dev) {
    native_network_stack::ready_promise.set_value(std::unique_ptr<network_stack>(std::make_unique<native_network_stack>(opts, std::move(dev))));
}

native_stack_options::native_stack_options()
    : program_options::option_group(nullptr, "Native networking stack options")
    // these two are ghost options
    , net_config(*this, "net-config", program_options::unused{})
    , net_config_file(*this, "net-config-file", program_options::unused{})
    , tap_device(*this, "tap-device",
                "tap0",
                "tap device to connect to")
    , host_ipv4_addr(*this, "host-ipv4-addr",
                "192.168.122.2",
                "static IPv4 address to use")
    , gw_ipv4_addr(*this, "gw-ipv4-addr",
                "192.168.122.1",
                "static IPv4 gateway to use")
    , netmask_ipv4_addr(*this, "netmask-ipv4-addr",
                "255.255.255.0",
                "static IPv4 netmask to use")
    , udpv4_queue_size(*this, "udpv4-queue-size",
                ipv4_udp::default_queue_size,
                "Default size of the UDPv4 per-channel packet queue")
    , dhcp(*this, "dhcp",
                true,
                        "Use DHCP discovery")
    , hw_queue_weight(*this, "hw-queue-weight",
                1.0f,
                "Weighing of a hardware network queue relative to a software queue (0=no work, 1=equal share)")
#ifdef SEASTAR_HAVE_DPDK
    , dpdk_pmd(*this, "dpdk-pmd", "Use DPDK PMD drivers")
#else
    , dpdk_pmd(*this, "dpdk-pmd", program_options::unused{})
#endif
    , lro(*this, "lro",
                "on",
                "Enable LRO")
    , virtio_opts(this)
    , dpdk_opts(this)
{
}

network_stack_entry register_native_stack() {
    return network_stack_entry{"native", std::make_unique<native_stack_options>(), native_network_stack::create, false};
}

class native_network_stack::native_network_interface : public net::network_interface_impl {
    const native_network_stack& _stack;
    std::vector<net::inet_address> _addresses;
    std::vector<uint8_t> _hardware_address;
public:
    native_network_interface(const native_network_stack& stack)
        : _stack(stack)
        , _addresses(1, _stack._inet.host_address())
    {
        const auto mac = _stack._inet.netif()->hw_address().mac;
        _hardware_address = std::vector<uint8_t>{mac.cbegin(), mac.cend()};
    }
    native_network_interface(const native_network_interface&) = default;

    uint32_t index() const override {
        return 0;
    }
    uint32_t mtu() const override {
        return _stack._inet.netif()->hw_features().mtu;
    }
    const sstring& name() const override {
        static const sstring name = "if0";
        return name;
    }
    const sstring& display_name() const override {
        return name();
    }
    const std::vector<net::inet_address>& addresses() const override {
        return _addresses;
    }
    const std::vector<uint8_t> hardware_address() const override {
        return _hardware_address;
    }
    bool is_loopback() const override {
        return false;
    }
    bool is_virtual() const override {
        return false;
    }
    bool is_up() const override {
        return true;
    }
    bool supports_ipv6() const override {
        return false;
    }
};

std::vector<network_interface> native_network_stack::network_interfaces() {
    if (!_inet.netif()) {
        return {};
    }

    static const native_network_interface nwif(*this);

    std::vector<network_interface> res;
    res.emplace_back(make_shared<native_network_interface>(nwif));
    return res;
}


// PL: multicast impl
#ifdef SEASTAR_HAVE_DPDK
logger nmc_logger("multicast_native");

namespace igmp {
// IGMP definitions
// IP Protocol number for IGMP
constexpr uint8_t IPPROTO_IGMP = 2;
// IGMP Message Types
constexpr uint8_t IGMP_MEMBERSHIP_QUERY     = 0x11;
constexpr uint8_t IGMP_V1_MEMBERSHIP_REPORT = 0x12;
constexpr uint8_t IGMP_V2_MEMBERSHIP_REPORT = 0x16;
constexpr uint8_t IGMP_V2_LEAVE_GROUP       = 0x17;

#pragma pack(push, 1)
struct igmp_hdr {
    uint8_t type;
    uint8_t max_resp_time;
    uint16_t checksum;
    ipv4_address group_address;

    igmp_hdr(uint8_t t, const ipv4_address& group)
        : type(t), max_resp_time(0), checksum(0), group_address(group) {}

    void hton_fields() { // Renamed to avoid conflict if seastar::net::hton is brought in by using namespace
        checksum = seastar::net::hton(checksum);
    }
};
#pragma pack(pop)

// IP Router Alert option (RFC 2113)
constexpr uint32_t IP_ROUTER_ALERT_OPTION = 0x94040000; // Network byte order

uint16_t calculate_checksum(const void* data, size_t length) {
    checksummer csum;
    csum.sum(reinterpret_cast<const char*>(data), length);
    return csum.get();
}

// Define after ipv4_address is complete
const ipv4_address ALL_ROUTERS_MULTICAST_ADDRESS("224.0.0.2");

} // namespace igmp (inside seastar::net)

// Convert IP multicast address to Ethernet multicast MAC
rte_ether_addr ip_mcast_to_mac(const socket_address& mcast_addr) {
    rte_ether_addr mac;
    if (mcast_addr.family() == AF_INET) {
        ipv4_addr ipv4(mcast_addr);
        uint32_t ip_addr_host_order = ntoh(ipv4.ip);

        if ((ip_addr_host_order & 0xF0000000) != 0xE0000000) { // Check in host order
             throw std::runtime_error(fmt::format("IPv4 address {} is not a multicast address", ipv4));
        }
        // IP multicast MAC: 01:00:5e:00:00:00 to 01:00:5e:7f:ff:ff
        // Lower 23 bits of IP address are mapped.
        mac.addr_bytes[0] = 0x01;
        mac.addr_bytes[1] = 0x00;
        mac.addr_bytes[2] = 0x5e;
        mac.addr_bytes[3] = (ip_addr_host_order >> 16) & 0x7f;
        mac.addr_bytes[4] = (ip_addr_host_order >> 8) & 0xff;
        mac.addr_bytes[5] = ip_addr_host_order & 0xff;
        return mac;
    } 
    throw std::runtime_error(fmt::format("Unsupported address family {} for multicast MAC conversion", mcast_addr.family()));
}

// Definition for send_igmp_packet_low_level
seastar::future<> native_network_stack::send_igmp_packet_low_level(seastar::net::ipv4_address source_ip, seastar::net::ipv4_address destination_ip, seastar::net::packet p) {
    seastar::future<seastar::net::ethernet_address> f_eth_dst_resolved = [this, &destination_ip] () -> seastar::future<seastar::net::ethernet_address> { 
        if (is_ipv4_multicast(destination_ip)) { // Use helper function
            try {
                // Convert the multicast IP destination_ip to its corresponding Ethernet multicast MAC.
                // The ip_mcast_to_mac function takes a socket_address. Port number is irrelevant for MAC conversion.
                seastar::net::socket_address mcast_sock_addr(destination_ip, 0); 
                rte_ether_addr rte_mac = ip_mcast_to_mac(mcast_sock_addr); // Assumes ip_mcast_to_mac is accessible
                return seastar::make_ready_future<seastar::net::ethernet_address>(seastar::net::ethernet_address(rte_mac.addr_bytes));
            } catch (const std::exception& e) {
                fmt::print(stderr, "ERROR: IGMP: Failed to convert multicast IP {} to MAC for L2 destination: {}\n", fmt::streamed(destination_ip), e.what());
                return seastar::make_exception_future<seastar::net::ethernet_address>(e); // Propagate exception
            }
        } else {
            // For unicast, use the existing ARP/neighbor discovery mechanism
            return _inet.get_l2_dst_address(destination_ip);
        }
    }(); // Immediately-invoked lambda

    return f_eth_dst_resolved.then_wrapped([this, destination_ip, p = std::move(p)] (seastar::future<seastar::net::ethernet_address> f_eth_dst_inner) mutable {
        try {
            seastar::net::ethernet_address eth_dst = f_eth_dst_inner.get(); // This will throw if f_eth_dst_inner had an exception
            
            auto eh = p.prepend_header<seastar::net::eth_hdr>();
            eh->dst_mac = eth_dst;
            eh->src_mac = _netif.hw_address(); // hw_address() should provide the MAC of the DPDK NIC
            eh->eth_proto = seastar::net::hton(static_cast<uint16_t>(seastar::net::eth_protocol_num::ipv4));
            
            fmt::print(stderr, "TRACE: IGMP: Sending packet with L2 Dest MAC {} for IP Dest {}\n", eth_dst, fmt::streamed(destination_ip));
            return _netif.get_device()->local_queue().send(std::move(p));
        } catch (const std::system_error& e) {
            if (e.code().value() == ENETUNREACH || e.code().value() == EHOSTUNREACH) {
                fmt::print(stderr, "WARN: IGMP: Could not send packet, L2 destination for {} unreachable (MAC {}): {}\n", 
                    fmt::streamed(destination_ip), 
                    f_eth_dst_inner.failed() ? seastar::sstring("unknown") : seastar::sstring(fmt::to_string(f_eth_dst_inner.get())),
                    e.what());
            } else {
                fmt::print(stderr, "ERROR: IGMP: System error sending packet to {} (L2 MAC {}): {}\n", 
                    fmt::streamed(destination_ip), 
                    f_eth_dst_inner.failed() ? seastar::sstring("unknown") : seastar::sstring(fmt::to_string(f_eth_dst_inner.get())),
                    e.what());
            }
        } catch (const std::exception& e) {
            fmt::print(stderr, "ERROR: IGMP: Exception sending packet to {} (L2 MAC {}): {}\n", 
                fmt::streamed(destination_ip), 
                f_eth_dst_inner.failed() ? seastar::sstring("unknown") : seastar::sstring(fmt::to_string(f_eth_dst_inner.get())),
                e.what());
        }
        return seastar::make_ready_future<>(); // Suppress error for IGMP, it's often best-effort
    });
}

// Definition for dispatch_igmp_packet_for_dpdk_multicast
seastar::future<> native_network_stack::dispatch_igmp_packet_for_dpdk_multicast(
    seastar::net::ipv4_address destination_ip, seastar::net::ipv4_address group_address_param, uint8_t igmp_type, const std::string& interface_name) {

    seastar::net::ipv4_address source_ip = _inet.host_address();
    if (seastar::net::is_unspecified(source_ip)) {
        fmt::print(stderr, "WARN: Cannot send IGMP message: source IP for native stack ({}) is unspecified.\n", interface_name);
        return seastar::make_ready_future<>();
    }
    fmt::print(stderr, "DEBUG: IGMP: Sending type {} to {} for group {} from {} via dev {}\n", igmp_type, fmt::streamed(destination_ip), fmt::streamed(group_address_param), fmt::streamed(source_ip), interface_name);
    // ... rest of dispatch_igmp_packet_for_dpdk_multicast logic ...

    // Prepare IP header fields
    ip_hdr iph_template;
    iph_template.ihl = 5; // Base header length initially
    iph_template.ver = 4;
    iph_template.dscp = 0;
    iph_template.ecn = 0;
    // Generate a proper IP ID for IGMP packets
    static thread_local uint16_t ip_id_counter = 1;
    iph_template.id = seastar::net::hton(ip_id_counter++);
    iph_template.frag = 0;
    iph_template.ttl = 1;
    iph_template.ip_proto = igmp::IPPROTO_IGMP;
    iph_template.csum = 0;
    iph_template.src_ip = source_ip;
    iph_template.dst_ip = destination_ip;

    // Construct IGMP message
    igmp::igmp_hdr igmp_msg(igmp_type, group_address_param);
    // Calculate checksum on host byte order data first
    igmp_msg.checksum = igmp::calculate_checksum(&igmp_msg, sizeof(igmp_msg));
    // Convert everything to network byte order (including the checksum)
    igmp_msg.hton_fields();

    // Total packet length and IHL calculation (IP header + Router Alert + IGMP payload)
    size_t ip_header_base_len = sizeof(ip_hdr);
    size_t router_alert_option_len = sizeof(uint32_t);
    size_t igmp_payload_len = sizeof(igmp::igmp_hdr);
    size_t ip_options_len = router_alert_option_len;
    size_t ip_header_total_len = ip_header_base_len + ip_options_len;

    iph_template.ihl = ip_header_total_len / 4; // IHL in 32-bit words
    iph_template.len = seastar::net::hton(uint16_t(ip_header_total_len + igmp_payload_len));
    // id, src_ip, dst_ip are already handled or fine.

    // Create a single buffer for IP header (with options) + IGMP message
    size_t total_packet_size = ip_header_total_len + igmp_payload_len;
    auto pkt_buf = temporary_buffer<char>(total_packet_size);
    char* current_ptr = pkt_buf.get_write();

    // Copy base IP header template
    memcpy(current_ptr, &iph_template, ip_header_base_len);

    // Copy IP Router Alert Option
    uint32_t router_alert_val_nbo = igmp::IP_ROUTER_ALERT_OPTION;
    memcpy(current_ptr + ip_header_base_len, &router_alert_val_nbo, router_alert_option_len);

    // Calculate IP checksum on (IP header + IP Options)
    ip_hdr* iph_in_buf = reinterpret_cast<ip_hdr*>(current_ptr);
    iph_in_buf->csum = igmp::calculate_checksum(current_ptr, ip_header_total_len);
    iph_in_buf->csum = seastar::net::hton(iph_in_buf->csum);

    // Copy IGMP message
    memcpy(current_ptr + ip_header_total_len, &igmp_msg, igmp_payload_len);

    // Create packet from the contiguous buffer
    packet p(std::move(pkt_buf));

    offload_info oi; // IP checksum is pre-calculated, IGMP checksum is in payload
    oi.protocol = static_cast<ip_protocol_num>(igmp::IPPROTO_IGMP);
    oi.needs_ip_csum = false;
    oi.needs_csum = false;
    p.set_offload_info(oi);

    return send_igmp_packet_low_level(source_ip, destination_ip, std::move(p));
}

// native_multicast_udp_channel_impl join/leave methods updated to call the new IGMP dispatch method
// (Code for join and leave remains largely the same as in the previous step, just ensuring correct calls to dispatch_igmp_packet_for_dpdk_multicast)

future<> native_multicast_udp_channel_impl::join(datagram_channel& chan, const std::string& interface_name, const socket_address& mcast_addr) {
    nmc_logger.debug("Joining multicast group {} on interface {}", mcast_addr, interface_name);

    auto& native_stack_ref = static_cast<native_network_stack&>(engine().net());
    std::optional<uint16_t> opt_port_id = native_stack_ref.get_dpdk_port_id();

    if (!opt_port_id) {
        return make_exception_future<>(std::runtime_error(
            fmt::format("DPDK port_id not available for native stack multicast join on interface '{}'", interface_name)));
    }
    uint16_t port_id = *opt_port_id;

    rte_ether_addr mcast_mac = ip_mcast_to_mac(mcast_addr);

    // Call DPDK function to add the MAC address to the device's filter table
    int ret = rte_eth_dev_mac_addr_add(port_id, &mcast_mac, 0); // Pool 0 for unicast/multicast, check DPDK docs if pool matters

    if (ret < 0) {
        // rte_errno is thread-local, check its value for more details
        int dpdk_errno = rte_errno;
        nmc_logger.error("rte_eth_dev_mac_addr_add failed for port {} MAC {} (errno={}): {}",
                        port_id, ethernet_address(mcast_mac.addr_bytes), dpdk_errno, rte_strerror(dpdk_errno));
        return make_exception_future<>(std::system_error(-ret, std::system_category(),
            "rte_eth_dev_mac_addr_add failed: " + sstring(rte_strerror(dpdk_errno))));
    }

    nmc_logger.debug("Successfully added MAC {} for multicast group {} to port {}", ethernet_address(mcast_mac.addr_bytes), mcast_addr, port_id);

    ipv4_addr group_ipv4(mcast_addr);
    return native_stack_ref.dispatch_igmp_packet_for_dpdk_multicast(group_ipv4, group_ipv4, igmp::IGMP_V2_MEMBERSHIP_REPORT, interface_name);
}

future<> native_multicast_udp_channel_impl::leave(datagram_channel& chan, const std::string& interface_name, const socket_address& mcast_addr) {
    nmc_logger.debug("Leaving multicast group {} on interface {}", mcast_addr, interface_name);

    auto& native_stack_ref = static_cast<native_network_stack&>(engine().net());
    std::optional<uint16_t> opt_port_id = native_stack_ref.get_dpdk_port_id();

    if (!opt_port_id) {
        // Log an error but proceed with MAC removal if possible, or decide if this is fatal
        nmc_logger.error("DPDK port_id not available for native stack multicast leave on interface '{}'. Cannot send IGMP Leave.", interface_name);
        // Depending on desired behavior, could return early or try to remove MAC if pci->port_id mapping is assumed elsewhere
        // For now, let's make it consistent with join and return an error if port_id is essential for MAC removal too.
        return make_exception_future<>(std::runtime_error(
            fmt::format("DPDK port_id not available for native stack multicast leave on interface '{}'", interface_name)));
    }
    uint16_t port_id = *opt_port_id;

    rte_ether_addr mcast_mac = ip_mcast_to_mac(mcast_addr);

    // Call DPDK function to remove the MAC address
    int ret = rte_eth_dev_mac_addr_remove(port_id, &mcast_mac);

    if (ret < 0) {
        int dpdk_errno = rte_errno;
        nmc_logger.error("rte_eth_dev_mac_addr_del failed for port {} MAC {} (errno={}): {}",
                           port_id, ethernet_address(mcast_mac.addr_bytes), dpdk_errno, rte_strerror(dpdk_errno));
    } else {
       nmc_logger.debug("Successfully removed MAC {} for multicast group {} from port {}", ethernet_address(mcast_mac.addr_bytes), mcast_addr, port_id);
    }

    ipv4_addr group_ipv4(mcast_addr);
    return native_stack_ref.dispatch_igmp_packet_for_dpdk_multicast(igmp::ALL_ROUTERS_MULTICAST_ADDRESS, group_ipv4, igmp::IGMP_V2_LEAVE_GROUP, interface_name);
}

#endif // SEASTAR_HAVE_DPDK

} // namespace net

} // namespace seastar
