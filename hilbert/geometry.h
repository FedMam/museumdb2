#pragma once

#include "rocksdb/rocksdb_namespace.h"

#include <stdint.h>
#include <algorithm>

namespace ROCKSDB_NAMESPACE {

// these structures will be written to and read from files
#pragma pack(push, 1)

struct UInt64Point {
 public:
  UInt64Point(uint64_t x, uint64_t y)
    : x_(x), y_(y) { }

  inline uint64_t GetX() const { return x_; }
  inline uint64_t GetY() const { return y_; }

 private:
  uint64_t x_;
  uint64_t y_;
};

// Note: left = min X, right = max X, top = min Y, bottom = max Y (all inclusive)
// Note: if left > right or top > bottom, the rectangle will be considered
// an invalid rectangle. An invalid rectangle does not contain any point or
// intersect with any rectangle. MBR of a valid and an invalid rectangle is
// the valid rectangle. MBR of two invalid rectangles is an invalid rectangle.
struct UInt64Rectangle {
 public:
  UInt64Rectangle(uint64_t left, uint64_t top, uint64_t right, uint64_t bottom)
    : left_(left), top_(top), right_(right), bottom_(bottom) { }
  
  UInt64Rectangle(UInt64Point leftTop, UInt64Point rightBottom)
    : left_(leftTop.GetX()), top_(leftTop.GetY()), right_(rightBottom.GetX()), bottom_(rightBottom.GetY()) { }

  inline uint64_t GetLeft() const { return left_; }
  inline uint64_t GetTop() const { return top_; }
  inline uint64_t GetRight() const { return right_; }
  inline uint64_t GetBottom() const { return bottom_; }

  inline bool IsValid() const {
    return left_ <= right_ && top_ <= bottom_;
  }

  inline bool Contains(const UInt64Point& point) const {
    if (!IsValid()) return false;
    return left_ <= point.GetX() && point.GetX() <= right_ &&
           top_ <= point.GetY() && point.GetY() <= bottom_;
  }

  inline bool Intersects(const UInt64Rectangle& other) const {
    if (!IsValid()) return false;
    return right_ >= other.left_ &&
           left_ <= other.right_ &&
           bottom_ >= other.top_ &&
           top_ <= other.bottom_;
  }

  inline UInt64Rectangle MBR(const UInt64Point& point) const {
    if (!IsValid()) return UInt64Rectangle(point, point);
    return UInt64Rectangle(
      std::min(left_, point.GetX()),
      std::min(top_, point.GetY()),
      std::max(right_, point.GetX()),
      std::max(bottom_, point.GetY()));
  }

  inline UInt64Rectangle MBR(const UInt64Rectangle& other) const {
    if (!IsValid()) return other;
    if (!other.IsValid()) return *this;
    return UInt64Rectangle(
      std::min(left_, other.left_),
      std::min(top_, other.top_),
      std::max(right_, other.right_),
      std::max(bottom_, other.bottom_));
  }

  static inline UInt64Rectangle CreateInvalidRectangle() {
    return UInt64Rectangle(1, 1, 0, 0);
  }

 private:
  uint64_t left_;
  uint64_t top_;
  uint64_t right_;
  uint64_t bottom_;
};

#pragma pack(pop)

} // namespace ROCKSDB_NAMESPACE

namespace std {

namespace {

uint64_t splitmix64(uint64_t x) {
  uint64_t z = x;
  z += 0x9e3779b97f4a7c15ull;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
  return z ^ (z >> 31);
}

}

template<>
struct hash<ROCKSDB_NAMESPACE::UInt64Point> {
  size_t operator()(const ROCKSDB_NAMESPACE::UInt64Point& point) const {
    uint64_t xhash = hash<uint64_t>{}(point.GetX());
    uint64_t yhash = hash<uint64_t>{}(point.GetY());
    return splitmix64(splitmix64(xhash) ^ (splitmix64(yhash) << 2)) + splitmix64(yhash);
  }
};

}
