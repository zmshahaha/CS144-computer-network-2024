#include "byte_stream.hh"

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

bool Writer::is_closed() const
{
  return closed_;
}

void Writer::push( string data )
{
  uint64_t max_push = available_capacity();

  if (data.size() > max_push) {
    data.resize(max_push);
  }

  for (char c : data) {
    buffer_.push_back(c);
  }

  size_pushed_ += data.size();
  return;
}

void Writer::close()
{
  closed_ = true;
}

uint64_t Writer::available_capacity() const
{
  return (capacity_ - buffer_.size());
}

uint64_t Writer::bytes_pushed() const
{
  return size_pushed_;
}

bool Reader::is_finished() const
{
  return closed_ && (buffer_.size() == 0);
}

uint64_t Reader::bytes_popped() const
{
  return size_pushed_ - buffer_.size();
}

string_view Reader::peek() const
{
  return buffer_;
}

void Reader::pop( uint64_t len )
{
  if (len > bytes_buffered()) {
    len = bytes_buffered();
  }

  buffer_.erase(0, len);
}

uint64_t Reader::bytes_buffered() const
{
  return buffer_.size();
}
