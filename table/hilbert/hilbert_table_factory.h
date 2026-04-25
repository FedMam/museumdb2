#pragma once

#include "rocksdb/external_table.h"
#include "table/hilbert/hilbert_table_builder.h"
#include "table/block_based/block_based_table_factory.h"

namespace ROCKSDB_NAMESPACE {

class HilbertTableFactory : public BlockBasedTableFactory {
 public:
  // Note: env_options, file_options and env will be used when creating
  // SER-tree files for SSTables
  explicit HilbertTableFactory(
    const BlockBasedTableOptions& table_options,
    const EnvOptions& env_options = EnvOptions(),
    const FileOptions& file_options = FileOptions(),
    Env* env = Env::Default())
    : BlockBasedTableFactory(table_options),
      env_(env),
      env_options_(env_options),
      file_options_(file_options) { }

  static const char* kClassName() { return kHilbertTableName(); }
  const char* Name() const override { return kHilbertTableName(); }

  TableBuilder* NewTableBuilder(
      const TableBuilderOptions& table_builder_options,
      WritableFileWriter* file) const override;

  std::unique_ptr<TableFactory> Clone() const override {
    return std::make_unique<HilbertTableFactory>(*this);
  }

  bool IsDeleteRangeSupported() const override { return false; }

  bool IsSpatialDataSupported() const override { return true; }

 private:
  // for creating SER-tree files
  Env* env_;
  EnvOptions env_options_;
  FileOptions file_options_;
};

} // namespace ROCKSDB_NAMESPACE
