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
