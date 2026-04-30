#pragma once

#include <stdint.h>

#include "hilbert/geometry.h"
#include "rocksdb/rocksdb_namespace.h"
#include "table/format.h"

namespace ROCKSDB_NAMESPACE {

const uint64_t MUSEUMDB2_SER_TREE_FILE_MAGIC_NUMBER = 0x98e13db6c9118546ull; // obtained by `$ echo museumdb2.table.hilbert.SERTreeBuilder | sha256sum` and taking the trailing 8 bytes

#pragma pack(push, 1)
// Only used by SERTreeBuilder and SERTreeReader internally
struct SERTreeNodeHeader {
  uint8_t is_leaf;
  uint32_t num_children;
};

// Only used by SERTreeBuilder and SERTreeReader internally
// offset: position of the node info in file
// also acts as a non-leaf node child representation in file
struct SERTreeNodeInfo {
  uint64_t offset;
  UInt64Rectangle mbr;
};

// Only used by SERTreeBuilder and SERTreeReader internally
struct SERTreeLeafNodeChildRepr {
  BlockHandle block_handle;
  UInt64Rectangle mbr;
};

struct SERTreeFooter {
  SERTreeFooter()
    : mbr(UInt64Rectangle::CreateInvalidRectangle()) { }

  uint64_t root_offset;
  UInt64Rectangle mbr;
  uint64_t magic_number;
};
#pragma pack(pop)

}