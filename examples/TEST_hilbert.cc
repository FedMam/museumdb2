// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <cstdio>
#include <memory>
#include <string>
#include <random>

#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"

#include "table/hilbert/hilbert_table_factory.h"

using ROCKSDB_NAMESPACE::DB;
using ROCKSDB_NAMESPACE::Options;
using ROCKSDB_NAMESPACE::PinnableSlice;
using ROCKSDB_NAMESPACE::ReadOptions;
using ROCKSDB_NAMESPACE::Status;
using ROCKSDB_NAMESPACE::WriteBatch;
using ROCKSDB_NAMESPACE::WriteOptions;

using ROCKSDB_NAMESPACE::HilbertCode;

using ROCKSDB_NAMESPACE::BlockBasedTableOptions;
using ROCKSDB_NAMESPACE::HilbertTableFactory;

#if defined(OS_WIN)
std::string kDBPath = "C:\\Windows\\TEMP\\rocksdb_TEST_hilbert";
#else
std::string kDBPath = "/tmp/rocksdb_TEST_hilbert";
#endif

int main() {
  std::unique_ptr<DB> db;
  Options options;
  // Optimize RocksDB. This is the easiest way to get RocksDB to perform well
  // options.IncreaseParallelism();
  // options.OptimizeLevelStyleCompaction();
  // create the DB if it's not already present
  options.create_if_missing = true;

  options.spatial_data = true;
  auto hilbert_factory = std::make_shared<HilbertTableFactory>(BlockBasedTableOptions());
  options.table_factory = hilbert_factory;

  // open DB
  Status s = DB::Open(options, kDBPath, &db);
  assert(s.ok());

  std::mt19937_64 mt(42);

  for (int n = 0; n < 0x20000; ++n) {
    WriteBatch batch;
    for (int m = 0; m < 0x80; ++m) {
      HilbertCode code(0, 0x10000 + n * 0x80 + m);

      batch.Put(
        std::string(reinterpret_cast<const char*>(&code), sizeof(HilbertCode)),
        std::string("value") + std::to_string(n * 100 + m));
    }
    s = db->Write(WriteOptions(), &batch);
    assert(s.ok());
  }

  db.reset();

  return 0;
}
