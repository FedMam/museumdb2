#pragma once

#include <memory>
#include <vector>

#include "hilbert/hilbert_curve.h"
#include "file/writable_file_writer.h"
#include "rocksdb/status.h"
#include "table/format.h"

namespace ROCKSDB_NAMESPACE {

const uint64_t MUSEUMDB2_SER_TREE_FILE_MAGIC_NUMBER = 0x98e13db6c9118546ull; // obtained by `$ echo museumdb2.table.hilbert.SERTreeBuilder | sha256sum` and taking the trailing 8 bytes

/*
A SER-tree consists of nodes. Each node can either be a leaf node
or a non-leaf node. A non-leaf node's children are other nodes.
A leaf node's children are blocks of the SSTable.

SER-tree format:

For each node:
- 1 byte: 1 if it is a leaf node or 0 if it's a non-leaf node
- sizeof(uint32_t): number of children
- If this is a leaf node, for each child:
  -- sizeof(BlockHandle): the BlockHandle of the original block in SST file
  -- sizeof(UInt64Rectangle): MBR of the original block in SST file
- Else if this is a non-leaf node, for each child:
  -- sizeof(uint32_t): position (offset) of the child node in file
  -- sizeof(UInt64Rectangle): MBR of the child node

Footer:
- sizeof(uint32_t): position (offset) of the root node in file
- sizeof(UInt64Rectangle): MBR of the whole tree
- sizeof(uint64_t): magic number
*/
class SERTreeBuilder {
 public:
  // TODO (FedMam): implement customizability for capacities
  SERTreeBuilder(std::unique_ptr<WritableFileWriter> ser_tree_file,
                 const IOOptions& opts,
                 Env* env = Env::Default(),
                 uint32_t leaf_capacity = 4,
                 uint32_t non_leaf_capacity = 4);

  ~SERTreeBuilder();
  
  SERTreeBuilder(const SERTreeBuilder& other) = delete;
  SERTreeBuilder(SERTreeBuilder&& other) = delete;

  IOStatus Add(const BlockHandle& block_handle, const UInt64Rectangle& mbr);

  IOStatus Finish();

  IOStatus Abandon();

  inline std::string SERTreeFileName() const {
    return file_->file_name();
  }

 private:
  inline bool ok() { return status_.ok(); }

  std::unique_ptr<WritableFileWriter> file_;
  IOOptions opts_;
  Env* env_;

  uint32_t leaf_capacity_;  
  uint32_t non_leaf_capacity_;

  IOStatus status_ = IOStatus::OK();

  bool finished_ = false;

  #pragma pack(push, 1)
  struct SERTreeNodeHeader {
    uint8_t is_leaf;
    uint32_t num_children;
  };

  // offset: position of the node info in file
  // also acts as a non-leaf node child representation in file
  struct SERTreeNodeInfo {
    uint32_t offset;
    UInt64Rectangle mbr;
  };

  struct SERTreeLeafNodeChildRepr {
    BlockHandle block_handle;
    UInt64Rectangle mbr;
  };
  #pragma pack(pop)

  // this will be used in Finish() to build all the non-leaf nodes atop leaf nodes
  std::vector<SERTreeNodeInfo> leaf_nodes_;

  // the elements of Add() calls will be remembered in memory and after the
  // leaf_capacity_-th call, they will be written in file
  uint32_t next_leaf_node_position_in_file_;
  SERTreeNodeHeader next_leaf_node_header_{1, 0};
  UInt64Rectangle next_leaf_node_mbr_ = UInt64Rectangle::CreateInvalidRectangle();
  std::vector<SERTreeLeafNodeChildRepr> next_leaf_node_children_;

  IOStatus WriteLeafNode_();
};

}
