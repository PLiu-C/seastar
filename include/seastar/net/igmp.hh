#pragma once

#include <seastar/net/ip.hh>
#include <seastar/net/packet.hh>

namespace seastar {
namespace net {

// IGMP constants
constexpr uint8_t IGMP_PROTO = 2; // IP protocol number for IGMP
constexpr uint8_t IGMP_JOIN = 0x16; // Membership Report (v2)
constexpr uint8_t IGMP_LEAVE = 0x17; // Leave Group

// IGMP packet structure
struct igmp_header {
    uint8_t type;
    uint8_t max_response_time;
    uint16_t checksum;
    ipv4_address group_address;
} __attribute__((packed));

class igmp {
private:
    ipv4& _ip;
    
public:
    explicit igmp(ipv4& ip) : _ip(ip) {}
    
    // Send IGMP join message
    future<> send_join(ipv4_address group);
    
    // Send IGMP leave message
    future<> send_leave(ipv4_address group);
    
    // Process received IGMP packet
    bool process_packet(packet& p, ipv4_address from, ipv4_address to);
};

} // namespace net
} // namespace seastar 