#pragma once

#include "hilbert/geometry.h"
#include "hilbert/rtree.h"

#include "rocksdb/rocksdb_namespace.h"

#include <assert.h>

#include <string>
#include <vector>
#include <random>

namespace ROCKSDB_NAMESPACE {

VarLenNumber RandomNumber(std::mt19937& mt, int n_bytes) {
  std::string repr;
  int left_bytes = n_bytes;
  while (left_bytes > 0) {
    uint32_t rand_num = mt();
    for (unsigned i = 0; i < sizeof(uint32_t); ++i) {
      if (left_bytes > 0) {
        repr += static_cast<char>((rand_num >> (8 * i)) & 0xff);
        --left_bytes;
      } else break;
    }
  }
  return VarLenNumber(n_bytes, repr);
}

VarLenRectangle RandomRectangle(std::mt19937& mt, int n_bytes) {
  VarLenNumber left = RandomNumber(mt, n_bytes),
    top = RandomNumber(mt, n_bytes),
    right = RandomNumber(mt, n_bytes),
    bottom = RandomNumber(mt, n_bytes);
  
  VarLenNumber temp = left;
  left = std::min(left, right);
  right = std::max(temp, right);

  temp = top;
  top = std::min(top, bottom);
  bottom = std::max(temp, bottom);

  return VarLenRectangle(left, top, right, bottom);
}

// Helper function to quickly test a range query result
// in which items may be stored in arbitrary order.
template<typename TItem>
inline bool ItemInRangeResult(const std::vector<RTreeEntry<TItem>>& range_result, const TItem& item, const VarLenPoint2D& expected_point) {
  for (auto entry: range_result)
    if (entry.GetItem() == item && entry.GetPointWithoutHilbertCode() == expected_point)
      return true;
  return false;
}

}

