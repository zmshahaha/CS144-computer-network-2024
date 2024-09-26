## lab0

queue不支持容器遍历(不支持begin，end)，所以使用deque

string_view Reader::peek() const返回值为string_view则返回的必须是全局的变量，因为string_view不支持拷贝，所以不能返回栈上数据。

因为peek函数返回string_view,所以基本只能用类里现有string变量。string可以删除开头字符，可以作queue,所以直接string就行。

string_view Reader::peek() const 中的const表示对类的每一个成员都是const，根据编译时的提示

## lab1

auto &iter = buffer_.begin()语法是错误的，不能加&，因为buffer_.begin()是右值，不能赋给引用

```c
  for (auto iter = buffer_.begin(); iter != buffer_.end(); ) {
    uint64_t index = iter->first;//正确
    uint64_t index = iter.first();//错误
```

中间会删的map遍历需要这样写

```c
for (auto it = myMap.begin(); it != myMap.end(); ) {
    if (it->first % 2 == 0) { // 删除键为偶数的元素
        it = myMap.erase(it); // erase 返回下一个有效的迭代器
    } else {
        ++it; // 移动到下一个元素
    }
}
```

算法思路: 每次新加一个字符串，对map中最多只会增加一个string slice，所以最多只要insert一次。insert的这次就可以基于新加的字符串修改(最多就是左右两边扩充)，将所有与新串有重叠或者紧邻的全部删除。即只会增加新的data或者减少旧的slice。

如果有一个slice完全包含新增的，则不做任何修改

减少复杂度就是要看什么情况下才会作修改

注意以下在reassembler中的错误调法

尝试如下

```c
Reader _reader = output_.reader();
Reader _reader2 = output_.reader();
cout<<&_reader<<" "<<&_reader2<<endl;
```

会发现这两个地址不同

正确的应该是

```c
Reader &_reader = output_.reader();
cout<<&_reader<<" "<<&_reader2<<endl;
```

此时发现reader地址正常

## lab2

1 << 31 UL错，1UL << 31对

unwrap的当前seqno是调用者自己

## lab3

连续重传不确定是啥意思，这里取timeout时连续几次有重传的消息。

在这个sender模型中，每个位置的消息只会构造一次，之后发这个位置只会通过in flight buf，所以push的时候FIN直接取内部data是否完成而不用管发的是那个位置的(不会重复构造之前报文)。

If the window size is nonzero才会倍增重传时间，zero时应该是想及时看到它window变大

![alt text](images/win_non_zero.png.png)

这里出现错误ACK没有RST，testcase这样设定的

为什么要在新的ack接收才重新计时timer???

## lab4

需要研究下minnow的util是怎么支持socket操作的

## lab5

arp学习是从arp报文(不管是req还是reply)的发送方信息学习的

arp只会回复目标IP是本机的请求，不会回复目标IP只是在arp cache的请求

set/map的key必须是可比大小的，不然编译报错，不可比用unordered

unordered需要提供哈希函数

这里以下两个的键是相同，都是nexthop的ip，导致两个必须同时变化，引入依赖，可能需要优化

```c++
  std::multimap<uint32_t, InternetDatagram> datagrams_wait_for_arp_ {};
  std::map<uint32_t, size_t> arp_resend_table_ {};
```
```c++
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
      // 注意这里如果写成auto &msg = resend_frames.first，则会导致resend_frames.first会跟着变化
      // 直至msg = resend_frames.first = resend_frames.second。导致后面erase失效
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
    if (cache->second.reserve_ms <= ms_since_last_tick) {
      cache = arp_cache_.erase(cache);
    } else {
      cache->second.reserve_ms -= ms_since_last_tick;
      cache++;
    }
  }

  // 这里的auto后面一定要加&，否则不是改本身
  for (auto &arp_resend_info : arp_resend_table_) {
    if (arp_resend_info.second <= ms_since_last_tick) {
      arp_resend_info.second = 0;
    } else {
      arp_resend_info.second -= ms_since_last_tick;
    }
  }
}
```

## lab6

Address::from_ipv4_numeric的函数定义是static，所以可以不用Address对象来调用这个

route对所有收到的包都进行转发，这里应该是没有考虑包发给自己情况，看测试也没测到包目的是某个路由器

next_hop是空的是说直接向目标IP转发，不是说转发给自己

route主要是在不同的网络接口转发数据包，IP层不用变，只是mac层会变

## minnow

### TUN设备创建

```bash
ip tuntap add mode tun user zms name tun144
ip addr add 169.254.144.1/24 dev tun144
ip link set dev tun144 up
ip route change 169.254.144.0/24 dev tun144 rto_min 10ms
iptables -t nat -A PREROUTING -s 169.254.144.0/24 -j CONNMARK --set-mark 144
iptables -t nat -A POSTROUTING -j MASQUERADE -m connmark --mark 144
```

ip tuntap add mode tun user zms name tun144

strace系统调用

```c
openat(AT_FDCWD, "/dev/net/tun", O_RDWR) = 4
ioctl(4, TUNSETIFF, 0x7ffe1a43b580)     = 0
ioctl(4, TUNSETOWNER, 0x3e8)            = 0
ioctl(4, TUNSETPERSIST, 0x1)            = 0
close(4)                                = 0
```

看系统调用主要就是用netlink配置

ip addr add 169.254.144.1/24 dev tun144

```c
socket(AF_NETLINK, SOCK_RAW|SOCK_CLOEXEC, NETLINK_ROUTE) = 3
setsockopt(3, SOL_SOCKET, SO_SNDBUF, [32768], 4) = 0
setsockopt(3, SOL_SOCKET, SO_RCVBUF, [1048576], 4) = 0
setsockopt(3, SOL_NETLINK, NETLINK_EXT_ACK, [1], 4) = 0
bind(3, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 0
getsockname(3, {sa_family=AF_NETLINK, nl_pid=9194, nl_groups=00000000}, [12]) = 0
setsockopt(3, SOL_NETLINK, NETLINK_GET_STRICT_CHK, [1], 4) = 0
socket(AF_NETLINK, SOCK_RAW|SOCK_CLOEXEC, NETLINK_ROUTE) = 4
setsockopt(4, SOL_SOCKET, SO_SNDBUF, [32768], 4) = 0
setsockopt(4, SOL_SOCKET, SO_RCVBUF, [1048576], 4) = 0
setsockopt(4, SOL_NETLINK, NETLINK_EXT_ACK, [1], 4) = 0
bind(4, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 0
getsockname(4, {sa_family=AF_NETLINK, nl_pid=-1030586232, nl_groups=00000000}, [12]) = 0
sendmsg(4, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=52, nlmsg_type=RTM_GETLINK, nlmsg_flags=NLM_F_REQUEST, nlmsg_seq=1727310555, nlmsg_pid=0}, {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_NETROM, ifi_index=0, ifi_flags=0, ifi_change=0}, [[{nla_len=8, nla_type=IFLA_EXT_MASK}, RTEXT_FILTER_VF|RTEXT_FILTER_SKIP_STATS], [{nla_len=11, nla_type=IFLA_IFNAME}, "tun144"]]], iov_len=52}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 52
recvmsg(4, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=NULL, iov_len=0}], msg_iovlen=1, msg_controllen=0, msg_flags=MSG_TRUNC}, MSG_PEEK|MSG_TRUNC) = 1092
getrandom("\xd1\x0a\x54\x99\x52\xcd\x5a\xec", 8, GRND_NONBLOCK) = 8
brk(NULL)                               = 0x5a1d81ba5000
brk(0x5a1d81bc6000)                     = 0x5a1d81bc6000
recvmsg(4, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=1092, nlmsg_type=RTM_NEWLINK, nlmsg_flags=0, nlmsg_seq=1727310555, nlmsg_pid=-1030586232}, {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_NONE, ifi_index=if_nametoindex("tun144"), ifi_flags=IFF_POINTOPOINT|IFF_NOARP|IFF_MULTICAST, ifi_change=0}, [[{nla_len=11, nla_type=IFLA_IFNAME}, "tun144"], [{nla_len=8, nla_type=IFLA_TXQLEN}, 500], [{nla_len=5, nla_type=IFLA_OPERSTATE}, 2], [{nla_len=5, nla_type=IFLA_LINKMODE}, 0], [{nla_len=8, nla_type=IFLA_MTU}, 1500], [{nla_len=8, nla_type=IFLA_MIN_MTU}, 68], [{nla_len=8, nla_type=IFLA_MAX_MTU}, 65535], [{nla_len=8, nla_type=IFLA_GROUP}, 0], [{nla_len=8, nla_type=IFLA_PROMISCUITY}, 0], [{nla_len=8, nla_type=IFLA_ALLMULTI}, 0], [{nla_len=8, nla_type=IFLA_NUM_TX_QUEUES}, 1], [{nla_len=8, nla_type=IFLA_GSO_MAX_SEGS}, 65535], [{nla_len=8, nla_type=IFLA_GSO_MAX_SIZE}, 65536], [{nla_len=8, nla_type=IFLA_GRO_MAX_SIZE}, 65536], [{nla_len=8, nla_type=IFLA_GSO_IPV4_MAX_SIZE}, 65536], [{nla_len=8, nla_type=IFLA_GRO_IPV4_MAX_SIZE}, 65536], [{nla_len=8, nla_type=IFLA_TSO_MAX_SIZE}, 65536], [{nla_len=8, nla_type=IFLA_TSO_MAX_SEGS}, 65535], [{nla_len=8, nla_type=IFLA_NUM_RX_QUEUES}, 1], [{nla_len=5, nla_type=IFLA_CARRIER}, 0], [{nla_len=9, nla_type=IFLA_QDISC}, "noop"], [{nla_len=8, nla_type=IFLA_CARRIER_CHANGES}, 1], [{nla_len=8, nla_type=IFLA_CARRIER_UP_COUNT}, 0], [{nla_len=8, nla_type=IFLA_CARRIER_DOWN_COUNT}, 1], [{nla_len=5, nla_type=IFLA_PROTO_DOWN}, 0], [{nla_len=36, nla_type=IFLA_MAP}, {mem_start=0, mem_end=0, base_addr=0, irq=0, dma=0, port=0}], [{nla_len=204, nla_type=IFLA_STATS64}, {rx_packets=0, tx_packets=0, rx_bytes=0, tx_bytes=0, rx_errors=0, tx_errors=0, rx_dropped=0, tx_dropped=0, multicast=0, collisions=0, rx_length_errors=0, rx_over_errors=0, rx_crc_errors=0, rx_frame_errors=0, rx_fifo_errors=0, rx_missed_errors=0, tx_aborted_errors=0, tx_carrier_errors=0, tx_fifo_errors=0, tx_heartbeat_errors=0, tx_window_errors=0, rx_compressed=0, tx_compressed=0, rx_nohandler=0, rx_otherhost_dropped=0}], [{nla_len=100, nla_type=IFLA_STATS}, {rx_packets=0, tx_packets=0, rx_bytes=0, tx_bytes=0, rx_errors=0, tx_errors=0, rx_dropped=0, tx_dropped=0, multicast=0, collisions=0, rx_length_errors=0, rx_over_errors=0, rx_crc_errors=0, rx_frame_errors=0, rx_fifo_errors=0, rx_missed_errors=0, tx_aborted_errors=0, tx_carrier_errors=0, tx_fifo_errors=0, tx_heartbeat_errors=0, tx_window_errors=0, rx_compressed=0, tx_compressed=0, rx_nohandler=0}], [{nla_len=12, nla_type=IFLA_XDP}, [{nla_len=5, nla_type=IFLA_XDP_ATTACHED}, XDP_ATTACHED_NONE]], [{nla_len=64, nla_type=IFLA_LINKINFO}, [[{nla_len=8, nla_type=IFLA_INFO_KIND}, "tun"], [{nla_len=52, nla_type=IFLA_INFO_DATA}, [[{nla_len=5, nla_type=IFLA_TUN_TYPE}, IFF_TUN], [{nla_len=8, nla_type=IFLA_TUN_OWNER}, 1000], [{nla_len=5, nla_type=IFLA_TUN_PI}, 0], [{nla_len=5, nla_type=IFLA_TUN_VNET_HDR}, 0], [{nla_len=5, nla_type=IFLA_TUN_PERSIST}, 1], [{nla_len=5, nla_type=IFLA_TUN_MULTI_QUEUE}, 0]]]]], [{nla_len=428, nla_type=IFLA_AF_SPEC}, [[{nla_len=12, nla_type=AF_MCTP}, [{nla_len=8, nla_type=IFLA_MCTP_NET}, 1]], [{nla_len=140, nla_type=AF_INET}, [{nla_len=136, nla_type=IFLA_INET_CONF}, [[IPV4_DEVCONF_FORWARDING-1]=1, [IPV4_DEVCONF_MC_FORWARDING-1]=0, [IPV4_DEVCONF_PROXY_ARP-1]=0, [IPV4_DEVCONF_ACCEPT_REDIRECTS-1]=1, [IPV4_DEVCONF_SECURE_REDIRECTS-1]=1, [IPV4_DEVCONF_SEND_REDIRECTS-1]=1, [IPV4_DEVCONF_SHARED_MEDIA-1]=1, [IPV4_DEVCONF_RP_FILTER-1]=2, [IPV4_DEVCONF_ACCEPT_SOURCE_ROUTE-1]=1, [IPV4_DEVCONF_BOOTP_RELAY-1]=0, [IPV4_DEVCONF_LOG_MARTIANS-1]=0, [IPV4_DEVCONF_TAG-1]=0, [IPV4_DEVCONF_ARPFILTER-1]=0, [IPV4_DEVCONF_MEDIUM_ID-1]=0, [IPV4_DEVCONF_NOXFRM-1]=0, [IPV4_DEVCONF_NOPOLICY-1]=0, [IPV4_DEVCONF_FORCE_IGMP_VERSION-1]=0, [IPV4_DEVCONF_ARP_ANNOUNCE-1]=0, [IPV4_DEVCONF_ARP_IGNORE-1]=0, [IPV4_DEVCONF_PROMOTE_SECONDARIES-1]=0, [IPV4_DEVCONF_ARP_ACCEPT-1]=0, [IPV4_DEVCONF_ARP_NOTIFY-1]=0, [IPV4_DEVCONF_ACCEPT_LOCAL-1]=0, [IPV4_DEVCONF_SRC_VMARK-1]=0, [IPV4_DEVCONF_PROXY_ARP_PVLAN-1]=0, [IPV4_DEVCONF_ROUTE_LOCALNET-1]=0, [IPV4_DEVCONF_IGMPV2_UNSOLICITED_REPORT_INTERVAL-1]=10000, [IPV4_DEVCONF_IGMPV3_UNSOLICITED_REPORT_INTERVAL-1]=1000, [IPV4_DEVCONF_IGNORE_ROUTES_WITH_LINKDOWN-1]=0, [IPV4_DEVCONF_DROP_UNICAST_IN_L2_MULTICAST-1]=0, [IPV4_DEVCONF_DROP_GRATUITOUS_ARP-1]=0, [IPV4_DEVCONF_BC_FORWARDING-1]=0, ...]]], [{nla_len=272, nla_type=AF_INET6}, [[{nla_len=8, nla_type=IFLA_INET6_FLAGS}, 0], [{nla_len=20, nla_type=IFLA_INET6_CACHEINFO}, {max_reasm_len=65535, tstamp=327636, reachable_time=31749, retrans_time=1000}], [{nla_len=240, nla_type=IFLA_INET6_CONF}, [[DEVCONF_FORWARDING]=0, [DEVCONF_HOPLIMIT]=64, [DEVCONF_MTU6]=1500, [DEVCONF_ACCEPT_RA]=1, [DEVCONF_ACCEPT_REDIRECTS]=1, [DEVCONF_AUTOCONF]=1, [DEVCONF_DAD_TRANSMITS]=1, [DEVCONF_RTR_SOLICITS]=-1, [DEVCONF_RTR_SOLICIT_INTERVAL]=4000, [DEVCONF_RTR_SOLICIT_DELAY]=1000, [DEVCONF_USE_TEMPADDR]=-1, [DEVCONF_TEMP_VALID_LFT]=604800, [DEVCONF_TEMP_PREFERED_LFT]=86400, [DEVCONF_REGEN_MAX_RETRY]=3, [DEVCONF_MAX_DESYNC_FACTOR]=600, [DEVCONF_MAX_ADDRESSES]=16, [DEVCONF_FORCE_MLD_VERSION]=0, [DEVCONF_ACCEPT_RA_DEFRTR]=1, [DEVCONF_ACCEPT_RA_PINFO]=1, [DEVCONF_ACCEPT_RA_RTR_PREF]=1, [DEVCONF_RTR_PROBE_INTERVAL]=60000, [DEVCONF_ACCEPT_RA_RT_INFO_MAX_PLEN]=0, [DEVCONF_PROXY_NDP]=0, [DEVCONF_OPTIMISTIC_DAD]=0, [DEVCONF_ACCEPT_SOURCE_ROUTE]=0, [DEVCONF_MC_FORWARDING]=0, [DEVCONF_DISABLE_IPV6]=0, [DEVCONF_ACCEPT_DAD]=-1, [DEVCONF_FORCE_TLLAO]=0, [DEVCONF_NDISC_NOTIFY]=0, [DEVCONF_MLDV1_UNSOLICITED_REPORT_INTERVAL]=10000, [DEVCONF_MLDV2_UNSOLICITED_REPORT_INTERVAL]=1000, ...]]]]]], {nla_len=4, nla_type=NLA_F_NESTED|IFLA_DEVLINK_PORT}, ...]], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 1092
close(4)                                = 0
sendmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=40, nlmsg_type=RTM_NEWADDR, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK|NLM_F_EXCL|NLM_F_CREATE, nlmsg_seq=1727310555, nlmsg_pid=0}, {ifa_family=AF_INET, ifa_prefixlen=24, ifa_flags=0, ifa_scope=RT_SCOPE_UNIVERSE, ifa_index=if_nametoindex("tun144")}, [[{nla_len=8, nla_type=IFA_LOCAL}, inet_addr("169.254.144.1")], [{nla_len=8, nla_type=IFA_ADDRESS}, inet_addr("169.254.144.1")]]], iov_len=40}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 40
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=NULL, iov_len=0}], msg_iovlen=1, msg_controllen=0, msg_flags=MSG_TRUNC}, MSG_PEEK|MSG_TRUNC) = 36
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=36, nlmsg_type=NLMSG_ERROR, nlmsg_flags=NLM_F_CAPPED, nlmsg_seq=1727310555, nlmsg_pid=9194}, {error=0, msg={nlmsg_len=40, nlmsg_type=RTM_NEWADDR, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK|NLM_F_EXCL|NLM_F_CREATE, nlmsg_seq=1727310555, nlmsg_pid=0}}], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 36
```

ip link set dev tun144 up

```c
socket(AF_NETLINK, SOCK_RAW|SOCK_CLOEXEC, NETLINK_ROUTE) = 3
setsockopt(3, SOL_SOCKET, SO_SNDBUF, [32768], 4) = 0
setsockopt(3, SOL_SOCKET, SO_RCVBUF, [1048576], 4) = 0
setsockopt(3, SOL_NETLINK, NETLINK_EXT_ACK, [1], 4) = 0
bind(3, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 0
getsockname(3, {sa_family=AF_NETLINK, nl_pid=9495, nl_groups=00000000}, [12]) = 0
setsockopt(3, SOL_NETLINK, NETLINK_GET_STRICT_CHK, [1], 4) = 0
sendto(3, [{nlmsg_len=32, nlmsg_type=RTM_NEWLINK, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK, nlmsg_seq=0, nlmsg_pid=0}, {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_NETROM, ifi_index=0, ifi_flags=0, ifi_change=0}], 32, 0, NULL, 0) = 32
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=52, nlmsg_type=NLMSG_ERROR, nlmsg_flags=0, nlmsg_seq=0, nlmsg_pid=9495}, {error=-ENODEV, msg=[{nlmsg_len=32, nlmsg_type=RTM_NEWLINK, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK, nlmsg_seq=0, nlmsg_pid=0}, {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_NETROM, ifi_index=0, ifi_flags=0, ifi_change=0}]}], iov_len=16384}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 52
socket(AF_NETLINK, SOCK_RAW|SOCK_CLOEXEC, NETLINK_ROUTE) = 4
setsockopt(4, SOL_SOCKET, SO_SNDBUF, [32768], 4) = 0
setsockopt(4, SOL_SOCKET, SO_RCVBUF, [1048576], 4) = 0
setsockopt(4, SOL_NETLINK, NETLINK_EXT_ACK, [1], 4) = 0
bind(4, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 0
getsockname(4, {sa_family=AF_NETLINK, nl_pid=-2089760641, nl_groups=00000000}, [12]) = 0
sendmsg(4, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=52, nlmsg_type=RTM_GETLINK, nlmsg_flags=NLM_F_REQUEST, nlmsg_seq=1727311065, nlmsg_pid=0}, {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_NETROM, ifi_index=0, ifi_flags=0, ifi_change=0}, [[{nla_len=8, nla_type=IFLA_EXT_MASK}, RTEXT_FILTER_VF|RTEXT_FILTER_SKIP_STATS], [{nla_len=11, nla_type=IFLA_IFNAME}, "tun144"]]], iov_len=52}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 52
recvmsg(4, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=NULL, iov_len=0}], msg_iovlen=1, msg_controllen=0, msg_flags=MSG_TRUNC}, MSG_PEEK|MSG_TRUNC) = 1092
getrandom("\x13\xbe\x05\xf1\x89\x5a\x04\xbc", 8, GRND_NONBLOCK) = 8
brk(NULL)                               = 0x5fef639f9000
brk(0x5fef63a1a000)                     = 0x5fef63a1a000
recvmsg(4, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=1092, nlmsg_type=RTM_NEWLINK, nlmsg_flags=0, nlmsg_seq=1727311065, nlmsg_pid=-2089760641}, {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_NONE, ifi_index=if_nametoindex("tun144"), ifi_flags=IFF_POINTOPOINT|IFF_NOARP|IFF_MULTICAST, ifi_change=0}, [[{nla_len=11, nla_type=IFLA_IFNAME}, "tun144"], [{nla_len=8, nla_type=IFLA_TXQLEN}, 500], [{nla_len=5, nla_type=IFLA_OPERSTATE}, 2], [{nla_len=5, nla_type=IFLA_LINKMODE}, 0], [{nla_len=8, nla_type=IFLA_MTU}, 1500], [{nla_len=8, nla_type=IFLA_MIN_MTU}, 68], [{nla_len=8, nla_type=IFLA_MAX_MTU}, 65535], [{nla_len=8, nla_type=IFLA_GROUP}, 0], [{nla_len=8, nla_type=IFLA_PROMISCUITY}, 0], [{nla_len=8, nla_type=IFLA_ALLMULTI}, 0], [{nla_len=8, nla_type=IFLA_NUM_TX_QUEUES}, 1], [{nla_len=8, nla_type=IFLA_GSO_MAX_SEGS}, 65535], [{nla_len=8, nla_type=IFLA_GSO_MAX_SIZE}, 65536], [{nla_len=8, nla_type=IFLA_GRO_MAX_SIZE}, 65536], [{nla_len=8, nla_type=IFLA_GSO_IPV4_MAX_SIZE}, 65536], [{nla_len=8, nla_type=IFLA_GRO_IPV4_MAX_SIZE}, 65536], [{nla_len=8, nla_type=IFLA_TSO_MAX_SIZE}, 65536], [{nla_len=8, nla_type=IFLA_TSO_MAX_SEGS}, 65535], [{nla_len=8, nla_type=IFLA_NUM_RX_QUEUES}, 1], [{nla_len=5, nla_type=IFLA_CARRIER}, 0], [{nla_len=9, nla_type=IFLA_QDISC}, "noop"], [{nla_len=8, nla_type=IFLA_CARRIER_CHANGES}, 1], [{nla_len=8, nla_type=IFLA_CARRIER_UP_COUNT}, 0], [{nla_len=8, nla_type=IFLA_CARRIER_DOWN_COUNT}, 1], [{nla_len=5, nla_type=IFLA_PROTO_DOWN}, 0], [{nla_len=36, nla_type=IFLA_MAP}, {mem_start=0, mem_end=0, base_addr=0, irq=0, dma=0, port=0}], [{nla_len=204, nla_type=IFLA_STATS64}, {rx_packets=0, tx_packets=0, rx_bytes=0, tx_bytes=0, rx_errors=0, tx_errors=0, rx_dropped=0, tx_dropped=0, multicast=0, collisions=0, rx_length_errors=0, rx_over_errors=0, rx_crc_errors=0, rx_frame_errors=0, rx_fifo_errors=0, rx_missed_errors=0, tx_aborted_errors=0, tx_carrier_errors=0, tx_fifo_errors=0, tx_heartbeat_errors=0, tx_window_errors=0, rx_compressed=0, tx_compressed=0, rx_nohandler=0, rx_otherhost_dropped=0}], [{nla_len=100, nla_type=IFLA_STATS}, {rx_packets=0, tx_packets=0, rx_bytes=0, tx_bytes=0, rx_errors=0, tx_errors=0, rx_dropped=0, tx_dropped=0, multicast=0, collisions=0, rx_length_errors=0, rx_over_errors=0, rx_crc_errors=0, rx_frame_errors=0, rx_fifo_errors=0, rx_missed_errors=0, tx_aborted_errors=0, tx_carrier_errors=0, tx_fifo_errors=0, tx_heartbeat_errors=0, tx_window_errors=0, rx_compressed=0, tx_compressed=0, rx_nohandler=0}], [{nla_len=12, nla_type=IFLA_XDP}, [{nla_len=5, nla_type=IFLA_XDP_ATTACHED}, XDP_ATTACHED_NONE]], [{nla_len=64, nla_type=IFLA_LINKINFO}, [[{nla_len=8, nla_type=IFLA_INFO_KIND}, "tun"], [{nla_len=52, nla_type=IFLA_INFO_DATA}, [[{nla_len=5, nla_type=IFLA_TUN_TYPE}, IFF_TUN], [{nla_len=8, nla_type=IFLA_TUN_OWNER}, 1000], [{nla_len=5, nla_type=IFLA_TUN_PI}, 0], [{nla_len=5, nla_type=IFLA_TUN_VNET_HDR}, 0], [{nla_len=5, nla_type=IFLA_TUN_PERSIST}, 1], [{nla_len=5, nla_type=IFLA_TUN_MULTI_QUEUE}, 0]]]]], [{nla_len=428, nla_type=IFLA_AF_SPEC}, [[{nla_len=12, nla_type=AF_MCTP}, [{nla_len=8, nla_type=IFLA_MCTP_NET}, 1]], [{nla_len=140, nla_type=AF_INET}, [{nla_len=136, nla_type=IFLA_INET_CONF}, [[IPV4_DEVCONF_FORWARDING-1]=1, [IPV4_DEVCONF_MC_FORWARDING-1]=0, [IPV4_DEVCONF_PROXY_ARP-1]=0, [IPV4_DEVCONF_ACCEPT_REDIRECTS-1]=1, [IPV4_DEVCONF_SECURE_REDIRECTS-1]=1, [IPV4_DEVCONF_SEND_REDIRECTS-1]=1, [IPV4_DEVCONF_SHARED_MEDIA-1]=1, [IPV4_DEVCONF_RP_FILTER-1]=2, [IPV4_DEVCONF_ACCEPT_SOURCE_ROUTE-1]=1, [IPV4_DEVCONF_BOOTP_RELAY-1]=0, [IPV4_DEVCONF_LOG_MARTIANS-1]=0, [IPV4_DEVCONF_TAG-1]=0, [IPV4_DEVCONF_ARPFILTER-1]=0, [IPV4_DEVCONF_MEDIUM_ID-1]=0, [IPV4_DEVCONF_NOXFRM-1]=0, [IPV4_DEVCONF_NOPOLICY-1]=0, [IPV4_DEVCONF_FORCE_IGMP_VERSION-1]=0, [IPV4_DEVCONF_ARP_ANNOUNCE-1]=0, [IPV4_DEVCONF_ARP_IGNORE-1]=0, [IPV4_DEVCONF_PROMOTE_SECONDARIES-1]=0, [IPV4_DEVCONF_ARP_ACCEPT-1]=0, [IPV4_DEVCONF_ARP_NOTIFY-1]=0, [IPV4_DEVCONF_ACCEPT_LOCAL-1]=0, [IPV4_DEVCONF_SRC_VMARK-1]=0, [IPV4_DEVCONF_PROXY_ARP_PVLAN-1]=0, [IPV4_DEVCONF_ROUTE_LOCALNET-1]=0, [IPV4_DEVCONF_IGMPV2_UNSOLICITED_REPORT_INTERVAL-1]=10000, [IPV4_DEVCONF_IGMPV3_UNSOLICITED_REPORT_INTERVAL-1]=1000, [IPV4_DEVCONF_IGNORE_ROUTES_WITH_LINKDOWN-1]=0, [IPV4_DEVCONF_DROP_UNICAST_IN_L2_MULTICAST-1]=0, [IPV4_DEVCONF_DROP_GRATUITOUS_ARP-1]=0, [IPV4_DEVCONF_BC_FORWARDING-1]=0, ...]]], [{nla_len=272, nla_type=AF_INET6}, [[{nla_len=8, nla_type=IFLA_INET6_FLAGS}, 0], [{nla_len=20, nla_type=IFLA_INET6_CACHEINFO}, {max_reasm_len=65535, tstamp=327636, reachable_time=22092, retrans_time=1000}], [{nla_len=240, nla_type=IFLA_INET6_CONF}, [[DEVCONF_FORWARDING]=0, [DEVCONF_HOPLIMIT]=64, [DEVCONF_MTU6]=1500, [DEVCONF_ACCEPT_RA]=1, [DEVCONF_ACCEPT_REDIRECTS]=1, [DEVCONF_AUTOCONF]=1, [DEVCONF_DAD_TRANSMITS]=1, [DEVCONF_RTR_SOLICITS]=-1, [DEVCONF_RTR_SOLICIT_INTERVAL]=4000, [DEVCONF_RTR_SOLICIT_DELAY]=1000, [DEVCONF_USE_TEMPADDR]=-1, [DEVCONF_TEMP_VALID_LFT]=604800, [DEVCONF_TEMP_PREFERED_LFT]=86400, [DEVCONF_REGEN_MAX_RETRY]=3, [DEVCONF_MAX_DESYNC_FACTOR]=600, [DEVCONF_MAX_ADDRESSES]=16, [DEVCONF_FORCE_MLD_VERSION]=0, [DEVCONF_ACCEPT_RA_DEFRTR]=1, [DEVCONF_ACCEPT_RA_PINFO]=1, [DEVCONF_ACCEPT_RA_RTR_PREF]=1, [DEVCONF_RTR_PROBE_INTERVAL]=60000, [DEVCONF_ACCEPT_RA_RT_INFO_MAX_PLEN]=0, [DEVCONF_PROXY_NDP]=0, [DEVCONF_OPTIMISTIC_DAD]=0, [DEVCONF_ACCEPT_SOURCE_ROUTE]=0, [DEVCONF_MC_FORWARDING]=0, [DEVCONF_DISABLE_IPV6]=0, [DEVCONF_ACCEPT_DAD]=-1, [DEVCONF_FORCE_TLLAO]=0, [DEVCONF_NDISC_NOTIFY]=0, [DEVCONF_MLDV1_UNSOLICITED_REPORT_INTERVAL]=10000, [DEVCONF_MLDV2_UNSOLICITED_REPORT_INTERVAL]=1000, ...]]]]]], {nla_len=4, nla_type=NLA_F_NESTED|IFLA_DEVLINK_PORT}, ...]], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 1092
close(4)                                = 0
sendmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=32, nlmsg_type=RTM_NEWLINK, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK, nlmsg_seq=1727311065, nlmsg_pid=0}, {ifi_family=AF_UNSPEC, ifi_type=ARPHRD_NETROM, ifi_index=if_nametoindex("tun144"), ifi_flags=IFF_UP, ifi_change=0x1}], iov_len=32}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 32
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=NULL, iov_len=0}], msg_iovlen=1, msg_controllen=0, msg_flags=MSG_TRUNC}, MSG_PEEK|MSG_TRUNC) = 36
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=36, nlmsg_type=NLMSG_ERROR, nlmsg_flags=NLM_F_CAPPED, nlmsg_seq=1727311065, nlmsg_pid=9495}, {error=0, msg={nlmsg_len=32, nlmsg_type=RTM_NEWLINK, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK, nlmsg_seq=1727311065, nlmsg_pid=0}}], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 36
```

此时在路由表里添加了一项

```bash
zms# route
Kernel IP routing table
Destination     Gateway         Genmask         Flags Metric Ref    Use Iface
default         10.0.2.2        0.0.0.0         UG    100    0        0 enp0s3
10.0.2.0        0.0.0.0         255.255.255.0   U     100    0        0 enp0s3
169.254.144.0   0.0.0.0         255.255.255.0   U     0      0        0 tun144 // 从tun144发
```

```bash
iptables -t nat -A PREROUTING -s 169.254.144.0/24 -j CONNMARK --set-mark 144
iptables -t nat -A POSTROUTING -j MASQUERADE -m connmark --mark 144
```
这个运行后运行iptables -t nat -L -n -v会发现多了两项

```bash
Chain PREROUTING (policy ACCEPT 1 packets, 225 bytes)
 pkts bytes target     prot opt in     out     source               destination         
    0     0 CONNMARK   0    --  *      *       169.254.144.0/24     0.0.0.0/0            CONNMARK set 0x90

Chain INPUT (policy ACCEPT 0 packets, 0 bytes)
 pkts bytes target     prot opt in     out     source               destination         

Chain OUTPUT (policy ACCEPT 0 packets, 0 bytes)
 pkts bytes target     prot opt in     out     source               destination         

Chain POSTROUTING (policy ACCEPT 69 packets, 5440 bytes)
 pkts bytes target     prot opt in     out     source               destination         
    0     0 MASQUERADE  0    --  *      *       0.0.0.0/0            0.0.0.0/0            connmark match  0x90
```

```c
socket(AF_NETLINK, SOCK_RAW, NETLINK_NETFILTER) = 3
bind(3, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 0
getsockname(3, {sa_family=AF_NETLINK, nl_pid=9936, nl_groups=00000000}, [12]) = 0
newfstatat(AT_FDCWD, "/usr/lib/x86_64-linux-gnu/xtables/libipt_CONNMARK.so", 0x7ffcb01a8fc0, 0) = -1 ENOENT (No such file or directory)
newfstatat(AT_FDCWD, "/usr/lib/x86_64-linux-gnu/xtables/libxt_CONNMARK.so", {st_mode=S_IFREG|0644, st_size=19280, ...}, 0) = 0
openat(AT_FDCWD, "/usr/lib/x86_64-linux-gnu/xtables/libxt_CONNMARK.so", O_RDONLY|O_CLOEXEC) = 4
read(4, "\177ELF\2\1\1\0\0\0\0\0\0\0\0\0\3\0>\0\1\0\0\0\0\0\0\0\0\0\0\0"..., 832) = 832
fstat(4, {st_mode=S_IFREG|0644, st_size=19280, ...}) = 0
mmap(NULL, 17024, PROT_READ, MAP_PRIVATE|MAP_DENYWRITE, 4, 0) = 0x71e86f661000
mmap(0x71e86f662000, 4096, PROT_READ|PROT_EXEC, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 4, 0x1000) = 0x71e86f662000
mmap(0x71e86f663000, 4096, PROT_READ, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 4, 0x2000) = 0x71e86f663000
mmap(0x71e86f664000, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE, 4, 0x3000) = 0x71e86f664000
close(4)                                = 0
mprotect(0x71e86f664000, 4096, PROT_READ) = 0
socket(AF_NETLINK, SOCK_RAW, NETLINK_NETFILTER) = 4
bind(4, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 0
getsockname(4, {sa_family=AF_NETLINK, nl_pid=-107731339, nl_groups=00000000}, [12]) = 0
sendto(4, [{nlmsg_len=52, nlmsg_type=NFNL_SUBSYS_NFT_COMPAT<<8|NFNL_MSG_COMPAT_GET, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK, nlmsg_seq=1727312271, nlmsg_pid=0}, {nfgen_family=AF_INET, version=NFNETLINK_V0, res_id=htons(0)}, [[{nla_len=13, nla_type=0x1}, "\x43\x4f\x4e\x4e\x4d\x41\x52\x4b\x00"], [{nla_len=8, nla_type=0x2}, "\x00\x00\x00\x02"], [{nla_len=8, nla_type=0x3}, "\x00\x00\x00\x01"]]], 52, 0, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 52
recvmsg(4, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=52, nlmsg_type=NFNL_SUBSYS_NFT_COMPAT<<8|NFNL_MSG_COMPAT_GET, nlmsg_flags=NLM_F_MULTI, nlmsg_seq=1727312271, nlmsg_pid=-107731339}, {nfgen_family=AF_INET, version=NFNETLINK_V0, res_id=htons(0)}, [[{nla_len=13, nla_type=0x1}, "\x43\x4f\x4e\x4e\x4d\x41\x52\x4b\x00"], [{nla_len=8, nla_type=0x2}, "\x00\x00\x00\x02"], [{nla_len=8, nla_type=0x3}, "\x00\x00\x00\x01"]]], iov_len=16536}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 52
close(4)                                = 0
sendto(3, [{nlmsg_len=20, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_GETGEN, nlmsg_flags=NLM_F_REQUEST, nlmsg_seq=0, nlmsg_pid=0}, {nfgen_family=AF_UNSPEC, version=NFNETLINK_V0, res_id=htons(0)}], 20, 0, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 20
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=52, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_NEWGEN, nlmsg_flags=0, nlmsg_seq=0, nlmsg_pid=9936}, {nfgen_family=AF_UNSPEC, version=NFNETLINK_V0, res_id=htons(9)}, [[{nla_len=8, nla_type=0x1}, "\x00\x00\x00\x09"], [{nla_len=8, nla_type=0x2}, "\x00\x00\x26\xd0"], [{nla_len=13, nla_type=0x3}, "\x69\x70\x74\x61\x62\x6c\x65\x73\x00"]]], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 52
sendto(3, [{nlmsg_len=20, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_GETTABLE, nlmsg_flags=NLM_F_REQUEST|NLM_F_DUMP, nlmsg_seq=0, nlmsg_pid=0}, {nfgen_family=AF_INET, version=NFNETLINK_V0, res_id=htons(0)}], 20, 0, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 20
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=56, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_NEWTABLE, nlmsg_flags=NLM_F_MULTI, nlmsg_seq=0, nlmsg_pid=9936}, {nfgen_family=AF_INET, version=NFNETLINK_V0, res_id=htons(9)}, [[{nla_len=8, nla_type=0x1}, "\x6e\x61\x74\x00"], [{nla_len=8, nla_type=0x3}, "\x00\x00\x00\x02"], [{nla_len=12, nla_type=0x4}, "\x00\x00\x00\x00\x00\x00\x00\x02"], [{nla_len=8, nla_type=0x2}, "\x00\x00\x00\x00"]]], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 56
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=20, nlmsg_type=NLMSG_DONE, nlmsg_flags=NLM_F_MULTI, nlmsg_seq=0, nlmsg_pid=9936}, 0], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 20
sendto(3, [{nlmsg_len=40, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_GETCHAIN, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK, nlmsg_seq=0, nlmsg_pid=0}, {nfgen_family=AF_INET, version=NFNETLINK_V0, res_id=htons(0)}, [[{nla_len=8, nla_type=0x1}, "\x6e\x61\x74\x00"], [{nla_len=10, nla_type=0x3}, "\x49\x4e\x50\x55\x54\x00"]]], 40, 0, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 40
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=60, nlmsg_type=NLMSG_ERROR, nlmsg_flags=0, nlmsg_seq=0, nlmsg_pid=9936}, {error=-ENOENT, msg=[{nlmsg_len=40, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_GETCHAIN, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK, nlmsg_seq=0, nlmsg_pid=0}, {nfgen_family=AF_INET, version=NFNETLINK_V0, res_id=htons(0)}, [[{nla_len=8, nla_type=0x1}, "\x6e\x61\x74\x00"], [{nla_len=10, nla_type=0x3}, "\x49\x4e\x50\x55\x54\x00"]]]}], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 60
sendto(3, [{nlmsg_len=40, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_GETCHAIN, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK, nlmsg_seq=0, nlmsg_pid=0}, {nfgen_family=AF_INET, version=NFNETLINK_V0, res_id=htons(0)}, [[{nla_len=8, nla_type=0x1}, "\x6e\x61\x74\x00"], [{nla_len=11, nla_type=0x3}, "\x4f\x55\x54\x50\x55\x54\x00"]]], 40, 0, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 40
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=60, nlmsg_type=NLMSG_ERROR, nlmsg_flags=0, nlmsg_seq=0, nlmsg_pid=9936}, {error=-ENOENT, msg=[{nlmsg_len=40, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_GETCHAIN, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK, nlmsg_seq=0, nlmsg_pid=0}, {nfgen_family=AF_INET, version=NFNETLINK_V0, res_id=htons(0)}, [[{nla_len=8, nla_type=0x1}, "\x6e\x61\x74\x00"], [{nla_len=11, nla_type=0x3}, "\x4f\x55\x54\x50\x55\x54\x00"]]]}], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 60
sendto(3, [{nlmsg_len=44, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_GETCHAIN, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK, nlmsg_seq=0, nlmsg_pid=0}, {nfgen_family=AF_INET, version=NFNETLINK_V0, res_id=htons(0)}, [[{nla_len=8, nla_type=0x1}, "\x6e\x61\x74\x00"], [{nla_len=16, nla_type=0x3}, "\x50\x4f\x53\x54\x52\x4f\x55\x54\x49\x4e\x47\x00"]]], 44, 0, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 44
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=136, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_NEWCHAIN, nlmsg_flags=0, nlmsg_seq=0, nlmsg_pid=9936}, {nfgen_family=AF_INET, version=NFNETLINK_V0, res_id=htons(9)}, [[{nla_len=8, nla_type=0x1}, "\x6e\x61\x74\x00"], [{nla_len=16, nla_type=0x3}, "\x50\x4f\x53\x54\x52\x4f\x55\x54\x49\x4e\x47\x00"], [{nla_len=12, nla_type=0x2}, "\x00\x00\x00\x00\x00\x00\x00\x03"], [{nla_len=20, nla_type=0x4}, "\x08\x00\x01\x00\x00\x00\x00\x04\x08\x00\x02\x00\x00\x00\x00\x64"], [{nla_len=8, nla_type=0x5}, "\x00\x00\x00\x01"], [{nla_len=8, nla_type=0x7}, "\x6e\x61\x74\x00"], [{nla_len=28, nla_type=0x8}, "\x0c\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x10\x0c\x00\x01\x00\x00\x00\x00\x00\x00\x00\x05\x86"], [{nla_len=8, nla_type=0xa}, "\x00\x00\x00\x01"], [{nla_len=8, nla_type=0x6}, "\x00\x00\x00\x00"]]], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 136
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=36, nlmsg_type=NLMSG_ERROR, nlmsg_flags=NLM_F_CAPPED, nlmsg_seq=0, nlmsg_pid=9936}, {error=0, msg={nlmsg_len=44, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_GETCHAIN, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK, nlmsg_seq=0, nlmsg_pid=0}}], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 36
sendto(3, [{nlmsg_len=44, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_GETCHAIN, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK, nlmsg_seq=0, nlmsg_pid=0}, {nfgen_family=AF_INET, version=NFNETLINK_V0, res_id=htons(0)}, [[{nla_len=8, nla_type=0x1}, "\x6e\x61\x74\x00"], [{nla_len=15, nla_type=0x3}, "\x50\x52\x45\x52\x4f\x55\x54\x49\x4e\x47\x00"]]], 44, 0, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 44
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=136, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_NEWCHAIN, nlmsg_flags=0, nlmsg_seq=0, nlmsg_pid=9936}, {nfgen_family=AF_INET, version=NFNETLINK_V0, res_id=htons(9)}, [[{nla_len=8, nla_type=0x1}, "\x6e\x61\x74\x00"], [{nla_len=15, nla_type=0x3}, "\x50\x52\x45\x52\x4f\x55\x54\x49\x4e\x47\x00"], [{nla_len=12, nla_type=0x2}, "\x00\x00\x00\x00\x00\x00\x00\x01"], [{nla_len=20, nla_type=0x4}, "\x08\x00\x01\x00\x00\x00\x00\x00\x08\x00\x02\x00\xff\xff\xff\x9c"], [{nla_len=8, nla_type=0x5}, "\x00\x00\x00\x01"], [{nla_len=8, nla_type=0x7}, "\x6e\x61\x74\x00"], [{nla_len=28, nla_type=0x8}, "\x0c\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x01\x0c\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\xe1"], [{nla_len=8, nla_type=0xa}, "\x00\x00\x00\x01"], [{nla_len=8, nla_type=0x6}, "\x00\x00\x00\x00"]]], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 136
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=36, nlmsg_type=NLMSG_ERROR, nlmsg_flags=NLM_F_CAPPED, nlmsg_seq=0, nlmsg_pid=9936}, {error=0, msg={nlmsg_len=44, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_GETCHAIN, nlmsg_flags=NLM_F_REQUEST|NLM_F_ACK, nlmsg_seq=0, nlmsg_pid=0}}], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 36
sendto(3, [{nlmsg_len=20, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_GETGEN, nlmsg_flags=NLM_F_REQUEST, nlmsg_seq=0, nlmsg_pid=0}, {nfgen_family=AF_UNSPEC, version=NFNETLINK_V0, res_id=htons(0)}], 20, 0, {sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, 12) = 20
recvmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[{nlmsg_len=52, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_NEWGEN, nlmsg_flags=0, nlmsg_seq=0, nlmsg_pid=9936}, {nfgen_family=AF_UNSPEC, version=NFNETLINK_V0, res_id=htons(9)}, [[{nla_len=8, nla_type=0x1}, "\x00\x00\x00\x09"], [{nla_len=8, nla_type=0x2}, "\x00\x00\x26\xd0"], [{nla_len=13, nla_type=0x3}, "\x69\x70\x74\x61\x62\x6c\x65\x73\x00"]]], iov_len=32768}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 52
mmap(NULL, 2170880, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x71e86ee00000
setsockopt(3, SOL_SOCKET, SO_SNDBUFFORCE, [2097152], 4) = 0
setsockopt(3, SOL_SOCKET, SO_RCVBUFFORCE, [16384], 4) = 0
sendmsg(3, {msg_name={sa_family=AF_NETLINK, nl_pid=0, nl_groups=00000000}, msg_namelen=12, msg_iov=[{iov_base=[[{nlmsg_len=28, nlmsg_type=NFNL_MSG_BATCH_BEGIN, nlmsg_flags=NLM_F_REQUEST, nlmsg_seq=1, nlmsg_pid=0}, {nfgen_family=AF_UNSPEC, version=NFNETLINK_V0, res_id=htons(10)}, "\x08\x00\x01\x00\x00\x00\x00\x09"], [{nlmsg_len=272, nlmsg_type=NFNL_SUBSYS_NFTABLES<<8|NFT_MSG_NEWRULE, nlmsg_flags=NLM_F_REQUEST|NLM_F_CREATE|NLM_F_APPEND, nlmsg_seq=2, nlmsg_pid=0}, {nfgen_family=AF_INET, version=NFNETLINK_V0, res_id=htons(0)}, [[{nla_len=8, nla_type=0x1}, "\x6e\x61\x74\x00"], [{nla_len=15, nla_type=0x2}, "\x50\x52\x45\x52\x4f\x55\x54\x49\x4e\x47\x00"], [{nla_len=208, nla_type=NLA_F_NESTED|0x4}, "\x34\x00\x01\x80\x0c\x00\x01\x00\x70\x61\x79\x6c\x6f\x61\x64\x00\x24\x00\x02\x80\x08\x00\x01\x00\x00\x00\x00\x01\x08\x00\x02\x00"...], [{nla_len=20, nla_type=NLA_F_NESTED|0x5}, "\x08\x00\x01\x00\x00\x00\x00\x00\x08\x00\x02\x00\x00\x00\x00\x00"]]], [{nlmsg_len=20, nlmsg_type=NFNL_MSG_BATCH_END, nlmsg_flags=NLM_F_REQUEST, nlmsg_seq=3, nlmsg_pid=0}, {nfgen_family=AF_UNSPEC, version=NFNETLINK_V0, res_id=htons(10)}]], iov_len=320}], msg_iovlen=1, msg_controllen=0, msg_flags=0}, 0) = 320
pselect6(4, [3], NULL, NULL, {tv_sec=0, tv_nsec=0}, NULL) = 0 (Timeout)
```