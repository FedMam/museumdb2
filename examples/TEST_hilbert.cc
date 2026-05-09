// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <cstdio>
#include <memory>
#include <string>
#include <random>
#include <vector>
#include <thread>
#include <chrono>

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

using ROCKSDB_NAMESPACE::UInt64Rectangle;
using ROCKSDB_NAMESPACE::HilbertCode;
using ROCKSDB_NAMESPACE::HilbertCodeToPoint;
using ROCKSDB_NAMESPACE::PointToHilbertCode;

using ROCKSDB_NAMESPACE::Iterator;
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

  std::vector<std::pair<HilbertCode, std::string>> remembered_examples;

  UInt64Rectangle mbr;

  const uint64_t EXPECTED_RESULT_NUMBER = 0x20000 * 0x80;

  for (int n = 0; n < 0x20000; ++n) {
    WriteBatch batch;
    for (int m = 0; m < 0x80; ++m) {
      HilbertCode code(0, 0x10000 + n * 0x80 + m);

      std::string key = code.ToString();
      std::string value = std::string("value") + std::to_string(n * 100 + m);

      batch.Put(key, value);

      auto point = HilbertCodeToPoint(code);
      if (m == 0 && n % 0x10 == 0)
        remembered_examples.emplace_back(code, value);
      mbr = mbr.MBR(point);
    }
    s = db->Write(WriteOptions(), &batch);
    assert(s.ok());
  }

  printf("=== Writing finished ===\n");
  fflush(stdout);

  ReadOptions opts;
  for (auto [code, expected_value]: remembered_examples) {
    std::string received_value;
    s = db->Get(opts, code.ToString(), &received_value);
    assert(s.ok());
    assert(expected_value == received_value);
  }

  printf("=== Reading finished ===\n");
  fflush(stdout);

  const uint64_t MAX_RECT_WIDTH = 10;

  for (unsigned i = 0; i < 100; ++i) {
    uint64_t left = mt() % (mbr.GetRight() - mbr.GetLeft() - MAX_RECT_WIDTH) + mbr.GetLeft();
    uint64_t right = left + (mt() % MAX_RECT_WIDTH);
    uint64_t top = mt() % (mbr.GetBottom() - mbr.GetTop() - MAX_RECT_WIDTH) + mbr.GetTop();
    uint64_t bottom = top + (mt() % MAX_RECT_WIDTH);
    if (left > right) { auto temp = left; left = right; right = temp; }
    if (top > bottom) { auto temp = top; top = bottom; bottom = temp; }
    UInt64Rectangle rect(left, top, right, bottom);

    auto result = db->RectangularRangeQuery(opts,
      db->DefaultColumnFamily(), rect, &s);
    if (!s.ok()) {
      printf("%s\n", s.ToString().c_str());
      assert(false);
    }
    if (result.size() > (right - left + 1) * (bottom - top + 1)) {
      printf("Too many results for rectangle (%lu, %lu, %lu, %lu)\n", left, top, right, bottom);
      assert(false);
    }

    for (const auto& entry: result) {
      assert(rect.Contains(entry.first));
    }
  }

  printf("=== Range query finished ===\n");
  fflush(stdout);

  auto result = db->RectangularRangeQuery(opts,
    db->DefaultColumnFamily(),
    UInt64Rectangle(0x0000000000000000ull,
                    0x0000000000000000ull,
                    0xffffffffffffffffull,
                    0xffffffffffffffffull), &s);
  assert(s.ok());
  if (result.size() != EXPECTED_RESULT_NUMBER) {
    printf("Error: result size is %ld\n", result.size());
    assert(false);
  }

  printf("=== Large range query finished ===\n");
  fflush(stdout);

  db.reset();

  return 0;
}
