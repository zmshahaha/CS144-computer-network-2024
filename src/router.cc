#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

void Router::route_single_packet(InternetDatagram &packet)
{
  decltype(route_table_.size()) route_table_size = route_table_.size(), best_match_idx = route_table_size;
  uint8_t best_match_len {UINT8_MAX};
  uint32_t dst_addr = packet.header.dst;
  RouteEntry route_entry;

  for (decltype(route_table_.size()) i = 0; i < route_table_size; i++) {
    if (is_match(dst_addr, route_table_[i].route_prefix, route_table_[i].prefix_length) &&
        ((best_match_len < route_table_[i].prefix_length) || (best_match_len == UINT8_MAX))) {
      best_match_len = route_table_[i].prefix_length;
      best_match_idx = i;
    }
  }

  route_entry = route_table_[best_match_idx];

  // found a route entry
  if (best_match_idx != route_table_size) {
    packet.header.ttl --;
    if (packet.header.ttl == 0) {
      // drop
      return;
    }

    // recompute checksum
    packet.header.compute_checksum();

    if (route_entry.next_hop.has_value()) {
      interface(route_entry.interface_num)->send_datagram(packet, route_entry.next_hop.value());
    } else {
      interface(route_entry.interface_num)->send_datagram(packet, Address::from_ipv4_numeric(packet.header.dst));
    }
  }
  // else drop
}
// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";

  // Your code here.
  route_table_.push_back({move(route_prefix), move(prefix_length), move(next_hop), move(interface_num)});
}

// Go through all the interfaces, and route every incoming datagram to its proper outgoing interface.
void Router::route()
{
  // Your code here.
  for (auto iface : _interfaces) {
    auto &recv_queue = iface->datagrams_received();
    while (!recv_queue.empty()) {
      route_single_packet(recv_queue.front());
      recv_queue.pop();
    }
  }
}
