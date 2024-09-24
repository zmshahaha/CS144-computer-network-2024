#include "reassembler.hh"

using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring )
{
  // I'm writer
  Writer &writer = output_.writer();
  uint64_t available_cap = writer.available_capacity();
  uint64_t index;
  string str = {};
  bool need_insert = true;

  // data size may vary later
  if (is_last_substring) {
    stream_size = first_index + data.length();
  }

  // out of window
  if ((first_index >= writer.bytes_pushed() + available_cap) ||
      (first_index + data.length() <= writer.bytes_pushed())) {
    goto push_check;
  }

  // fit into window, cut left side
  if (first_index < writer.bytes_pushed()) {
    data.erase(0, writer.bytes_pushed() - first_index);
    first_index = writer.bytes_pushed();
  }

  // fit into window, cut right side
  if (first_index + data.length() > writer.bytes_pushed() + available_cap) {
    data.erase(writer.bytes_pushed() + available_cap - first_index,
              first_index + data.length() - writer.bytes_pushed() - available_cap);
  }

  // insert to buffer
  for (auto iter = buffer_.begin(); iter != buffer_.end(); ) {
    index = iter->first;
    str = iter->second;

    if ((index <= first_index) && (index + str.length() >= first_index + data.length())) {
      need_insert = false;
      break;
    }

    // whether this iter's string slice can expand data's left
    if ((index < first_index) && (index + str.length() >= first_index)) {
      data = str.substr(0, first_index - index) + data;
      first_index = index;
      iter = buffer_.erase(iter);
      continue; // save time
    }

    // data fully contain iter
    if ((index >= first_index) && (index + str.length() <= first_index + data.length())) {
      iter = buffer_.erase(iter);
      continue;
    }

    // whether this iter's string slice can expand data's right
    if ((index <= first_index + data.length()) && (index + str.length() > first_index + data.length())) {
      data =  data + str.substr(first_index + data.length() - index,
                                index + str.length() - first_index - data.length());
      iter = buffer_.erase(iter);
      break; // save time
    }

    // save time
    if (index > first_index + data.length()) {
      break;
    }

    ++iter;
  }

  if (need_insert) {
    buffer_.insert({first_index, data});
  }

push_check:
  // push to bytestream
  if (!buffer_.empty() && (buffer_.begin()->first == writer.bytes_pushed()) && available_cap) {
    auto to_push = buffer_.begin();
    index = to_push->first;
    str = to_push->second;
    buffer_.erase(to_push);
    if (available_cap >= str.length()) {
      writer.push(str);
    } else {
      string new_str = str.substr(available_cap, str.length() - available_cap);
      str = str.erase(available_cap, str.length() - available_cap);
      writer.push(str);
      buffer_.insert({index + available_cap, new_str});
    }
  }

  if (buffer_.empty() && (writer.bytes_pushed() == stream_size)) {
    writer.close();
  }
}

uint64_t Reassembler::bytes_pending() const
{
  uint64_t ret = 0;

  for (auto &[k, v] : buffer_) {
    ret += v.length();
  }
  return ret;
}
