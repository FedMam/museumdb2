#include "hilbert/ser_tree_builder.h"

#include "port/likely.h"

namespace ROCKSDB_NAMESPACE {

SERTreeBuilder::SERTreeBuilder(std::unique_ptr<WritableFileWriter> ser_tree_file,
                               const IOOptions& opts,
                               Env* env,
                               uint32_t leaf_capacity,
                               uint32_t non_leaf_capacity)
  : file_(std::move(ser_tree_file)),
    opts_(opts),
    env_(env),
    leaf_capacity_(leaf_capacity),
    non_leaf_capacity_(non_leaf_capacity),
    next_leaf_node_position_in_file_(file_->GetFileSize()) { }

SERTreeBuilder::~SERTreeBuilder() {
  // if (!finished_)
  //  Abandon();
  assert(finished_);
}

IOStatus SERTreeBuilder::Add(const BlockHandle& block_handle, const UInt64Rectangle& mbr) {
  if (UNLIKELY(!ok())) {
    return status_;
  }
  if (UNLIKELY(finished_)) {
    status_ = IOStatus::NotSupported("SERTreeBuilder::Add(): already finished");
    return status_;
  }

  ++next_leaf_node_header_.num_children;
  next_leaf_node_mbr_ = next_leaf_node_mbr_.MBR(mbr);
  next_leaf_node_children_.push_back({block_handle, mbr});

  assert(next_leaf_node_children_.size() <= leaf_capacity_);
  if (next_leaf_node_children_.size() == leaf_capacity_) {
    status_ = WriteLeafNode_();
  }
  return status_;
}

IOStatus SERTreeBuilder::WriteLeafNode_() {
  IOStatus s = file_->Append(opts_,
    Slice(reinterpret_cast<const char*>(&next_leaf_node_header_), sizeof(next_leaf_node_header_)));
  if (UNLIKELY(!s.ok())) return s;
  s = file_->Append(opts_,
    Slice(reinterpret_cast<const char*>(next_leaf_node_children_.data()), next_leaf_node_children_.size() * sizeof(SERTreeLeafNodeChildRepr)));
  if (UNLIKELY(!s.ok())) return s;

  leaf_nodes_.push_back({next_leaf_node_position_in_file_, next_leaf_node_mbr_});

  next_leaf_node_position_in_file_ = file_->GetFileSize();
  next_leaf_node_header_.num_children = 0;
  next_leaf_node_mbr_ = UInt64Rectangle::CreateInvalidRectangle();
  next_leaf_node_children_.clear();

  return IOStatus::OK();
}

IOStatus SERTreeBuilder::Finish() {
  if (UNLIKELY(!ok())) {
    return status_;
  }
  if (UNLIKELY(finished_)) {
    status_ = IOStatus::NotSupported("SERTreeBuilder::Finish(): already finished");
    return status_;
  }

  if (next_leaf_node_children_.size() > 0)
    WriteLeafNode_();

  if (leaf_nodes_.size() == 0) {
    status_ = IOStatus::InvalidArgument("SERTreeBuilder::Finish(): cannot create a SER-tree file with no entries");
    return status_;
  }

  // next level nodes are parents of current level nodes
  std::vector<SERTreeNodeInfo> current_level_nodes = leaf_nodes_;
  leaf_nodes_.clear();

  while (current_level_nodes.size() > 1) {
    std::vector<SERTreeNodeInfo> next_level_nodes;

    // splitting current level nodes into groups of size non_leaf_capacity_ as children of next level nodes
    for (uint32_t new_node_first_child = 0; new_node_first_child < current_level_nodes.size(); new_node_first_child += non_leaf_capacity_) {
      uint32_t new_node_num_children = std::min(non_leaf_capacity_, (uint32_t)current_level_nodes.size() - new_node_first_child);

      UInt64Rectangle new_node_mbr = UInt64Rectangle::CreateInvalidRectangle();
      for (uint32_t i = 0; i < new_node_num_children; ++i)
        new_node_mbr = new_node_mbr.MBR(current_level_nodes[new_node_first_child + i].mbr);

      SERTreeNodeInfo new_node_info{current_level_nodes[new_node_first_child].offset, new_node_mbr};
      next_level_nodes.push_back(new_node_info);

      SERTreeNodeHeader new_node_header{0, new_node_num_children};

      status_ = file_->Append(opts_,
        Slice(reinterpret_cast<const char*>(&new_node_header), sizeof(new_node_header)));
      if (UNLIKELY(!ok())) { return status_; }
      status_ = file_->Append(opts_,
        Slice(reinterpret_cast<const char*>(current_level_nodes.data() + new_node_first_child), sizeof(SERTreeNodeInfo) * new_node_num_children));
      if (UNLIKELY(!ok())) { return status_; }
    }

    current_level_nodes = next_level_nodes;
  }
  assert(current_level_nodes.size() == 1);

  // footer
  status_ = file_->Append(opts_,
    Slice(reinterpret_cast<const char*>(&current_level_nodes[0]), sizeof(SERTreeNodeInfo)));
  if (UNLIKELY(!ok())) { return status_; }
  status_ = file_->Append(opts_,
    Slice(reinterpret_cast<const char*>(&MUSEUMDB2_SER_TREE_FILE_MAGIC_NUMBER), sizeof(uint64_t)));
  if (UNLIKELY(!ok())) { return status_; }

  finished_ = true;
  status_ = file_->Close(opts_);
  return status_;
}

IOStatus SERTreeBuilder::Abandon() {
  if (UNLIKELY(finished_))
    return status_;

  finished_ = true;
  status_ = file_->Close(opts_);
  if (LIKELY(status_.ok())) {
    Status ds = env_->DeleteFile(file_->file_name());
    if (UNLIKELY(!ds.ok()))
      status_ = IOStatus::IOError(std::string("SERTreeBuilder::Abandon(): failed to delete SER-tree file ") + file_->file_name() + ": " + ds.ToString());
  }
  return status_;
}

}
