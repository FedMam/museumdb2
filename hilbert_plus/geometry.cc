#include "hilbert/geometry.h"

#include <assert.h>

#include <sstream>
#include <iomanip>

namespace ROCKSDB_NAMESPACE {

VarLenNumber::VarLenNumber(size_t len, const std::string& repr)
    : len_(len) {
  if (repr.length() < len)
    repr_ = repr + std::string(len - repr.length(), '\0');
  else if (repr.length() > len)
    repr_ = repr.substr(0, len);
  else repr_ = repr;
}

VarLenNumber::VarLenNumber(size_t len, const char* repr)
    : VarLenNumber(len, std::string(repr)) { }

VarLenNumber::VarLenNumber(size_t len, uint64_t value)
    : len_(len) {
  repr_ = "";
  for (size_t i = 0; i < std::min(len, sizeof(uint64_t)); ++i) {
    repr_.push_back(value % (1 << 8));
    value >>= 8;
  }
  if (repr_.length() < len)
    repr_ += std::string(len - repr_.length(), '\0');
}

VarLenNumber::VarLenNumber(size_t len, uint32_t value)
    : len_(len) {
  repr_ = "";
  for (size_t i = 0; i < std::min(len, sizeof(uint32_t)); ++i) {
    repr_.push_back(value % (1 << 8));
    value >>= 8;
  }
  if (repr_.length() < len)
    repr_ += std::string(len - repr_.length(), '\0');
}

VarLenNumber::VarLenNumber(size_t len)
    : len_(len), repr_(len, '\0') { }

std::string VarLenNumber::NumericalStringRepr() const {
  auto digits = "0123456789abcdef";

  std::ostringstream result;
  for (size_t i = 0; i < len_; ++i) {
    result << digits[static_cast<uint8_t>(repr_[len_ - 1 - i]) >> 4];
    result << digits[static_cast<uint8_t>(repr_[len_ - 1 - i]) % (1 << 4)];
  }
  return result.str();
}

uint32_t VarLenNumber::NumericalValue32() const {
  uint32_t result = 0;

  for (size_t i = 0; i < sizeof(uint32_t); ++i) {
    result |= static_cast<uint32_t>(GetByteAt(i)) << (8 * i);
  }

  return result;
}

uint64_t VarLenNumber::NumericalValue64() const {
  uint64_t result = 0;

  for (size_t i = 0; i < sizeof(uint64_t); ++i) {
    result |= static_cast<uint64_t>(GetByteAt(i)) << (8 * i);
  }

  return result;
}

int VarLenNumber::Compare(const VarLenNumber& other) const {
  for (size_t i = std::max(len_, other.len_); i > 0; --i) {
    uint8_t myByte = static_cast<uint8_t>(GetByteAt(i-1));
    uint8_t otherByte = static_cast<uint8_t>(other.GetByteAt(i-1));
    if (myByte > otherByte)
      return 1;
    else if (myByte < otherByte)
      return -1;
  }
  return 0;
}

VarLenNumber VarLenNumber::Add(const VarLenNumber& other, bool do_not_expand) const {
  std::string result_repr = "";
  uint8_t leftover = 0;
  size_t max_length = std::max(len_, other.len_);

  for (size_t i = 0; i < max_length || (leftover != 0 && !do_not_expand); ++i) {
    uint8_t myByte = static_cast<uint8_t>(GetByteAt(i));
    uint8_t otherByte = static_cast<uint8_t>(other.GetByteAt(i));
    uint16_t result = static_cast<uint16_t>(myByte) + otherByte + leftover;
    uint8_t resultByte;

    leftover = (result >> 8);
    resultByte = static_cast<uint8_t>(result % (1 << 8));

    result_repr.push_back(static_cast<char>(resultByte));
  }
  return VarLenNumber(result_repr.length(), result_repr);
}

VarLenNumber VarLenNumber::And(const VarLenNumber& other) const {
  std::string result_repr = "";
  size_t max_length = std::max(len_, other.len_);
  for (size_t i = 0; i < max_length; ++i) {
    result_repr.push_back(static_cast<char>(GetByteAt(i) & other.GetByteAt(i)));
  }
  return VarLenNumber(max_length, result_repr);
}

VarLenNumber VarLenNumber::Or(const VarLenNumber& other) const {
  std::string result_repr = "";
  size_t max_length = std::max(len_, other.len_);
  for (size_t i = 0; i < max_length; ++i) {
    result_repr.push_back(static_cast<char>(GetByteAt(i) | other.GetByteAt(i)));
  }
  return VarLenNumber(max_length, result_repr);
}

VarLenNumber VarLenNumber::Xor(const VarLenNumber& other) const {
  std::string result_repr = "";
  size_t max_length = std::max(len_, other.len_);
  for (size_t i = 0; i < max_length; ++i) {
    result_repr.push_back(static_cast<char>(GetByteAt(i) ^ other.GetByteAt(i)));
  }
  return VarLenNumber(max_length, result_repr);
}

void VarLenNumber::AndAssign(const VarLenNumber& other) {
  assert(other.len_ <= len_);
  for (size_t i = 0; i < other.len_; ++i) {
    repr_[i] &= other.repr_[i];
  }
}

void VarLenNumber::OrAssign(const VarLenNumber& other) {
  assert(other.len_ <= len_);
  for (size_t i = 0; i < other.len_; ++i) {
    repr_[i] |= other.repr_[i];
  }
}

void VarLenNumber::XorAssign(const VarLenNumber& other) {
  assert(other.len_ <= len_);
  for (size_t i = 0; i < other.len_; ++i) {
    repr_[i] ^= other.repr_[i];
  }
}

VarLenNumber VarLenNumber::RShift(size_t bits) const {
  if (bits >= len_ * 8)
    return VarLenNumber(len_, std::string(len_, '\0'));  // zero
  
  if (bits % 8 == 0) {
    return VarLenNumber(len_, repr_.substr(bits / 8, len_));
  }

  size_t full_bytes = bits / 8;
  size_t rem_bits = bits % 8;
  std::string result_repr = "";
  for (size_t i = full_bytes; i < len_; ++i) {
    result_repr.push_back(static_cast<char>(GetByteAt(i) >> rem_bits) + static_cast<char>(GetByteAt(i+1) << (8 - rem_bits)));
  }
  return VarLenNumber(len_, result_repr);
}

void VarLenNumber::InvertNLowestBits(size_t bits) {
  assert(bits <= len_ * 8);
  size_t i = 0;
  while (bits != 0) {
    if (bits >= 8) {
      repr_[i] = static_cast<char>(~repr_[i]);
      ++i;
      bits -= 8;
    } else {
      repr_[i] = static_cast<char>(repr_[i] ^ ((1 << bits) - 1));
      bits = 0;
    }
  }
}

VarLenPoint2D::VarLenPoint2D(const VarLenNumber& x, const VarLenNumber& y) {
  size_t max_length = std::max(x.GetLength(), y.GetLength());
  x_ = x.CropOrResize(max_length);
  y_ = y.CropOrResize(max_length);
}

VarLenRectangle::VarLenRectangle(const VarLenNumber& left,
                                 const VarLenNumber& top,
                                 const VarLenNumber& right,
                                 const VarLenNumber& bottom) {
  size_t max_length = std::max(std::max(std::max(left.GetLength(), top.GetLength()), right.GetLength()), bottom.GetLength());
  left_ = left.CropOrResize(max_length);
  top_ = top.CropOrResize(max_length);
  right_ = right.CropOrResize(max_length);
  bottom_ = bottom.CropOrResize(max_length);
}

}