#pragma once

#include "rocksdb/rocksdb_namespace.h"

#include <stdint.h>

#include <string>

namespace ROCKSDB_NAMESPACE {

// This structure stores a number of arbitrary length in bytes.
// The number value is stored as a string, whose characters are the
// bytes of the number. The bytes are in Little-Endian (the first character
// of the string is the least significant byte).
struct VarLenNumber {
 public:
  VarLenNumber(size_t len, const std::string& repr);

  VarLenNumber(size_t len, const char* repr);

  VarLenNumber(size_t len, uint64_t value);

  VarLenNumber(size_t len, uint32_t value);

  // Returns zero of length len in bytes
  explicit VarLenNumber(size_t len = 0);

  // Does not copy the string representation, returns reference instead
  inline const std::string& GetReprLittleEndian() const {
    return repr_;
  }

  // Copies the string representation
  inline std::string GetReprBigEndian() const {
    return std::string(repr_.rbegin(), repr_.rend());
  }

  inline size_t GetLength() const {
    return len_;
  }

  // Returns the index-st byte of the number representation,
  // or 0x00 if index is greater than or equal to the length of the number.
  inline uint8_t GetByteAt(size_t index) const {
    if (index >= len_)
      return 0x00;
    return static_cast<uint8_t>(repr_[index]);
  }

  // Returns the index-st byte of the number representation,
  // or 0x00 if index is greater than or equal to the length of the number.
  inline uint8_t operator[](size_t index) const {
    return GetByteAt(index);
  }

  // If len > length of this number, take only len lowest bytes from the number.
  // If len > length of this number, return a number of length len with zero extra bytes.
  inline VarLenNumber CropOrResize(size_t len) const {
    if (len == len_) return *this;
    return VarLenNumber(len, repr_);
  }

  // Returns the numeric representation of this
  // number in hexadecimal big-endian format.
  std::string NumericalStringRepr() const;
  // Tries to return the exact 32-bit numerical value of this number.
  // WARNING: throws an error if length of this number is > 4 bytes.
  uint32_t NumericalValue32() const;
  // Tries to return the exact 64-bit numerical value of this number.
  // WARNING: throws an error if length of this number is > 8 bytes.
  uint64_t NumericalValue64() const;

  // Returns a positive integer if the value of this instance is greater than that of `other`,
  // a negative integer if it is less than `other`'s, and 0 if they are equal.
  int Compare(const VarLenNumber& other) const;

  inline bool operator<(const VarLenNumber& other) const {
    return Compare(other) < 0;
  }

  inline bool operator<=(const VarLenNumber& other) const {
    return Compare(other) <= 0;
  }

  inline bool operator>(const VarLenNumber& other) const {
    return Compare(other) > 0;
  }

  inline bool operator>=(const VarLenNumber& other) const {
    return Compare(other) >= 0;
  }

  inline bool operator==(const VarLenNumber& other) const {
    return Compare(other) == 0;
  }

  inline bool operator!=(const VarLenNumber& other) const {
    return Compare(other) != 0;
  }

  // Adds two numbers together. If do_not_expand is true and the result does
  // not fit into max(len of number 1, len of number 2) bytes, then drop the extra byte.
  VarLenNumber Add(const VarLenNumber& other, bool do_not_expand = false) const;

  inline VarLenNumber operator+(const VarLenNumber &other) const {
    return Add(other);
  }
  
  VarLenNumber And(const VarLenNumber& other) const;
  VarLenNumber Or(const VarLenNumber& other) const;
  VarLenNumber Xor(const VarLenNumber& other) const;
  
  inline VarLenNumber operator&(const VarLenNumber& other) const {
    return And(other);
  }

  inline VarLenNumber operator|(const VarLenNumber& other) const {
    return Or(other);
  }

  inline VarLenNumber operator^(const VarLenNumber& other) const {
    return Xor(other);
  }

  // Modifies this instance. Other's length should be at least this number's length. If it's strictly less, then this operation is faster than And().
  void AndAssign(const VarLenNumber& other);
  // Modifies this instance. Other's length should be at least this number's length. If it's strictly less, then this operation is faster than Or().
  void OrAssign(const VarLenNumber& other);
  // Modifies this instance. Other's length should be at least this number's length. If it's strictly less, then this operation is faster than Xor().
  void XorAssign(const VarLenNumber& other);

  inline VarLenNumber& operator&=(const VarLenNumber& other) {
    AndAssign(other);
    return *this;
  }

  inline VarLenNumber& operator|=(const VarLenNumber& other) {
    OrAssign(other);
    return *this;
  }

  inline VarLenNumber& operator^=(const VarLenNumber& other) {
    XorAssign(other);
    return *this;
  }

  // Keeps the byte length of the number
  VarLenNumber RShift(size_t bits) const;

  // Keeps the byte length of the number
  inline VarLenNumber operator>>(size_t bits) const {
    return RShift(bits);
  }

  inline bool IsBitSetAt(size_t bit) const {
    if (bit >= len_ * 8) return false;
    return (repr_[bit / 8] & (1 << (bit % 8))) != 0;
  }

  // WARNING: if bit is out of range, this is undefined behavior
  inline void SetBitAt(size_t bit) {
    repr_[bit / 8] |= static_cast<char>(1 << (bit % 8));
  }

  // WARNING: if bit is out of range, this is undefined behavior
  inline void UnsetBitAt(size_t bit) {
    repr_[bit / 8] &= static_cast<char>(~(1 << (bit % 8)));
  }

  // WARNING: if bit is out of range, this is undefined behavior
  inline void SetBitValueAt(size_t bit, bool set) {
    set ? SetBitAt(bit) : UnsetBitAt(bit);
  }

  // WARNING: if bit is out of range, this is undefined behavior
  inline void InvertBitAt(size_t bit) {
    repr_[bit / 8] ^= static_cast<char>(1 << (bit % 8));
  }

  // This is needed for Hilbert value computation
  // Throws an error if bits > length of this number in bits
  void InvertNLowestBits(size_t bits);

  // Equivalent to And(AllPositive(bits)) but also shrinks this number's length to ceil(bits)
  inline VarLenNumber CropBits(size_t bits) const {
    if (bits % 8 == 0)
      return CropOrResize(bits / 8);

    if (bits >= len_ * 8)
      return CropOrResize(bits / 8 + 1);

    return VarLenNumber(bits / 8 + 1, repr_.substr(0, bits / 8) + static_cast<char>(GetByteAt(bits / 8) & ((1 << (bits % 8)) - 1)));
  }

  // Returns a number of 'bits' positive bits (2^bits - 1)
  static inline VarLenNumber AllPositive(size_t bits) {
    if (bits % 8 == 0)
      return VarLenNumber(bits / 8, std::string(bits / 8, static_cast<char>(0xff)));
    
    return VarLenNumber(bits / 8 + 1, std::string(bits / 8, static_cast<char>(0xff)) + static_cast<char>((1 << (bits % 8)) - 1));
  }

 private:
  size_t len_;
  std::string repr_;
};

// This structure represents a 2D point with VarLenNumber's as coordinates.
struct VarLenPoint2D {
 public:
  // Will construct a (0, 0) point
  VarLenPoint2D()
    : VarLenPoint2D(VarLenNumber(), VarLenNumber()) { }

  // Will automatically adjust x and y to the same length
  VarLenPoint2D(const VarLenNumber& x, const VarLenNumber& y);

  // Will automatically adjust x and y to number_length
  // (be careful of possible data loss if number_length is smaller than
  // any coordinate's length)
  VarLenPoint2D(size_t number_length, const VarLenNumber& x, const VarLenNumber& y)
    : VarLenPoint2D(x.CropOrResize(number_length), y.CropOrResize(number_length)) { }
  
  inline const VarLenNumber& GetX() const { return x_; }
  inline const VarLenNumber& GetY() const { return y_; }

  inline bool operator==(const VarLenPoint2D& other) const {
    return x_ == other.x_ && y_ == other.y_;
  }

  inline bool operator!=(const VarLenPoint2D& other) const {
    return x_ != other.x_ || y_ != other.y_;
  }

  // Returns a copy of this point with CropOrResize() applied to both coordinates.
  // Warning: be careful of possible data loss.
  inline VarLenPoint2D ResizeCoordinates(size_t length) const {
    return VarLenPoint2D(length, x_, y_);
  }
  
 private:
  VarLenNumber x_;
  VarLenNumber y_;
};

// This structure represents a rectangle with four values: left (min X),
// right (max X), top (min Y), bottom (max Y) (ALL INCLUSIVE!)
// represented as VarLenNumber's.
struct VarLenRectangle {
 public:
  // Will construct a (0, 0, 0, 0) rectangle
  VarLenRectangle()
    : VarLenRectangle(VarLenNumber(), VarLenNumber(), VarLenNumber(), VarLenNumber()) { }
  
  // Note: will automatically adjust all numbers to the same length
  // Note: IT'S UP TO THE CALLER TO ENSURE THAT left <= right and top <= bottom.
  // Otherwise, Contains(), Intersects() and MBR() may yield incorrect results.
  VarLenRectangle(const VarLenNumber& left,
                  const VarLenNumber& top,
                  const VarLenNumber& right,
                  const VarLenNumber& bottom);
  
  // Will automatically adjust all numbers to number_length (be careful of
  // possible data loss if needed to crop numbers for that).
  // Note: IT'S UP TO THE CALLER TO ENSURE THAT left <= right and top <= bottom.
  // Otherwise, Contains(), Intersects() and MBR() may yield incorrect results.
  VarLenRectangle(size_t number_length,
                  const VarLenNumber& left,
                  const VarLenNumber& top,
                  const VarLenNumber& right,
                  const VarLenNumber& bottom)
    : VarLenRectangle(left.CropOrResize(number_length),
                      top.CropOrResize(number_length),
                      right.CropOrResize(number_length),
                      bottom.CropOrResize(number_length)) { }
  
  // Note: will automatically adjust both points' coordinates to the same length
  // Note: IT'S UP TO THE CALLER TO ENSURE THAT coordinates of leftTop <= respective coordinates of rightBottom.
  // Otherwise, Contains(), Intersects() and MBR() may yield incorrect results.
  VarLenRectangle(const VarLenPoint2D& leftTop, const VarLenPoint2D& rightBottom)
    : VarLenRectangle(leftTop.GetX(), leftTop.GetY(), rightBottom.GetX(), rightBottom.GetY()) { }

  // Note: 'left' denotes the min X point of the rectangle (inclusive)
  inline const VarLenNumber& GetLeft() const { return left_; }
  // Note: 'top' denotes the min Y point of the rectangle (inclusive)
  inline const VarLenNumber& GetTop() const { return top_; }
  // Note: 'right' denotes the max X point of the rectangle (INCLUSIVE!)
  inline const VarLenNumber& GetRight() const { return right_; }
  // Note: 'bottom' denotes the max Y point of the rectangle (INCLUSIVE!)
  inline const VarLenNumber& GetBottom() const { return bottom_; }

  // Returns false if this rectangle is not valid
  // (left > right or top > bottom).
  // Invalid rectangles should not be used for any calculations.
  inline bool IsValid() const {
    return left_ <= right_ && top_ <= bottom_;
  }

  inline bool operator==(const VarLenRectangle& other) const {
    return left_ == other.left_ &&
           top_ == other.top_ &&
           right_ == other.right_ &&
           bottom_ == other.bottom_;
  }

  inline bool operator!=(const VarLenRectangle& other) const {
    return left_ != other.left_ ||
           top_ != other.top_ ||
           right_ != other.right_ ||
           bottom_ != other.bottom_;
  }

  // Returns a copy of this rectangle with CropOrResize() applied to all four coordinates.
  // Warning: be careful of possible data loss.
  inline VarLenRectangle ResizeCoordinates(size_t length) const {
    return VarLenRectangle(length, left_, top_, right_, bottom_);
  }

  inline bool Contains(const VarLenPoint2D& point) const {
    return (point.GetX() >= left_) &&
           (point.GetX() <= right_) &&
           (point.GetY() >= top_) &&
           (point.GetY() <= bottom_);
  }

  inline bool Intersects(const VarLenRectangle& other) const {
    return !(other.right_ < left_) &&
           !(other.left_ > right_) &&
           !(other.bottom_ < top_) &&
           !(other.top_ > bottom_);
  }

  // Returns a rectangle that is the minimum bounding box for the two rectangles
  inline VarLenRectangle MBR(const VarLenRectangle& other) const {
    return VarLenRectangle(
      std::min(left_, other.left_),
      std::min(top_, other.top_),
      std::max(right_, other.right_),
      std::max(bottom_, other.bottom_)
    );
  }

 private:
  VarLenNumber left_;
  VarLenNumber top_;
  VarLenNumber right_;
  VarLenNumber bottom_;
};

}