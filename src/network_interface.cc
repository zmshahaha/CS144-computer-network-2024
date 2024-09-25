#include <iostream>

#include "arp_message.hh"
#include "exception.hh"
#include "network_interface.hh"

using namespace std;

static constexpr size_t ARP_CACHE_RESERVE_TIME = 30000;
static constexpr size_t ARP_RESEND_TIME = 5000;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( string_view name,
                                    shared_ptr<OutputPort> port,
                                    const EthernetAddress& ethernet_address,
                                    const Address& ip_address )
  : name_( name )
  , port_( notnull( "OutputPort", move( port ) ) )
  , ethernet_address_( ethernet_address )
  , ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address ) << " and IP address "
       << ip_address.ip() << "\n";
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but
//! may also be another host if directly connected to the same network as the destination) Note: the Address type
//! can be converted to a uint32_t (raw 32-bit IP address) by using the Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  // Your code here.
  EthernetFrame eth_frame;
  ARPMessage arp_msg;

  eth_frame.header.src = ethernet_address_;

  if (dgram.header.ttl == 0) {
    // drop
    return;
  }

  // cached
  if (arp_cache_.count(next_hop.ipv4_numeric())) {
    eth_frame.header.dst = arp_cache_[next_hop.ipv4_numeric()].eth_addr_cache_;
    eth_frame.header.type = EthernetHeader::TYPE_IPv4;
    eth_frame.payload = move(serialize(move(dgram)));
    transmit(move(eth_frame));
  } else {
    datagrams_wait_for_arp_.insert({next_hop.ipv4_numeric(), move(dgram)});
    // send/resend arp
    if ((arp_resend_table_.count(next_hop.ipv4_numeric()) == 0) ||
        (arp_resend_table_[next_hop.ipv4_numeric()] == 0)) {
      arp_msg.opcode = ARPMessage::OPCODE_REQUEST;
      arp_msg.sender_ethernet_address = ethernet_address_;
      arp_msg.sender_ip_address = ip_address_.ipv4_numeric();
      arp_msg.target_ethernet_address = {};
      arp_msg.target_ip_address = next_hop.ipv4_numeric();
      eth_frame.header.dst = ETHERNET_BROADCAST;
      eth_frame.header.type = EthernetHeader::TYPE_ARP;
      eth_frame.payload = move(serialize(move(arp_msg)));
      transmit(move(eth_frame));
      arp_resend_table_[next_hop.ipv4_numeric()] = ARP_RESEND_TIME;
    }
  }
}

//! \param[in] frame the incoming Ethernet frame
void NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // Your code here.
  InternetDatagram dgram;
  ARPMessage arp_msg;
  EthernetFrame arp_reply_frame, resend_frame;
  ARPMessage arp_reply_msg;

  // ignore frame not send for us
  if ((frame.header.dst != ethernet_address_) && (frame.header.dst != ETHERNET_BROADCAST)) {
    return;
  }

  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    if (parse(dgram, frame.payload) == true) {
      datagrams_received_.push(move(dgram));
    }
  } else if (frame.header.type == EthernetHeader::TYPE_ARP) {
    if (parse(arp_msg, frame.payload) == true) {
      // learn the map
      arp_cache_[arp_msg.sender_ip_address] = {arp_msg.sender_ethernet_address, ARP_CACHE_RESERVE_TIME};

      // learn a map, and send prev dgram
      auto resend_frames = datagrams_wait_for_arp_.equal_range(arp_msg.sender_ip_address);
      for (auto msg = resend_frames.first; msg != resend_frames.second; msg++) {
        resend_frame.header.dst = arp_msg.sender_ethernet_address;  // next hop's mac
        resend_frame.header.src = ethernet_address_;
        resend_frame.header.type = EthernetHeader::TYPE_IPv4;
        resend_frame.payload = move(serialize(msg->second));
        transmit(resend_frame);
      }
      // remove dgram
      datagrams_wait_for_arp_.erase(resend_frames.first, resend_frames.second);
      // remove form arp resend table
      arp_resend_table_.erase(arp_msg.sender_ip_address);

      if ((arp_msg.opcode == ARPMessage::OPCODE_REQUEST) &&
          (arp_msg.target_ip_address == ip_address_.ipv4_numeric())) {
        arp_reply_msg.opcode = ARPMessage::OPCODE_REPLY;
        arp_reply_msg.sender_ethernet_address = ethernet_address_;
        arp_reply_msg.sender_ip_address = ip_address_.ipv4_numeric();
        arp_reply_msg.target_ethernet_address = arp_msg.sender_ethernet_address;
        arp_reply_msg.target_ip_address = arp_msg.sender_ip_address;
        arp_reply_frame.header.dst = arp_msg.sender_ethernet_address;
        arp_reply_frame.header.src = ethernet_address_;
        arp_reply_frame.header.type = EthernetHeader::TYPE_ARP;
        arp_reply_frame.payload = move(serialize(move(arp_reply_msg)));
        transmit(move(arp_reply_frame));
      }
    }
  }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  // Your code here.
  for (auto cache = arp_cache_.begin(); cache != arp_cache_.end(); ) {
    cout<<"reserve "<<cache->second.reserve_ms<<endl;
    if (cache->second.reserve_ms <= ms_since_last_tick) {
      cache = arp_cache_.erase(cache);
    } else {
      cache->second.reserve_ms -= ms_since_last_tick;
      cache++;
    }
  }

  // need &!!!!
  for (auto &arp_resend_info : arp_resend_table_) {
    if (arp_resend_info.second <= ms_since_last_tick) {
      arp_resend_info.second = 0;
    } else {
      arp_resend_info.second -= ms_since_last_tick;
    }
  }
}
