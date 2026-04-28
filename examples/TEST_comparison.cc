// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

// The same as TEST_comparison but without SER-trees for speed comparison

#include <cstdio>
#include <memory>
#include <string>
#include <random>
#include <vector>

#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"

#include "table/hilbert/hilbert_table_factory.h"

using ROCKSDB_NAMESPACE::DB;
using ROCKSDB_NAMESPACE::Options;
using ROCKSDB_NAMESPACE::Slice;
using ROCKSDB_NAMESPACE::PinnableSlice;
using ROCKSDB_NAMESPACE::ReadOptions;
using ROCKSDB_NAMESPACE::Status;
using ROCKSDB_NAMESPACE::WriteBatch;
using ROCKSDB_NAMESPACE::WriteOptions;

using ROCKSDB_NAMESPACE::HilbertCode;

using ROCKSDB_NAMESPACE::BlockBasedTableOptions;
using ROCKSDB_NAMESPACE::HilbertTableFactory;

#if defined(OS_WIN)
std::string kDBPath = "C:\\Windows\\TEMP\\rocksdb_TEST_comparison";
#else
std::string kDBPath = "/tmp/rocksdb_TEST_comparison";
#endif

int main() {
  std::unique_ptr<DB> db;
  Options options;
  // Optimize RocksDB. This is the easiest way to get RocksDB to perform well
  // options.IncreaseParallelism();
  // options.OptimizeLevelStyleCompaction();
  // create the DB if it's not already present
  options.create_if_missing = true;

  // open DB
  Status s = DB::Open(options, kDBPath, &db);
  assert(s.ok());

  std::mt19937_64 mt(42);

  std::vector<std::pair<std::string, std::string>> remembered_examples;

  for (int n = 0; n < 0x20000; ++n) {
    WriteBatch batch;
    for (int m = 0; m < 0x80; ++m) {
      HilbertCode code(0, 0x10000 + n * 0x80 + m);

      std::string key(reinterpret_cast<const char*>(&code), sizeof(HilbertCode));
      std::string value = std::string("value") + std::to_string(n * 100 + m);

      batch.Put(key, value);

      if (m == 0 && n % 0x10 == 0)
        remembered_examples.emplace_back(key, value);
    }
    s = db->Write(WriteOptions(), &batch);
    assert(s.ok());
  }

  printf("=== Writing finished ===\n");
  fflush(stdout);

  ReadOptions opts;
  for (auto [key, expected_value]: remembered_examples) {
    std::string received_value;
    s = db->Get(opts, key, &received_value);
    assert(s.ok());
    assert(expected_value == received_value);
  }

  printf("=== Reading finished ===\n");
  fflush(stdout);

  db.reset();

  return 0;
}
