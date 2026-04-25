#pragma once

#include <string>

#include "rocksdb/rocksdb_namespace.h"

namespace ROCKSDB_NAMESPACE {

const std::string SER_TREE_FILE_EXTENSION_SUFFIX = ".ser";

inline std::string GetSERTreeFileName(std::string sst_file_name) {
  return sst_file_name + SER_TREE_FILE_EXTENSION_SUFFIX;
}

}
