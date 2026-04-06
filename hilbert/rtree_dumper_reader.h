#pragma once

#include "hilbert/rtree.h"
#include "rocksdb/status.h"
#include "rocksdb/env.h"

#include <stdint.h>

// As of now, RTreeDumper and RTreeReader only support RTree<uint32_t>

namespace ROCKSDB_NAMESPACE {

constexpr uint64_t MUSEUMDB2_RTREE_MAGIC_NUMBER = 0x79224cdec3730850u;  // obtained by $ echo museumdb2.hilbert.rtree | sha256sum

// FORMAT OF R-TREE FILE (n denotes the byte length of the points' coordinates in the R-tree): 
//
// For each node:
// 1 byte: 0x1 if this node is leaf, else 0x0
// 4*n bytes: the MBR of the node's children (left, top, right, bottom), all coordinates in little endian
// 4 bytes: number of node's children
//
// If node is leaf, for each node's child:
// 2*n bytes: the two coordinates of the point (X, Y) in little endian
// 4 bytes: the value of the item
//
// If node is non-leaf, for each node's child:
// 4 bytes: the position of the child node in this file
//
// Footer:
// 4 bytes: the byte length of all the numbers in the R-tree (n)
// 4 bytes: the position of the root node of the tree in this file
// 8 bytes: magic number
//
struct RTreeDumper {
 private:
  using RTreeNode = typename RTree<uint32_t>::RTreeNode;

 public:
  // For the exact format of R-Tree files, see the description of RTreeDumper.
  // WARNING: calling this will not close the file. Be sure to close it manually.
  static Status Dump(const RTree<uint32_t>& tree,
                  WritableFile& file);

 private:
  // Dumps node and its subtree into the file.
  // Stores the start position of node's info in file in node_start
  static Status DumpRec_(const RTreeNode* node, uint32_t number_length, WritableFile& file, uint32_t* node_start);
};

#pragma pack(push, 1)
struct RTreeFileFooter {
  uint32_t number_length;
  uint32_t root_position;
  uint64_t magic_number;
};
#pragma pack(pop)

// This structure accepts a R-Tree file name and can perform
// point and range search queries on the R-Tree in the file.
// It does not extract the R-Tree into an actual RTree instance in memory,
// but rather performs the search by seeking the data in file.
// WARNING: This structure keeps the reference to the file. Make sure the
// file object is not destroyed before this object.
// WARNING: This structure does not close the file. Be sure to close it manually.
struct RTreeReader {
 public:
  static Status NewRTreeReader(RandomAccessFile& file, RTreeReader* out);

  std::optional<RTreeEntry<uint32_t>> Find(const VarLenPoint2D& key_point, Status* s);

  std::vector<RTreeEntry<uint32_t>> RangeQuery(const VarLenRectangle& rect, Status* s);

  // Closes the file.
  // WARNING: do not call any other methods of this reader after calling Close()
  Status Close();

 private:
  RTreeReader(RandomAccessFile& file)
    : file_(file) { }

  // After this function, the file position will be where it was
  Status SearchRec_(uint32_t node_pos, const VarLenRectangle& rect, std::vector<RTreeEntry<uint32_t>>& result);

  RandomAccessFile& file_;
  uint32_t data_area_size_;
  RTreeFileFooter footer_;
};

}
