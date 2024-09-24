#include "tcp_receiver.hh"

using namespace std;

void TCPReceiver::receive( TCPSenderMessage message )
{
  uint64_t current_asn;

  if (message.RST) {
    reader().set_error();
    return;
  }

  if (message.SYN) {
    isn_ = Wrap32(message.seqno);
    syn_recved_ = true;
  }

  if (!syn_recved_) {
    reader().set_error();
    return;
  }

  current_asn = message.seqno.unwrap(isn_, last_asn_);

  if (message.FIN) {
    fin_recved_ = true;
    stream_len_ = current_asn - 1 + message.SYN + message.payload.length();
  }

  // reassembler's index is start from 0, and asn except SYN start from 1, so minus 1
  reassembler_.insert(current_asn + message.SYN - 1, message.payload, message.FIN);

  last_asn_ = writer().bytes_pushed(); // SYN is 0, so pushed bytes is last asn
}

TCPReceiverMessage TCPReceiver::send() const
{
  TCPReceiverMessage recv_msg = {};
  uint64_t next_asn = last_asn_ + 1 + (fin_recved_ && (writer().bytes_pushed() == stream_len_));

  if (syn_recved_) {
    recv_msg.ackno.emplace(Wrap32{0}.wrap(next_asn, isn_));
  } else {
    recv_msg.ackno = nullopt;
  }

  if (reader().has_error()) {
    recv_msg.RST = true;
  } else {
    recv_msg.RST = false;
  }

  recv_msg.window_size = writer().available_capacity() > UINT16_MAX ? UINT16_MAX : writer().available_capacity();

  return recv_msg;
}
