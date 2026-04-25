#pragma once

#include <memory>

#include "table/block_based/block_based_table_builder.h"
#include "rocksdb/external_table.h"
#include "hilbert/ser_tree_builder.h"

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
                      Env* env = Env::Default());

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

  std::unique_ptr<SERTreeBuilder> ser_tree_builder;
  IOStatus ser_tree_status = IOStatus::OK();
  UInt64Rectangle current_mbr = UInt64Rectangle::CreateInvalidRectangle();
};

} // namespace ROCKSDB_NAMESPACE
