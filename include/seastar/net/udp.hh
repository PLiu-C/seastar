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
 *
 */

#pragma once

#ifndef SEASTAR_MODULE
#include <unordered_map>
#include <assert.h>
#endif

#include <seastar/core/shared_ptr.hh>
#include <seastar/net/api.hh>
#include <seastar/net/const.hh>
#include <seastar/net/net.hh>
#include <seastar/util/modules.hh>

namespace seastar {

namespace net {

struct udp_hdr {
    packed<uint16_t> src_port;
    packed<uint16_t> dst_port;
    packed<uint16_t> len;
    packed<uint16_t> cksum;

    template<typename Adjuster>
    auto adjust_endianness(Adjuster a) {
        return a(src_port, dst_port, len, cksum);
    }
} __attribute__((packed));

struct udp_channel_state {
    queue<datagram> _queue;
    // Limit number of data queued into send queue
    semaphore _user_queue_space = {212992};
    udp_channel_state(size_t queue_size) : _queue(queue_size) {}
    future<> wait_for_send_buffer(size_t len) { return _user_queue_space.wait(len); }
    void complete_send(size_t len) { _user_queue_space.signal(len); }
};

class datagram_channel {
private:
    // Assuming we have these members or similar
    ipv4_udp* _udp;
    uint16_t _port;
    ipv4_address _address;

public:
    // ... existing methods ...
    
    // Add multicast group membership
    future<> add_membership(const socket_address& mcast_addr, const std::string& interface_name) {
        // Extract IP address from socket_address
        ipv4_address ip = mcast_addr.as_posix_sockaddr_in().sin_addr;
        
        // Join at IP layer via the UDP implementation
        if (_udp) {
            return _udp->join_multicast_group(ip);
        }
        
        return make_ready_future<>();
    }
    
    // Drop multicast group membership
    future<> drop_membership(const socket_address& mcast_addr, const std::string& interface_name) {
        ipv4_address ip = mcast_addr.as_posix_sockaddr_in().sin_addr;
        
        if (_udp) {
            return _udp->leave_multicast_group(ip);
        }
        
        return make_ready_future<>();
    }
};

class udp_channel {
public:
    // ... existing methods ...
    
    // Forward multicast methods to underlying implementation
    future<> add_membership(const socket_address& mcast_addr, const std::string& interface_name) {
        return _impl->add_membership(mcast_addr, interface_name);
    }
    
    future<> drop_membership(const socket_address& mcast_addr, const std::string& interface_name) {
        return _impl->drop_membership(mcast_addr, interface_name);
    }
};

}

}
