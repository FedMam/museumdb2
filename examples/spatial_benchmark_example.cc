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
#include <filesystem>

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

using ROCKSDB_NAMESPACE::UInt64Point;
using ROCKSDB_NAMESPACE::UInt64Rectangle;
using ROCKSDB_NAMESPACE::HilbertCode;
using ROCKSDB_NAMESPACE::HilbertCodeToPoint;
using ROCKSDB_NAMESPACE::PointToHilbertCode;

using ROCKSDB_NAMESPACE::Iterator;
using ROCKSDB_NAMESPACE::CompactRangeOptions;
using ROCKSDB_NAMESPACE::WaitForCompactOptions;
using ROCKSDB_NAMESPACE::BlockBasedTableOptions;
using ROCKSDB_NAMESPACE::HilbertTableFactory;

#if defined(OS_WIN)
std::string kDBPath = "C:\\Windows\\TEMP\\rocksdb_TEST_hilbert_comprehensive";
#else
std::string kDBPath = "/tmp/rocksdb_TEST_hilbert_comprehensive";
#endif

int main(int argc, char** argv) {
  std::vector<std::string> animals = { "Lion", "Tiger", "Elephant", "Giraffe", "Zebra", "Rhino", "Hippo", "Gorilla", "Orangutan", "Chimpanzee", "Kangaroo", "Koala", "Sloth", "Panda", "Moose", "Deer", "Fox", "Wolf", "Coyote", "Bear", "Otter", "Beaver", "Squirrel", "Hare", "Rabbit", "Hedgehog", "Badger", "Skunk", "Raccoon", "Seal", "Walrus", "Dolphin", "Whale", "Porpoise", "Shark", "Tuna", "Salmon", "Trout", "Carp", "Catfish", "Pike", "Mackerel", "Marlin", "Seahorse", "Jellyfish", "Octopus", "Squid", "Crab", "Lobster", "Shrimp", "Butterfly", "Moth", "Bee", "Wasp", "Ant", "Dragonfly", "Grasshopper", "Locust", "Beetle", "Ladybug", "Firefly", "Cicada", "Spider", "Scorpion", "Snake", "Cobra", "Viper", "Python", "Anaconda", "Turtle", "Tortoise", "Iguana", "Gecko", "Chameleon", "Alligator", "Crocodile", "Falcon", "Hawk", "Eagle", "Vulture", "Owl", "Sparrow", "Robin", "Finch", "Pigeon", "Dove", "Parrot", "Cockatoo", "Macaw", "Flamingo", "Pelican", "Heron", "Stork", "Crane", "Swan", "Goose", "Duck", "Turkey", "Chicken", "Peacock", "Emu", "Ostrich", "Penguin", "Platypus" };
  std::vector<std::string> adjectives = { "Agile", "Alert", "Ancient", "Armored", "Astonishing", "Attentive", "Audacious", "Austere", "Baleful", "Bashful", "Bittersweet", "Blazing", "Blithe", "Bold", "Brazen", "Bristling", "Buoyant", "Cunning", "Capricious", "Calm", "Carnivorous", "Celestial", "Charming", "Chromatic", "Clever", "Cloaked", "Cold-blooded", "Colossal", "Cozy", "Cryptic", "Dainty", "Dauntless", "Deafening", "Deft", "Delicate", "Diminutive", "Dire", "Dapper", "Dashing", "Draconic", "Dreamlike", "Durable", "Eerie", "Elusive", "Elegant", "Endearing", "Enigmatic", "Ethereal", "Ferocious", "Fierce", "Fleet", "Fluffy", "Formidable", "Forsaken", "Frosty", "Furtive", "Gallant", "Gargantuan", "Gleaming", "Golden", "Graceful", "Grizzled", "Harmonious", "Hardy", "Hypnotic", "Imposing", "Incandescent", "Industrious", "Ingenious", "Intrepid", "Iridescent", "Jagged", "Jovial", "Keen", "Lithe", "Luminous", "Majestic", "Merciless", "Mottled", "Mystical", "Nocturnal", "Noble", "Omnivorous", "Ornate", "Outsized", "Pensive", "Placid", "Primal", "Prismatic", "Prowling", "Puny", "Radiant", "Regal", "Rugged", "Sinuous", "Sleek", "Spellbound", "Stealthy", "Striking", "Tenacious" };

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

  WriteOptions write_options;
  ReadOptions read_options;

  bool do_manual_compaction = argc > 1 && strcmp(argv[1], "--yes") == 0;

  // open DB
  Status s = DB::Open(options, kDBPath, &db);
  assert(s.ok());

  std::mt19937 mt(192839);

  const uint32_t N_TESTS = 100000;
  const UInt64Rectangle MBR(10, 10, UINT64_MAX - 10, UINT64_MAX - 10);

  std::deque<UInt64Point> entry_points;
  std::unordered_map<UInt64Point, std::string> entry_values;

  // load existing entries
  Iterator* iter(
    db->NewIterator(read_options));
  for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
    HilbertCode code = HilbertCode::FromString(iter->key().ToString());
    auto point = HilbertCodeToPoint(code);
    entry_points.push_back(point);
    entry_values[point] = iter->value().ToString();
  }
  delete iter;

  auto time_writes = std::chrono::nanoseconds(0);
  auto time_reads = std::chrono::nanoseconds(0);
  auto time_replaces = std::chrono::nanoseconds(0);
  auto time_deletes = std::chrono::nanoseconds(0);
  auto time_range = std::chrono::nanoseconds(0);
  uint32_t n_writes = 0;
  uint32_t n_reads = 0;
  uint32_t n_replaces = 0;
  uint32_t n_deletes = 0;
  uint32_t n_range = 0;
  uint32_t total_range_result_size = 0;
  #define START_CLOCK auto __start = std::chrono::steady_clock::now();
  #define END_CLOCK(time) auto __end = std::chrono::steady_clock::now(); \
    time += std::chrono::duration_cast<std::chrono::nanoseconds>(__end - __start);

  uint32_t tests_run = 0;
  #define OUTPUT_STATS { \
    printf("Queries run: %d write, %d read, %d replace, %d delete, %d range, %d total\n", n_writes, n_reads, n_replaces, n_deletes, n_range, tests_run); \
    printf("Average write time: %f ns (batch size is %d)\n", (double)time_writes.count() / n_writes, WRITE_BATCH_SIZE); \
    printf("Average read time: %f ns\n", (double)time_reads.count() / n_reads); \
    printf("Average replace time: %f ns\n", (double)time_replaces.count() / n_replaces); \
    printf("Average delete time: %f ns (batch size is %d)\n", (double)time_deletes.count() / n_deletes, DELETE_BATCH_SIZE); \
    printf("Average range query time: %f ns\n", (double)time_range.count() / n_range); \
    printf("Average range query time per result entry: %f ns\n", (double)time_range.count() / total_range_result_size); \
    printf("Average n\'o\'range result entries: %f\n", (double)total_range_result_size / n_range); \
    fflush(stdout); \
  }
  #define FAIL(fmt, ...) { \
    printf(fmt, __VA_ARGS__); \
    fflush(stdout); \
    OUTPUT_STATS; \
    assert(false); \
  }

  const uint32_t WRITE_BATCH_SIZE = 10;
  const uint32_t DELETE_BATCH_SIZE = 5;

  for (uint32_t test_i = 0; test_i < N_TESTS; ++test_i) {
    uint32_t query_type = mt() % 5;

    if (query_type == 0) {
      // write query
      WriteBatch batch;
      for (uint32_t i = 0; i < WRITE_BATCH_SIZE; ++i) {
        UInt64Point point(0, 0);
        while (true) {
          point = UInt64Point(mt() % (MBR.GetRight() - MBR.GetLeft() + 1) + MBR.GetLeft(),
                              mt() % (MBR.GetBottom() - MBR.GetTop() + 1) + MBR.GetTop());
          if (entry_values.find(point) == entry_values.end()) break;
        }
        std::string value = adjectives[mt() % adjectives.size()] + " " + animals[mt() % animals.size()];

        HilbertCode code = PointToHilbertCode(point);
        assert(HilbertCodeToPoint(code) == point);
        assert(HilbertCode::FromString(code.ToString()) == code);

        std::string key = code.ToString();
        batch.Put(Slice(key), Slice(value));
        entry_points.push_back(point);
        entry_values[point] = value;
      }

      START_CLOCK;
      s = db->Write(write_options, &batch);
      END_CLOCK(time_writes);
      n_writes += 1;

      if (!s.ok()) FAIL("Write query error: %s\n", s.ToString().c_str());
    } else if (query_type == 1) {
      // read query
      if (entry_points.empty()) continue;

      auto point = entry_points[mt() % entry_points.size()];
      auto expected_value = entry_values[point];
      std::string key = PointToHilbertCode(point).ToString();
      std::string received_value;

      START_CLOCK;
      s = db->Get(read_options, Slice(key), &received_value);
      END_CLOCK(time_reads);

      if (!s.ok()) {
        FAIL("Read query error: %s\n", s.ToString().c_str());
      } else if (expected_value != received_value) {
        FAIL("Read query error: expected \'%s\', got \'%s\'\n", expected_value.c_str(), received_value.c_str());
      }

      n_reads += 1;
    } else if (query_type == 2) {
      // replace query
      if (entry_points.empty())
        continue;

      WriteBatch batch;

      auto point = entry_points.front();
      entry_points.pop_front();
      std::string key = PointToHilbertCode(point).ToString();
      std::string value = adjectives[mt() % adjectives.size()] + " " + animals[mt() % animals.size()];
      batch.Put(Slice(key), Slice(value));
      entry_points.push_back(point);
      entry_values[point] = value;

      START_CLOCK;
      s = db->Write(write_options, &batch);
      END_CLOCK(time_replaces);

      if (!s.ok()) FAIL("Replace query error: %s\n", s.ToString().c_str());

      n_replaces += 1;
    } else if (query_type == 3) {
      // delete query
      if (entry_points.size() < DELETE_BATCH_SIZE)
        continue;

      WriteBatch batch;
      for (uint32_t i = 0; i < DELETE_BATCH_SIZE; ++i) {
        auto point = entry_points.front();
        entry_points.pop_front();
        entry_values.erase(point);
        std::string key = PointToHilbertCode(point).ToString();
        batch.Delete(Slice(key));
      }

      START_CLOCK;
      s = db->Write(write_options, &batch);
      END_CLOCK(time_deletes);

      if (!s.ok()) FAIL("Delete query error: %s\n", s.ToString().c_str());

      n_deletes += 1;
    } else if (query_type == 4) {
      // range query
      uint32_t left   = mt() % (MBR.GetRight() - MBR.GetLeft() + 1) + MBR.GetLeft();
      uint32_t right  = mt() % (MBR.GetRight() - MBR.GetLeft() + 1) + MBR.GetLeft();
      uint32_t top    = mt() % (MBR.GetBottom() - MBR.GetTop() + 1) + MBR.GetTop();
      uint32_t bottom = mt() % (MBR.GetBottom() - MBR.GetTop() + 1) + MBR.GetTop();
      if (right < left) std::swap(left, right);
      if (bottom < top) std::swap(top, bottom);

      UInt64Rectangle rect(left, top, right, bottom);

      START_CLOCK;
      auto result = db->RectangularRangeQuery(read_options, db->DefaultColumnFamily(), rect, &s);
      END_CLOCK(time_range);

      if (!s.ok()) FAIL("Range query error: %s\n", s.ToString().c_str());

      std::unordered_map<UInt64Point, std::string> result_map;
      for (auto [point, value]: result)
        result_map[point] = value;

      for (auto point: entry_points) {
        auto expected_value = entry_values[point];

        if (!rect.Contains(point)) {
          if (result_map.find(point) != result_map.end()) {
            FAIL("Range query error: point (%lu, %lu) mistakenly returned as in rectangle (%lu, %lu, %lu, %lu)\n",
              point.GetX(), point.GetY(),
              rect.GetLeft(), rect.GetTop(), rect.GetRight(), rect.GetBottom());
          }
        } else {
          if (result_map.find(point) == result_map.end()) {
            FAIL("Range query error: point (%lu, %lu) is in rectangle (%lu, %lu, %lu, %lu) but not returned by query\n",
              point.GetX(), point.GetY(),
              rect.GetLeft(), rect.GetTop(), rect.GetRight(), rect.GetBottom());
          }
          auto received_value = result_map[point];
          if (received_value != expected_value) {
            FAIL("Range query error: at point (%lu, %lu) expected \'%s\', got \'%s\'\n", point.GetX(), point.GetY(), expected_value.c_str(), received_value.c_str());
          }
        }
      }

      n_range += 1;
      total_range_result_size += result.size();
    }
    tests_run += 1;
    
    if (do_manual_compaction) {
      if ((test_i + 1) % (N_TESTS / 10) == 0) {
        std::string begin = HilbertCode(0, 0).ToString();
        std::string end = HilbertCode(0xffffffffffffffffull, 0xffffffffffffffffull).ToString();
        Slice begin_slice(begin), end_slice(end);
        db->CompactRange(CompactRangeOptions(), &begin_slice, &end_slice);

        db->WaitForCompact(WaitForCompactOptions());
      }
    }
  }

  OUTPUT_STATS;

  db.reset();

  std::filesystem::remove_all(kDBPath);

  return 0;
}
