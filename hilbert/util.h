#pragma once

#include "rocksdb/slice.h"
#include "db/dbformat.h"
#include "hilbert/hilbert_curve.h"

namespace ROCKSDB_NAMESPACE {

inline bool TryParseHilbertCode(const Slice& internal_key, HilbertCode* result) {
  Slice user_key = ExtractUserKey(internal_key);
  if (user_key.size() != sizeof(uint64_t) * 2)
    return false;
  *result = HilbertCode(
    *reinterpret_cast<const uint64_t*>(user_key.data()),
    *reinterpret_cast<const uint64_t*>(user_key.data() + sizeof(uint64_t)));
  return true;
}

}
