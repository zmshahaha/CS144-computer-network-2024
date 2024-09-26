#include "tcp_sender.hh"
#include "tcp_config.hh"
#include <algorithm>

using namespace std;

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return next_send_asn_ - next_ack_asn_;
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retx_;
}

// it won't retrans in-flight data
void TCPSender::push( const TransmitFunction& transmit )
{
  uint64_t send_size = 0, in_flight = sequence_numbers_in_flight();
  uint64_t max_send, payload_size;
  TCPSenderMessage msg {};

  // according to testcase, when start, SYN/FIN is only info can be sent
  if (next_send_asn_ == 0) {
    msg.SYN = true;
    msg.seqno = Wrap32(0).wrap(next_send_asn_, isn_);
    msg.FIN = end_ = reader().is_finished();
    in_flight_msg_.push(msg);
    transmit(msg);
    next_send_asn_ += msg.sequence_length();
    return;
  }

  // tx end
  if (end_) {
    return;
  }

  // according testcase, when SYN is not sent, send the syn
  if ((!reader().bytes_buffered()) && (next_ack_asn_ != 0) && (!reader().is_finished())) {
    return;
  }

  if (window_size_ == 0) {
    max_send = in_flight ? 0 : 1;
  } else if (window_size_ > in_flight){
    max_send = window_size_ - in_flight;
  } else {   // error
    max_send = 0;
  }

  while ((send_size < max_send) &&
         (input_.reader().bytes_buffered() ||   // need send data
          (reader().is_finished() && !end_))) { // need send FIN
    // payload
    payload_size = min({TCPConfig::MAX_PAYLOAD_SIZE,
                        max_send - send_size,
                        input_.reader().bytes_buffered()});
    msg.payload = input_.reader().peek().substr(0, payload_size);
    send_size += payload_size;
    input_.reader().pop(payload_size); // it will update is_finished, must before setting FIN

    msg.seqno = Wrap32(0).wrap(next_send_asn_, isn_);

    msg.FIN = end_ = (reader().is_finished() && (max_send - send_size));
    send_size += msg.FIN;

    msg.RST = input_.has_error();

    in_flight_msg_.push(msg);
    transmit(msg);
    next_send_asn_ += msg.sequence_length();
  }
}

TCPSenderMessage TCPSender::make_empty_message() const
{
  TCPSenderMessage msg = {};

  msg.seqno = Wrap32(0).wrap(next_send_asn_, isn_);
  msg.RST = input_.has_error();

  return msg;
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  uint64_t ack_asn;
  TCPSenderMessage tx_msg;

  window_size_ = msg.window_size;

  if (msg.RST) {
    input_.set_error();
    return;
  }

  if (!msg.ackno.has_value()) {
    // testcase doesn't want set connection error
    return;
  }

  ack_asn = msg.ackno.value().unwrap(isn_, next_ack_asn_);

  if (ack_asn > next_send_asn_) {
    // testcase doesn't want set connection error
    return;
  }

  if (ack_asn > next_ack_asn_) {
    next_ack_asn_ = ack_asn;
    // according to testcase, timer restart when new ack come
    last_retx_ms_ = 0;
  }

  while (in_flight_msg_.size() > 0) {
    tx_msg = in_flight_msg_.front();
    if (tx_msg.seqno.unwrap(isn_, next_ack_asn_) + tx_msg.sequence_length() <= next_ack_asn_) {
      in_flight_msg_.pop();
    } else {
      break;
    }
  }

  if (sequence_numbers_in_flight() == 0) {
    if (in_flight_msg_.size() != 0) {
      throw runtime_error("seq_in_flight and in_flight_msg are inconsistent");
    }
    last_retx_ms_ = 0;
  }
  consecutive_retx_ = 0;
}

void TCPSender::tick( uint64_t ms_since_last_tick, const TransmitFunction& transmit )
{
  if (sequence_numbers_in_flight() != 0) {
    last_retx_ms_ += ms_since_last_tick;
  } else {
    last_retx_ms_ = 0;
  }

  if (last_retx_ms_ >= initial_RTO_ms_ << consecutive_retx_) {
    last_retx_ms_ = 0;
    if (in_flight_msg_.size() > 0) {
      // according to testcase, when no window_size set, do backoff.
      if ((window_size_ != 0) || (next_ack_asn_ == 0)) {
        consecutive_retx_ ++;
      }
      transmit(in_flight_msg_.front());
    }
  }
}
