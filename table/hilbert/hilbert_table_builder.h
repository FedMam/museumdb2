#pragma once

#include <memory>
#include <unordered_map>

#include "table/block_based/block_based_table_builder.h"
#include "rocksdb/external_table.h"
#include "hilbert/ser_tree_builder.h"
#include "port/port.h"

namespace ROCKSDB_NAMESPACE {

class HilbertTableBuilder : public BlockBasedTableBuilder {
 public:
  // io_options are to provide to SERTreeBuilder
  // env is to delete file in case of error
  HilbertTableBuilder(const BlockBasedTableOptions& table_options,
                      const TableBuilderOptions& table_builder_options,
                      WritableFileWriter* file,
                      std::unique_ptr<WritableFileWriter> ser_tree_file,
                      const IOOptions& io_options = IOOptions(),
                      Env* env = Env::Default(),
                      uint32_t leaf_capacity = 4,
                      uint32_t non_leaf_capacity = 4);

  void Add(const Slice& key, const Slice& value) override;

  void Flush(const Slice* first_key_in_next_block) override;

  Status Finish() override;

  void Abandon() override;

  inline Status SERTreeStatus() const {
    return SERTreeIOStatus();
  }
  inline IOStatus SERTreeIOStatus() const {
    return ser_tree_status;
  }

 private:
  inline bool ser_tree_ok() {
    return ser_tree_status.ok();
  }

  void NotifyOnPreparingIndexEntry(void* index_entry_pointer) override;
  
  void NotifyOnFinishingIndexEntry(const BlockHandle& pending_handle,
                                   void* index_entry_pointer) override;

  // This mutex protects all the members below from a data race when parallel
  // compression is active
  port::Mutex parallel_emitting_mutex;

  std::unique_ptr<SERTreeBuilder> ser_tree_builder;
  IOStatus ser_tree_status = IOStatus::OK();
  UInt64Rectangle current_mbr = UInt64Rectangle::CreateInvalidRectangle();

  // This map maps index entry nodes to MBRs of the blocks that are
  // yet to get their block handles due to parallel compression.
  std::unordered_map<void*, UInt64Rectangle> map_for_parallel_emitting;
};

} // namespace ROCKSDB_NAMESPACE
