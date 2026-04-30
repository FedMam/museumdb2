#pragma once

#include <memory>
#include <vector>

#include "hilbert/hilbert_curve.h"
#include "hilbert/ser_tree_util.h"
#include "file/writable_file_writer.h"
#include "rocksdb/status.h"

namespace ROCKSDB_NAMESPACE {

/*
A SER-tree consists of nodes. Each node can either be a leaf node
or a non-leaf node. A non-leaf node's children are other nodes.
A leaf node's children are blocks of the SSTable.

SER-tree format:

For each node, first header (see SERTreeNodeHeader):
- 1 byte: 1 if it is a leaf node or 0 if it's a non-leaf node
- sizeof(uint32_t): number of children

Then, if this is a leaf node, for each child (see SERTreeLeafNodeChildRepr):
- sizeof(BlockHandle): the BlockHandle of the original block in SST file
- sizeof(UInt64Rectangle): MBR of the original block in SST file

Else if this is a non-leaf node, for each child (see SERTreeNodeInfo):
- sizeof(uint64_t): position (offset) of the child node in file
- sizeof(UInt64Rectangle): MBR of the child node

Footer (see SERTreeFooter):
- sizeof(uint64_t): position (offset) of the root node in file
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

  // this will be used in Finish() to build all the non-leaf nodes atop leaf nodes
  std::vector<SERTreeNodeInfo> leaf_nodes_;

  // the elements of Add() calls will be remembered in memory and after the
  // leaf_capacity_-th call, they will be written in file
  uint64_t next_leaf_node_position_in_file_;
  SERTreeNodeHeader next_leaf_node_header_{1, 0};
  UInt64Rectangle next_leaf_node_mbr_ = UInt64Rectangle::CreateInvalidRectangle();
  std::vector<SERTreeLeafNodeChildRepr> next_leaf_node_children_;

  IOStatus WriteLeafNode_();
};

}
