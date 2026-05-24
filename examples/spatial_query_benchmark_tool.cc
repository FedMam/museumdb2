// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include <iostream>
#include <fstream>
#include <sstream>
#include <functional>
#include <iomanip>
#include <filesystem>
#include <memory>
#include <string>
#include <random>
#include <vector>
#include <chrono>
#include <cmath>

#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"

#include "table/hilbert/hilbert_table_factory.h"

using namespace ROCKSDB_NAMESPACE;

#if defined(OS_WIN)
std::string kDBPath = "C:\\Windows\\TEMP\\rocksdb_spatial_query_benchmark_tool";
#else
std::string kDBPath = "/tmp/rocksdb_spatial_query_benchmark_tool";
#endif

class CompactionTimeMeasureListener: public EventListener {
 public:
  void OnFlushBegin(DB* db, const FlushJobInfo& fi) override {
    (void)db;
    (void)fi;
    
    if (loading_dataset_now) ++n_flushes_loading;
    else ++n_flushes;
  }

  void OnCompactionBegin(DB* db, const CompactionJobInfo& ci) override {
    (void)db;
    (void)ci;
    
    if (loading_dataset_now) ++n_compactions_loading;
    else ++n_compactions;
  }

  bool loading_dataset_now = true;
  int n_flushes_loading = 0;
  int n_compactions_loading = 0;
  int n_flushes = 0;
  int n_compactions = 0;
};

int main(int argc, char** argv) {
  // === READING ARGS ===
  if (argc < 3) {
    std::cout<<"Usage: spatial_query_benchmark_tool <dataset.tsv> <output.tsv>"<<std::endl;
    std::cout<<"dataset.tsv format: no header, each row: X\\tY"<<std::endl;
    std::cout<<"output.tsv will store info about all queries. Format: no header, each row: <query-type>\\t<time>\\t<parameters>, possible query types:"<<std::endl;
    std::cout<<"  read\\t<time>\\t<X>\\t<Y>"<<std::endl;
    std::cout<<"  write\\t<time>\\t<N-writes>\\t<X1>\\t<Y1>\\t<value1>\\t<X2>\\t<Y2>\\t<value2>\\t..."<<std::endl;
    std::cout<<"  delete\\t<time>\\t<N-deletes>\\t<X1>\\t<Y1>\\t<X2>\\t<Y2>\\t..."<<std::endl;
    std::cout<<"  range\\t<time>\\t<left>\\t<top>\\t<bottom>\\t<N-entries-in-range>"<<std::endl;
    exit(1);
  }

  std::string dataset_file_name = argv[1];
  std::string output_file_name = argv[2];

  // === CONSTANTS ===
  // for generating random values
  const std::vector<std::string> animals = { "Lion", "Tiger", "Elephant", "Giraffe", "Zebra", "Rhino", "Hippo", "Gorilla", "Orangutan", "Chimpanzee", "Kangaroo", "Koala", "Sloth", "Panda", "Moose", "Deer", "Fox", "Wolf", "Coyote", "Bear", "Otter", "Beaver", "Squirrel", "Hare", "Rabbit", "Hedgehog", "Badger", "Skunk", "Raccoon", "Seal", "Walrus", "Dolphin", "Whale", "Porpoise", "Shark", "Tuna", "Salmon", "Trout", "Carp", "Catfish", "Pike", "Mackerel", "Marlin", "Seahorse", "Jellyfish", "Octopus", "Squid", "Crab", "Lobster", "Shrimp", "Butterfly", "Moth", "Bee", "Wasp", "Ant", "Dragonfly", "Grasshopper", "Locust", "Beetle", "Ladybug", "Firefly", "Cicada", "Spider", "Scorpion", "Snake", "Cobra", "Viper", "Python", "Anaconda", "Turtle", "Tortoise", "Iguana", "Gecko", "Chameleon", "Alligator", "Crocodile", "Falcon", "Hawk", "Eagle", "Vulture", "Owl", "Sparrow", "Robin", "Finch", "Pigeon", "Dove", "Parrot", "Cockatoo", "Macaw", "Flamingo", "Pelican", "Heron", "Stork", "Crane", "Swan", "Goose", "Duck", "Turkey", "Chicken", "Peacock", "Emu", "Ostrich", "Penguin", "Platypus" };
  const std::vector<std::string> adjectives = { "Agile", "Alert", "Ancient", "Armored", "Astonishing", "Attentive", "Audacious", "Austere", "Baleful", "Bashful", "Bittersweet", "Blazing", "Blithe", "Bold", "Brazen", "Bristling", "Buoyant", "Cunning", "Capricious", "Calm", "Carnivorous", "Celestial", "Charming", "Chromatic", "Clever", "Cloaked", "Cold-blooded", "Colossal", "Cozy", "Cryptic", "Dainty", "Dauntless", "Deafening", "Deft", "Delicate", "Diminutive", "Dire", "Dapper", "Dashing", "Draconic", "Dreamlike", "Durable", "Eerie", "Elusive", "Elegant", "Endearing", "Enigmatic", "Ethereal", "Ferocious", "Fierce", "Fleet", "Fluffy", "Formidable", "Forsaken", "Frosty", "Furtive", "Gallant", "Gargantuan", "Gleaming", "Golden", "Graceful", "Grizzled", "Harmonious", "Hardy", "Hypnotic", "Imposing", "Incandescent", "Industrious", "Ingenious", "Intrepid", "Iridescent", "Jagged", "Jovial", "Keen", "Lithe", "Luminous", "Majestic", "Merciless", "Mottled", "Mystical", "Nocturnal", "Noble", "Omnivorous", "Ornate", "Outsized", "Pensive", "Placid", "Primal", "Prismatic", "Prowling", "Puny", "Radiant", "Regal", "Rugged", "Sinuous", "Sleek", "Shocked", "Stealthy", "Striking", "Tenacious" };

  const int N_QUERIES = 50'000;
  const int LOAD_WRITE_BATCH_SIZE = 1000;
  const int WRITE_BATCH_SIZE = 50;
  const int DELETE_BATCH_SIZE = 10;

  std::mt19937 mt(std::hash<std::string>{}(dataset_file_name));
  std::uniform_int_distribution<size_t> adj_dist(0, adjectives.size() - 1);
  std::uniform_int_distribution<size_t> ani_dist(0, animals.size() - 1);
  std::mt19937_64 mt64(~std::hash<std::string>{}(dataset_file_name));

  // === CREATING DATABASE ===
  std::unique_ptr<DB> db;
  Options options;
  options.IncreaseParallelism();
  options.OptimizeLevelStyleCompaction();
  options.create_if_missing = true;

  options.spatial_data = true;
  auto hilbert_factory = std::make_shared<HilbertTableFactory>(BlockBasedTableOptions());
  options.table_factory = hilbert_factory;

  std::shared_ptr<CompactionTimeMeasureListener> compaction_listener = std::make_shared<CompactionTimeMeasureListener>();
  options.listeners.push_back(compaction_listener);

  options.write_buffer_size = 524288;

  WriteOptions write_options;
  ReadOptions read_options;

  Status s = DB::Open(options, kDBPath, &db);
  if (!s.ok()) {
    std::cerr<<"Error opening database: "<<s.ToString()<<std::endl;
    exit(255);
  }

  // === LOADING DATASET ===
  std::vector<HilbertCode> entries;

  {
  WriteBatch batch;
  std::ifstream dataset_file(dataset_file_name, std::ios::in);
  if (dataset_file.fail()) {
    std::cerr<<"Error opening file: "<<dataset_file_name<<std::endl;
    exit(255);
  }

  std::string dataset_line;
  while (std::getline(dataset_file, dataset_line)) {
    if (dataset_line.empty() || dataset_line == "\n" || dataset_line == "\r\n")
      continue;
    std::string dataset_line_copy = dataset_line;

    std::stringstream ss(dataset_line);
    std::string x_string, y_string;
    std::getline(ss, x_string, '\t');
    std::getline(ss, y_string);

    if (x_string.empty() || y_string.empty()) {
      dataset_file.close();
      std::cerr<<"Error parsing line `"<<dataset_line_copy<<"` in "<<dataset_file_name<<std::endl;
      exit(255);
    }

    uint64_t x, y;
    try {
      x = std::stoull(x_string);
      y = std::stoull(y_string);
    } catch (...) {
      dataset_file.close();
      std::cerr<<"Error parsing line `"<<dataset_line_copy<<"` in "<<dataset_file_name<<std::endl;
      exit(255);
    }

    UInt64Point point(x, y);
    auto hilbert_code = PointToHilbertCode(point);
    entries.push_back(hilbert_code);

    std::string key = hilbert_code.ToString();
    std::string value = adjectives[adj_dist(mt)] + " " + animals[ani_dist(mt)];
    batch.Put(Slice(key), Slice(value));

    if (batch.Count() == LOAD_WRITE_BATCH_SIZE) {
      s = db->Write(write_options, &batch);
      if (!s.ok()) {
        dataset_file.close();
        std::cerr<<"Write error on loading dataset: "<<s.ToString()<<std::endl;
        exit(255);
      }
      batch.Clear();
    }
  }

  s = db->Write(write_options, &batch);
  if (!s.ok()) {
    dataset_file.close();
    std::cerr<<"Write error on loading dataset: "<<s.ToString()<<std::endl;
    exit(255);
  }

  compaction_listener->loading_dataset_now = false;
  }

  // === PERFORMING QUERIES ===
  std::vector<double> time_read;
  std::vector<double> time_write;
  std::vector<double> time_delete;
  std::vector<double> time_range;
  std::vector<size_t> num_entries_range;

  std::uniform_int_distribution<size_t> dist(0, entries.size() - 1);

  #define START_CLOCK auto __start = std::chrono::steady_clock::now();
  #define END_CLOCK \
    auto __end = std::chrono::steady_clock::now(); \
    auto __dur = std::chrono::duration_cast<std::chrono::nanoseconds>(__end - __start); \
    double __time = (double)(__dur.count()) / 1e+9;  // in seconds

  std::vector<std::string> queries_info;

  for (int query_i = 0; query_i < N_QUERIES; ++query_i) {
    uint32_t query_type = mt() % 4;

    std::ostringstream query_info;
    query_info<<std::fixed<<std::setprecision(9);

    if (query_type == 0) {
      // read
      auto code = entries[dist(mt)];
      std::string key = code.ToString();
      std::string value;

      START_CLOCK;
      s = db->Get(read_options, Slice(key), &value);
      END_CLOCK;

      if (!s.ok() && !s.IsNotFound()) {
        std::cerr<<"Read query error: "<<s.ToString()<<std::endl;
        exit(255);
      }

      time_read.push_back(__time);

      auto point = HilbertCodeToPoint(code);
      query_info<<"read\t"<<__time<<"\t"<<point.GetX()<<"\t"<<point.GetY();

    } else if (query_type == 1) {
      // write
      WriteBatch batch;

      std::vector<std::pair<UInt64Point, std::string>> writes_info;

      for (int entry_i = 0; entry_i < WRITE_BATCH_SIZE; ++entry_i) {
        auto code = entries[dist(mt)];
        std::string key = code.ToString();
        std::string value = adjectives[adj_dist(mt)] + " " + animals[ani_dist(mt)];

        batch.Put(Slice(key), Slice(value));
        writes_info.push_back({HilbertCodeToPoint(code), value});
      }

      START_CLOCK;
      s = db->Write(write_options, &batch);
      END_CLOCK;

      if (!s.ok()) {
        std::cerr<<"Write query error: "<<s.ToString()<<std::endl;
        exit(255);
      }

      time_write.push_back(__time);

      query_info<<"write\t"<<__time<<"\t"<<WRITE_BATCH_SIZE;
      for (const auto& [point, value]: writes_info)
        query_info<<"\t"<<point.GetX()<<"\t"<<point.GetY()<<"\t"<<value;

    } else if (query_type == 2) {
      // delete
      WriteBatch batch;

      std::vector<UInt64Point> deletes_info;

      for (int entry_i = 0; entry_i < DELETE_BATCH_SIZE; ++entry_i) {
        auto code = entries[dist(mt)];
        std::string key = code.ToString();

        batch.Delete(Slice(key));
        deletes_info.push_back(HilbertCodeToPoint(code));
      }

      START_CLOCK;
      s = db->Write(write_options, &batch);
      END_CLOCK;

      if (!s.ok()) {
        std::cerr<<"Delete query error: "<<s.ToString()<<std::endl;
        exit(255);
      }

      time_delete.push_back(__time);

      query_info<<"delete\t"<<__time<<"\t"<<DELETE_BATCH_SIZE;
      for (const auto& point: deletes_info)
        query_info<<"\t"<<point.GetX()<<"\t"<<point.GetY();

    } else if (query_type == 3) {
      // range
      auto code_entry_c = entries[dist(mt)];
      auto point_c = HilbertCodeToPoint(code_entry_c);
      auto x_c = point_c.GetX(), y_c = point_c.GetY();

      uint64_t x_size, y_size;
      if (mt() % (N_QUERIES / 200) != 0) {
        x_size = mt64() % (1ull << 30);
        y_size = mt64() % (1ull << 30);
      } else {
        x_size = mt64() % (1ull << 62);
        y_size = mt64() % (1ull << 62);
      }

      uint64_t left = (x_c >= x_size / 2) ? (x_c - x_size / 2) : 0;
      uint64_t top  = (y_c >= y_size / 2) ? (y_c - y_size / 2) : 0;
      uint64_t right = (left <= UINT64_MAX - x_size) ? (left + x_size) : UINT64_MAX;
      uint64_t bottom = (top <= UINT64_MAX - y_size) ? (top + y_size) : UINT64_MAX;

      UInt64Rectangle rect(left, top, right, bottom);

      START_CLOCK;
      auto range_result = db->RectangularRangeQuery(
        read_options, db->DefaultColumnFamily(), rect, &s);
      END_CLOCK;

      if (!s.ok()) {
        std::cerr<<"Range query error: "<<s.ToString()<<std::endl;
        exit(255);
      }

      time_range.push_back(__time);
      num_entries_range.push_back(range_result.size());

      query_info<<"range\t"<<__time<<"\t"<<left<<"\t"<<top<<"\t"<<right<<"\t"<<bottom<<"\t"<<range_result.size();
    }

    queries_info.push_back(query_info.str());
  }

  s = db->WaitForCompact(WaitForCompactOptions());
  if (!s.ok()) {
    std::cerr<<"WaitForCompact error: "<<s.ToString()<<std::endl;
    exit(255);
  }

  // === OUTPUT STATS ===
  std::cout<<std::fixed<<std::setprecision(9);

  std::cout<<"=== SUMMARY ==="<<std::endl;
  std::cout<<N_QUERIES<<" queries total"<<std::endl;
  std::cout<<"WriteBatch in write queries has "<<WRITE_BATCH_SIZE<<" updates"<<std::endl;
  std::cout<<"WriteBatch in delete queries has "<<DELETE_BATCH_SIZE<<" updates"<<std::endl;

  std::cout<<compaction_listener->n_flushes_loading<<" flushes occurred while loading dataset"<<std::endl;
  std::cout<<compaction_listener->n_compactions_loading<<" compactions occurred while loading dataset"<<std::endl;
  std::cout<<compaction_listener->n_flushes<<" flushes occurred while performing queries"<<std::endl;
  std::cout<<compaction_listener->n_compactions<<" compactions occurred while performing queries"<<std::endl;

  std::cout<<"=== QUERY EXECUTION TIME SUMMARY ==="<<std::endl;

  #define OUTPUT_SUMMARY(time_data, description) {\
    std::cout<<(description)<<":\t"; \
    \
    double time_sum = 0.0; \
    for (size_t i = 0; i < (time_data).size(); ++i) time_sum += (time_data)[i]; \
    double time_mean = time_sum / (time_data).size(); \
    double time_sqsum = 0.0; \
    for (size_t i = 0; i < (time_data).size(); ++i) time_sqsum += ((time_data)[i] - time_mean) * ((time_data)[i] - time_mean); \
    double time_std = std::sqrt(time_sqsum / ((time_data).size() - 1)); \
    \
    std::cout<<" mean = "<<time_mean<<" s, std = "<<time_std<<" s, "<<(time_data).size()<<" total"<<std::endl; \
  }

  OUTPUT_SUMMARY(time_read, "Read query");
  OUTPUT_SUMMARY(time_write, "Write query");
  OUTPUT_SUMMARY(time_delete, "Delete query");
  OUTPUT_SUMMARY(time_range, "Range query");

  std::cout<<"=== RANGE QUERY TIME DETAILS ==="<<std::endl;

  #define RANGE_QUERY_DETAILS(range_low, range_high) {\
    std::vector<double> time_data; \
    for (size_t i = 0; i < time_range.size(); ++i) { \
      if ((range_low) <= num_entries_range[i] && num_entries_range[i] <= (range_high)) \
        time_data.push_back(time_range[i]); \
    } \
    if (!time_data.empty()) {\
      OUTPUT_SUMMARY(time_data, \
        std::string("Range query (") + \
          std::to_string(range_low) + \
          ((range_low != range_high) ? (std::string("-") + std::to_string(range_high)) : std::string("")) + \
          " entries in range)"); \
    } }

  RANGE_QUERY_DETAILS(0, 0);
  RANGE_QUERY_DETAILS(1, 9);
  RANGE_QUERY_DETAILS(10, 99);
  RANGE_QUERY_DETAILS(100, 999);
  RANGE_QUERY_DETAILS(1000, 9999);
  RANGE_QUERY_DETAILS(10000, 99999);
  RANGE_QUERY_DETAILS(100000, 999999);
  RANGE_QUERY_DETAILS(1000000, 9999999);

  // === FINISH ===

  db.reset();

  std::filesystem::remove_all(kDBPath);

  // === WRITE QUERY INFO ===

  std::ofstream output_file(output_file_name, std::ios::out | std::ios::trunc);
  if (output_file.fail()) {
    std::cerr<<"Error writing query info to file: "<<output_file_name<<std::endl;
    exit(255);
  }

  for (const auto& row: queries_info) {
    output_file<<row<<std::endl;

    if (output_file.fail()) {
      std::cerr<<"Error writing query info: `"<<row<<"` to file: "<<output_file_name<<std::endl;
      exit(255);
    }
  }
  output_file.close();

  return 0;
}
