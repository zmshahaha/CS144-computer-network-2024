#pragma once

#include <memory>
#include <optional>

#include "exception.hh"
#include "network_interface.hh"

// \brief A router that has multiple network interfaces and
// performs longest-prefix-match routing between them.
class Router
{
public:
  // Add an interface to the router
  // \param[in] interface an already-constructed network interface
  // \returns The index of the interface after it has been added to the router
  size_t add_interface( std::shared_ptr<NetworkInterface> interface )
  {
    _interfaces.push_back( notnull( "add_interface", std::move( interface ) ) );
    return _interfaces.size() - 1;
  }

  // Access an interface by index
  std::shared_ptr<NetworkInterface> interface( const size_t N ) { return _interfaces.at( N ); }

  // Add a route (a forwarding rule)
  void add_route( const uint32_t route_prefix,
                  const uint8_t prefix_length,
                  const std::optional<Address> next_hop,
                  const size_t interface_num );

  // Route packets between the interfaces
  void route();

private:
  typedef struct RouteEntry {
    uint32_t route_prefix {};
    uint8_t prefix_length {};
    std::optional<Address> next_hop {};
    size_t interface_num {};
  } RouteEntry;

  std::vector<RouteEntry> route_table_ {};

  // The router's collection of network interfaces
  std::vector<std::shared_ptr<NetworkInterface>> _interfaces {};

  // route specific datagram
  void route_single_packet(InternetDatagram &packet);

  // ipaddr and routepre is host byte order
  bool is_match(const uint32_t ip_addr, const uint32_t route_prefix, const uint8_t prefix_length) {
    uint32_t mask = ((1U << (prefix_length % 32)) - 1) << ((32 - prefix_length) % 32);
    return ((mask & ip_addr) == route_prefix);
  }
};
