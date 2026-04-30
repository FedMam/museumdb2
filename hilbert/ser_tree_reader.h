#pragma once

#include <memory>

#include "hilbert/hilbert_curve.h"
#include "hilbert/ser_tree_util.h"

#include "file/random_access_file_reader.h"
#include "rocksdb/io_status.h"
#include "table/format.h"

namespace ROCKSDB_NAMESPACE {

class SERTreeReader {
 public:
  ~SERTreeReader();

  SERTreeReader(const SERTreeReader& other) = delete;
  SERTreeReader(SERTreeReader&& other) = delete;

  static Status Open(std::unique_ptr<RandomAccessFileReader> ser_tree_file,
                     const IOOptions& io_options,
                     std::unique_ptr<SERTreeReader>* ser_tree_reader);

  inline bool ok() { return status_.ok(); }
  inline Status status() { return status_; }
  inline IOStatus io_status() { return status_; }

  inline UInt64Rectangle MBR() { return footer_.mbr; }

  Status Find(const UInt64Rectangle& rectangle, std::vector<BlockHandle>* result);

 private:
  SERTreeReader(std::unique_ptr<RandomAccessFileReader> ser_tree_file,
                const IOOptions& io_options);

  Status FindRec_(uint64_t node_offset, const UInt64Rectangle& rectangle, std::vector<BlockHandle>& result);

  std::unique_ptr<RandomAccessFileReader> file_;
  IOOptions io_options_;
  uint64_t file_size_;
  SERTreeFooter footer_;
  IOStatus status_ = IOStatus::OK();
};

}
