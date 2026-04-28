#pragma once

#include "rocksdb/slice.h"
#include "db/dbformat.h"
#include "hilbert/hilbert_curve.h"

namespace ROCKSDB_NAMESPACE {

inline bool TryParseHilbertCode(const Slice& internal_key, HilbertCode* result) {
  std::string user_key = ExtractUserKey(internal_key).ToString();
  if (!HilbertCode::IsValidHilbertCodeString(user_key))
    return false;
  *result = HilbertCode::FromString(user_key);
  return true;
}

}
