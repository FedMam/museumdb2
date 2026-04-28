#pragma once

#include "rocksdb/rocksdb_namespace.h"
#include "hilbert/geometry.h"

#include <string>

namespace ROCKSDB_NAMESPACE {

#pragma pack(push, 1)
// Hilbert value is represented as uint128
struct HilbertCode {
 public:
  HilbertCode() : upper_(0), lower_(0) { }

  HilbertCode(uint64_t upper_64_bits, uint64_t lower_64_bits)
    : upper_(upper_64_bits), lower_(lower_64_bits) { }

  inline uint64_t GetUpper64Bits() const { return upper_; }
  inline uint64_t GetLower64Bits() const { return lower_; }

  inline bool operator<(const HilbertCode& other) const {
    return (upper_ < other.upper_) || (upper_ == other.upper_ && lower_ < other.lower_);
  }

  inline bool operator>(const HilbertCode& other) const {
    return (upper_ > other.upper_) || (upper_ == other.upper_ && lower_ > other.lower_);
  }

  inline bool operator<=(const HilbertCode& other) const {
    return !(*this > other);
  }

  inline bool operator>=(const HilbertCode& other) const {
    return !(*this < other);
  }

  inline bool operator==(const HilbertCode& other) const {
    return upper_ == other.upper_ && lower_ == other.lower_;
  }

  inline bool operator!=(const HilbertCode& other) const {
    return upper_ != other.upper_ || lower_ != other.lower_;
  }

  inline bool IsZero() const {
    return upper_ == 0 && lower_ == 0;
  }

  // for computation

  inline bool IsBitSetAt(size_t bit) const {
    if (bit < 64)
      return (lower_ & (1 << bit)) != 0;
    else if (bit < 128)
      return (upper_ & (1 << (bit - 64))) != 0;
    return 0;
  }

  inline void SetBitAt(size_t bit) {
    if (bit < 64)
      lower_ |= (1 << bit);
    else if (bit < 128)
      upper_ |= (1 << (bit - 64));
  }

  inline void UnsetBitAt(size_t bit) {
    if (bit < 64)
      lower_ &= ~((uint64_t)1 << bit);
    else if (bit < 128)
      upper_ &= ~((uint64_t)1 << (bit - 64));
  }

  // Returns the code's string representation in big-endian so it gives the
  // same compare result. Because of this, always use this instead of
  // reinterpret_cast<const char*>(&hilbert_code)!
  std::string ToString() const;

  // Returns true if the string's length is exactly sizeof(uint64_t) * 2
  static inline bool IsValidHilbertCodeString(const std::string& str) {
    return str.size() == sizeof(uint64_t) * 2;
  }

  // The string should be a 16-byte string in big endian format
  // Warning: will throw an error if IsValidHilbertCodeString() returns false on str
  static HilbertCode FromString(const std::string& str);

 private:
  uint64_t upper_;
  uint64_t lower_;
};
#pragma pack(pop)

// This function computes the distance of the point on a Hilbert curve
// built in the [0, 2^N)x[0, 2^N) square, where N is the length
// in bits of the two coordinates of the point.
HilbertCode PointToHilbertCode(const UInt64Point& point);

// This function accepts the distance of a point on a Hilbert curve and
// returns the coordinates of that point. The Hilbert curve is assumed
// to have been built in the [0, 2^64)x[0, 2^64) square.
UInt64Point HilbertCodeToPoint(const HilbertCode& hilbertCode);

// A wrapper of VarLenPoint2D that automatically computes the Hilbert value
// for that point.
struct PointWithHilbertCode {
  explicit PointWithHilbertCode(const UInt64Point& point)
    : point_(point),
      hilbert_code_(PointToHilbertCode(point)) { }
  
  explicit PointWithHilbertCode(const HilbertCode& hilbertCode)
    : point_(HilbertCodeToPoint(hilbertCode)),
      hilbert_code_(hilbertCode) { }

  inline const UInt64Point& GetPoint() const { return point_; }

  inline const HilbertCode& GetHilbertCode() const { return hilbert_code_; }

 private:
  UInt64Point point_;
  HilbertCode hilbert_code_;
};

}
