#include "table/hilbert/hilbert_table_factory.h"

#include "table/hilbert/hilbert_table_util.h"

namespace ROCKSDB_NAMESPACE {

TableBuilder* HilbertTableFactory::NewTableBuilder(
      const TableBuilderOptions& table_builder_options,
      WritableFileWriter* file) const {
  Status s = Status::OK();

  std::string file_name = file->file_name();
  std::string ser_tree_file_name = GetSERTreeFileName(file_name);
  std::unique_ptr<FSWritableFile> ser_tree_file;

  s = env_->GetFileSystem()->NewWritableFile(
    ser_tree_file_name,
    file_options_,
    &ser_tree_file,
    nullptr);

  // TODO (FedMam): more meaningful error handling
  assert(s.ok());

  std::unique_ptr<WritableFileWriter> ser_writer = std::make_unique<WritableFileWriter>(
    std::move(ser_tree_file), ser_tree_file_name, file_options_);

  return new HilbertTableBuilder(
    table_options_,
    table_builder_options,
    file,
    std::move(ser_writer),
    file_options_.io_options);
}

} // namespace ROCKSDB_NAMESPACE
