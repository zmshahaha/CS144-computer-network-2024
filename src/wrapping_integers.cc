#include "wrapping_integers.hh"

using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + static_cast<uint32_t>(n & UINT32_MAX);
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint64_t warp_around = 1UL << 32;
  uint64_t max_diff = warp_around / 2;
  uint64_t seqno = static_cast<uint64_t>(raw_value_);
  uint64_t zero_u64 = static_cast<uint64_t>(zero_point.raw_value_);
  uint64_t asn;

  if (seqno < zero_u64) {
    seqno += warp_around;
  }

  asn = seqno - zero_u64;

  if (checkpoint > asn) {
    asn += ((checkpoint - asn) / warp_around) * warp_around;

    if (checkpoint - asn > max_diff) {
      asn += warp_around;
    }
  }

  return asn;
}
