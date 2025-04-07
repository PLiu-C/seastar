#include <seastar/net/igmp.hh>
#include <seastar/net/ip.hh>
#include <seastar/net/ip_checksum.hh>

namespace seastar {
namespace net {

future<> igmp::send_join(ipv4_address group) {
    packet p;
    
    // Create IGMP header
    auto* ih = p.prepend_header<igmp_header>();
    ih->type = IGMP_JOIN;
    ih->max_response_time = 0; // Not used in Join
    ih->group_address = group;
    ih->checksum = 0;
    
    // Calculate checksum
    ih->checksum = ip_checksum(reinterpret_cast<void*>(ih), sizeof(*ih));
    
    // Send via IP
    return _ip.send(std::move(p), group, IGMP_PROTO);
}

future<> igmp::send_leave(ipv4_address group) {
    packet p;
    
    // Create IGMP header
    auto* ih = p.prepend_header<igmp_header>();
    ih->type = IGMP_LEAVE;
    ih->max_response_time = 0; // Not used in Leave
    ih->group_address = group;
    ih->checksum = 0;
    
    // Calculate checksum
    ih->checksum = ip_checksum(reinterpret_cast<void*>(ih), sizeof(*ih));
    
    // Send via IP
    return _ip.send(std::move(p), group, IGMP_PROTO);
}

bool igmp::process_packet(packet& p, ipv4_address from, ipv4_address to) {
    // Basic IGMP packet processing - mainly for completeness
    // A full implementation would handle Query messages and track timers
    
    auto* ih = p.get_header<igmp_header>(0);
    if (!ih) {
        return false;
    }
    
    // Verify checksum
    uint16_t cksum = ih->checksum;
    ih->checksum = 0;
    auto computed_cksum = ip_checksum(ih, sizeof(*ih));
    if (computed_cksum != cksum) {
        return false; // Invalid checksum
    }
    
    // Process based on type
    switch (ih->type) {
        // Handle query messages from routers
        // This would be expanded in a full implementation
        default:
            break;
    }
    
    return true;
}

} // namespace net
} // namespace seastar 