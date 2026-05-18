#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <chrono>
#include <filesystem>

#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"

#include "table/hilbert/hilbert_table_factory.h"

using namespace ROCKSDB_NAMESPACE;

#if defined(OS_WIN)
std::string kDBPath = "C:\\Windows\\TEMP\\rocksdb_hilbert_spatial_query_benchmark_tool";
#else
std::string kDBPath = "/tmp/rocksdb_hilbert_spatial_query_benchmark_tool";
#endif

// This tool accepts two CSV files: one with spatial data and one
// describing rectangular queries needed to be performed on that data
// (see usage for details) and measures the average time of each 
// query (in seconds) and also number of entries inside the rectangle.

// parse a CSV arg with surrounding quotes and double quotes
std::string parse_csv(const std::string& csv_arg) {
  std::string new_arg = csv_arg;
  
  if (new_arg.ends_with("\r\n")) {
      new_arg = new_arg.substr(0, new_arg.size() - 2);
  } else if (new_arg.ends_with("\n")) {
    new_arg = new_arg.substr(0, new_arg.size() - 1);
  }
  
  if (!(new_arg.starts_with('"')) && !new_arg.ends_with('"'))
    return new_arg;

  new_arg = new_arg.substr(1, new_arg.length() - 2);

  size_t start_pos = 0;
  while((start_pos = new_arg.find("\"\"", start_pos)) != std::string::npos) {
    new_arg.replace(start_pos, 2, "\"");
    ++start_pos;
  }

  return new_arg;
}

int main(int argc, char** argv) {
  if (argc < 4) {
    std::cout<<"Usage: spatial_query_benchmark_tool <dataset.csv> <queries.csv> <output.csv>"<<std::endl;
    std::cout<<"dataset.csv format: header: `X,Y,Data`, separator: `,`, quotechar: `\"`"<<std::endl;
    std::cout<<"queries.csv format: header: `Id,Left,Top,Right,Bottom`, separator: `,`, quotechar: `\"`"<<std::endl;
    exit(1);
  }

  std::string dataset_file_name = argv[1];
  std::string queries_file_name = argv[2];
  std::string out_file_name = argv[3];

  const int WRITE_BATCH_MAX_COUNT = 10000;
  const int N_EXPERIMENTS = 50;

  std::unique_ptr<DB> db;
  Options options;
  options.IncreaseParallelism();
  options.OptimizeLevelStyleCompaction();
  options.create_if_missing = true;

  options.spatial_data = true;
  auto hilbert_factory = std::make_shared<HilbertTableFactory>(BlockBasedTableOptions());
  options.table_factory = hilbert_factory;

  WriteOptions write_options;
  ReadOptions read_options;

  Status s = DB::Open(options, kDBPath, &db);
  if (!s.ok()) {
    std::cerr<<"Error opening database: "<<s.ToString()<<std::endl;
    exit(255);
  }

  // === LOADING DATASET ===

  {
  std::ifstream dataset_file(dataset_file_name, std::ios::in);
  if (dataset_file.fail()) {
    std::cerr<<"Error opening file"<<dataset_file_name<<std::endl;
    exit(255);
  }

  std::string dataset_line;
  std::getline(dataset_file, dataset_line);  // header
  WriteBatch batch;
  while (std::getline(dataset_file, dataset_line)) {
    dataset_line = parse_csv(dataset_line);

    if (dataset_line.empty())
      continue;
    std::string dataset_line_copy = dataset_line;

    std::stringstream ss(dataset_line);

    std::string x_string, y_string, data;
    std::getline(ss, x_string, ',');
    std::getline(ss, y_string, ',');
    std::getline(ss, data);

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
    HilbertCode code = PointToHilbertCode(point);
    std::string key_string = code.ToString();
    Slice key(key_string), value(data);
    batch.Put(key, value);

    if (batch.Count() == WRITE_BATCH_MAX_COUNT) {
      s = db->Write(write_options, &batch);

      if (!s.ok()) {
        dataset_file.close();
        std::cerr<<"Write error: "<<s.ToString()<<std::endl;
        exit(255);
      }

      batch.Clear();
    }
  }

  s = db->Write(write_options, &batch);
  if (!s.ok()) {
    dataset_file.close();
    std::cerr<<"Write error: "<<s.ToString()<<std::endl;
    exit(255);
  }

  dataset_file.close();
  }

  // === COMPACTION ===

  {
  CompactRangeOptions cro;
  WaitForCompactOptions wfco;

  std::string first_key_string = HilbertCode(0, 0).ToString();
  std::string last_key_string = HilbertCode(UINT64_MAX, UINT64_MAX).ToString();
  Slice first_key(first_key_string), last_key(last_key_string);
  
  s = db->CompactRange(cro, &first_key, &last_key);
  if (!s.ok()) {
    std::cerr<<"Compaction error: "<<s.ToString()<<std::endl;
    exit(255);
  }

  s = db->WaitForCompact(wfco);
  if (!s.ok()) {
    std::cerr<<"WaitForCompact error: "<<s.ToString()<<std::endl;
    exit(255);
  }

  }

  // === LOADING QUERIES ===
  std::vector<UInt64Rectangle> queries;
  std::vector<std::vector<std::chrono::nanoseconds>> query_time;
  std::vector<uint32_t> query_num_entries;

  {
  std::ifstream queries_file(queries_file_name, std::ios::in);
  if (queries_file.fail()) {
    std::cerr<<"Error opening file "<<dataset_file_name<<std::endl;
    exit(255);
  }

  std::string queries_line;
  std::getline(queries_file, queries_line);  // header
  while (std::getline(queries_file, queries_line)) {
    if (queries_line.empty())
      continue;
    std::string queries_line_copy = queries_line;

    std::stringstream ss(queries_line);

    std::string id_string, left_string, top_string, right_string, bottom_string;
    std::getline(ss, id_string, ',');
    std::getline(ss, left_string, ',');
    std::getline(ss, top_string, ',');
    std::getline(ss, right_string, ',');
    std::getline(ss, bottom_string, ',');

    if (ss.fail()) {
      queries_file.close();
      std::cerr<<"Error parsing line `"<<queries_line_copy<<"` in "<<queries_file_name<<std::endl;
      exit(255);
    }
    
    size_t id;
    uint64_t left, top, right, bottom;
    try {
      id = std::stoul(id_string);
      left = std::stoull(left_string);
      top = std::stoull(top_string);
      right = std::stoull(right_string);
      bottom = std::stoull(bottom_string);
    } catch (...) {
      queries_file.close();
      std::cerr<<"Error parsing line `"<<queries_line_copy<<"` in "<<queries_file_name<<std::endl;
      exit(255);
    }

    if (id != queries.size()) {
      queries_file.close();
      std::cerr<<"Error: query IDs in file "<<queries_file_name<<" should be sequential numbers starting from 0"<<std::endl;
      exit(255);
    }

    queries.push_back(UInt64Rectangle(left, top, right, bottom));
    query_time.push_back(std::vector<std::chrono::nanoseconds>());
  }
  
  queries_file.close();
  }

  // === PERFORMING QUERIES ===

  for (int experiment_i = 0; experiment_i < N_EXPERIMENTS; ++experiment_i) {
    for (size_t query_id = 0; query_id < queries.size(); ++query_id) {
      UInt64Rectangle query_rect = queries[query_id];

      auto clock_start = std::chrono::steady_clock::now();
      auto query_result = db->RectangularRangeQuery(
        read_options, db->DefaultColumnFamily(), query_rect, &s);
      auto clock_end = std::chrono::steady_clock::now();

      auto query_duration = std::chrono::duration_cast<std::chrono::nanoseconds>(clock_end - clock_start);
      query_time[query_id].push_back(query_duration);

      if (query_num_entries.size() == query_id) {
        query_num_entries.push_back(query_result.size());
      } else {
        assert(query_result.size() == query_num_entries[query_id]);
      }
    }
  }

  // === WRITING RESULTS ===

  std::ofstream out_file(out_file_name, std::ios::out | std::ios::trunc);
  if (out_file.fail()) {
    std::cerr<<"Error creating file "<<out_file_name<<std::endl;
    exit(255);
  }

  out_file<<std::fixed<<std::setprecision(9);

  out_file<<"QueryId,NumEntries,Average";
  for (int experiment_i = 0; experiment_i < N_EXPERIMENTS; ++experiment_i)
    out_file<<",Experiment"<<experiment_i;
  out_file<<std::endl;

  for (size_t query_id = 0; query_id < queries.size(); ++query_id) {
    out_file<<query_id<<","<<query_num_entries[query_id];

    double query_time_sum = 0.0;
    for (int experiment_i = 0; experiment_i < N_EXPERIMENTS; ++experiment_i) {
      query_time_sum += (double)query_time[query_id][experiment_i].count() / 1e+9;
    }
    out_file<<","<<(query_time_sum / N_EXPERIMENTS);

    for (int experiment_i = 0; experiment_i < N_EXPERIMENTS; ++experiment_i) {
      out_file<<","<<((double)query_time[query_id][experiment_i].count() / 1e+9);
    }

    out_file<<std::endl;

    if (out_file.fail()) {
      out_file.close();
      std::cerr<<"Error writing to file "<<out_file_name<<std::endl;
      exit(255);
    }
  }

  // === CLEAN UP ===

  db.reset();
  
  std::filesystem::remove_all(kDBPath);

  return 0;
}
