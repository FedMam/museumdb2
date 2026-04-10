// TODO(FedMam): rewrite into Google Test
// TODO(FedMam): finish later

#include "hilbert/rtree.h"
#include "hilbert/rtree_builder.h"
#include "hilbert/rtree_dumper_reader.h"
#include "hilbert/geometry_test_util.h"

#include "rocksdb/rocksdb_namespace.h"

#include <assert.h>
#include <stdint.h>

#include <iostream>

namespace ROCKSDB_NAMESPACE {
namespace {

Env *const env = Env::Default();
const EnvOptions opts = EnvOptions();
const std::string test_file_name = "__temp__rtree_dumper_reader_test__";

inline std::unique_ptr<WritableFile> CreateWritableFile(const std::string& file_name) {
  std::unique_ptr<WritableFile> file;
  assert(env->NewWritableFile(file_name, &file, opts).ok());
  return file;
}

inline std::unique_ptr<RandomAccessFile> CreateRandomAccessFile(const std::string& file_name) {
  std::unique_ptr<RandomAccessFile> file;
  assert(env->NewRandomAccessFile(file_name, &file, opts).ok());
  return file;
}

void TEST_Smoke() {
  VarLenPoint2D point1(
    VarLenNumber(1, 0x10u), VarLenNumber(1, 0x10u)
  ), point2(
    VarLenNumber(1, 0x30u), VarLenNumber(1, 0x10u)
  ), point3(
    VarLenNumber(1, 0x10u), VarLenNumber(1, 0x30u)
  ), point4(
    VarLenNumber(1, 0x30u), VarLenNumber(1, 0x30u)
  ), point5(
    VarLenNumber(1, 0x20u), VarLenNumber(1, 0x20u)
  );

  RTreeBuilder<uint32_t> tree_builder(1);
  tree_builder.Add(VarLenPoint2DWithHilbertValue(point1), 1u);
  tree_builder.Add(VarLenPoint2DWithHilbertValue(point2), 2u);
  tree_builder.Add(VarLenPoint2DWithHilbertValue(point3), 3u);
  tree_builder.Add(VarLenPoint2DWithHilbertValue(point4), 4u);
  tree_builder.Add(VarLenPoint2DWithHilbertValue(point5), 5u);

  RTree<uint32_t> tree = tree_builder.Build(2, 2);

  {
  auto out_file = CreateWritableFile(test_file_name);
  assert(RTreeDumper::Dump(tree, *out_file).ok());
  }

  {
  auto in_file = CreateRandomAccessFile(test_file_name);
  Status s;
  RTreeReader reader(*in_file, &s);
  assert(s.ok());

  auto find_result = reader.Find(point1, &s).value();
  assert(find_result.GetPointWithoutHilbertValue() == point1);
  assert(find_result.GetItem() == 1u);
  find_result = reader.Find(point2, &s).value();
  assert(find_result.GetPointWithoutHilbertValue() == point2);
  assert(find_result.GetItem() == 2u);
  find_result = reader.Find(point3, &s).value();
  assert(find_result.GetPointWithoutHilbertValue() == point3);
  assert(find_result.GetItem() == 3u);
  find_result = reader.Find(point4, &s).value();
  assert(find_result.GetPointWithoutHilbertValue() == point4);
  assert(find_result.GetItem() == 4u);
  find_result = reader.Find(point5, &s).value();
  assert(find_result.GetPointWithoutHilbertValue() == point5);
  assert(find_result.GetItem() == 5u);
  assert(s.ok());

  auto range_result = reader.RangeQuery(VarLenRectangle(
    VarLenNumber(1, 0x0fu), VarLenNumber(1, 0x0fu),
    VarLenNumber(1, 0x31u), VarLenNumber(1, 0x1fu)
  ), &s);
  assert(s.ok());
  assert(ItemInRangeResult(range_result, 1u, point1));
  assert(ItemInRangeResult(range_result, 2u, point2));
  assert(!ItemInRangeResult(range_result, 3u, point3));
  assert(!ItemInRangeResult(range_result, 4u, point4));
  assert(!ItemInRangeResult(range_result, 5u, point5));

  range_result = reader.RangeQuery(VarLenRectangle(
    VarLenNumber(1, 0x0fu), VarLenNumber(1, 0x1fu),
    VarLenNumber(1, 0x31u), VarLenNumber(1, 0x31u)
  ), &s);
  assert(s.ok());
  assert(!ItemInRangeResult(range_result, 1u, point1));
  assert(!ItemInRangeResult(range_result, 2u, point2));
  assert(ItemInRangeResult(range_result, 3u, point3));
  assert(ItemInRangeResult(range_result, 4u, point4));
  assert(ItemInRangeResult(range_result, 5u, point5));
  }

  env->DeleteFile(test_file_name);
}

}
}

#define TEST(test_name) { ROCKSDB_NAMESPACE::TEST_ ## test_name(); std::cout << ("=== " #test_name " passed successfully") << std::endl; }

int main(void) {
  TEST(Smoke);
  return 0;
}
