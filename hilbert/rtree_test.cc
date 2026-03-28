// TODO(FedMam): rewrite into Google Test

#include "hilbert/rtree.h"

#include "rocksdb/rocksdb_namespace.h"

#include <assert.h>
#include <stdint.h>

#include <iostream>
#include <vector>
#include <random>

// DEBUG
#define DEBUGOK {std::cout<<"ok"<<__LINE__<<std::endl;}

namespace ROCKSDB_NAMESPACE {
namespace {

std::vector<std::string> fruits = {"Apple", "Banana", "Orange", "Mango", "Pineapple", "Strawberry", "Watermelon", "Cherry", "Grape", "Kiwi", "Peach", "Pear", "Plum", "Apricot", "Cantaloupe", "Pomegranate", "Dragon Fruit", "Persimmon", "Coconut", "Guava"};

// Helper function to quickly test a range query result
// in which items may be stored in arbitrary order.
template<typename TItem>
inline bool ItemInRangeResult(const std::vector<RTreeEntry<TItem>>& range_result, const TItem* item, const VarLenPoint2D& expected_point) {
  for (auto entry: range_result)
    if (entry.GetObjectPtr() == item && entry.GetPointWithoutHilbertValue() == expected_point)
      return true;
  return false;
}

void TEST_Smoke() {
  RTree<std::string> tree(1, 2, 2);
  
  VarLenPoint2D point0(
    VarLenNumber(1, 0x10u), VarLenNumber(1, 0x10u)
  ), point1(
    VarLenNumber(1, 0x30u), VarLenNumber(1, 0x10u)
  ), point2(
    VarLenNumber(1, 0x10u), VarLenNumber(1, 0x30u)
  ), point3(
    VarLenNumber(1, 0x30u), VarLenNumber(1, 0x30u)
  ), point4(
    VarLenNumber(1, 0x20u), VarLenNumber(1, 0x20u)
  );

  tree.VerifyIntegrity();
  tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point0), &fruits[0]);
  tree.VerifyIntegrity();
  tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point1), &fruits[1]);
  tree.VerifyIntegrity();
  tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point2), &fruits[2]);
  tree.VerifyIntegrity();
  tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point3), &fruits[3]);
  tree.VerifyIntegrity();
  tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point4), &fruits[0]); // note that 0, not 4
  tree.VerifyIntegrity();

  auto find_result = tree.Find(point0).value();
  assert(find_result.GetPointWithoutHilbertValue() == point0);
  assert(find_result.GetObjectPtr() == &fruits[0]);
  find_result = tree.Find(point1).value();
  assert(find_result.GetPointWithoutHilbertValue() == point1);
  assert(find_result.GetObjectPtr() == &fruits[1]);
  find_result = tree.Find(point2).value();
  assert(find_result.GetPointWithoutHilbertValue() == point2);
  assert(find_result.GetObjectPtr() == &fruits[2]);
  find_result = tree.Find(point3).value();
  assert(find_result.GetPointWithoutHilbertValue() == point3);
  assert(find_result.GetObjectPtr() == &fruits[3]);
  find_result = tree.Find(point4).value();
  assert(find_result.GetPointWithoutHilbertValue() == point4);
  assert(find_result.GetObjectPtr() == &fruits[4]);

  VarLenPoint2D point5(
    VarLenNumber(1, 0x50u), VarLenNumber(1, 0x50u)
  );
  assert(!tree.Find(point5).has_value());

  std::vector<RTreeEntry<std::string>> range_result;

  range_result = tree.RangeQuery(VarLenRectangle(
    VarLenNumber(1, 0x10u), VarLenNumber(1, 0x10u),
    VarLenNumber(1, 0x30u), VarLenNumber(1, 0x18u)
  ));
  assert(range_result.size() == 2);
  assert(ItemInRangeResult(range_result, &fruits[0], point0));
  assert(ItemInRangeResult(range_result, &fruits[1], point1));

  range_result = tree.RangeQuery(VarLenRectangle(
    VarLenNumber(1, 0x10u), VarLenNumber(1, 0x18u),
    VarLenNumber(1, 0x30u), VarLenNumber(1, 0x30u)
  ));
  assert(range_result.size() == 3);
  assert(ItemInRangeResult(range_result, &fruits[2], point2));
  assert(ItemInRangeResult(range_result, &fruits[3], point3));
  assert(ItemInRangeResult(range_result, &fruits[0], point4));

  range_result = tree.RangeQuery(VarLenRectangle(
    VarLenNumber(1, 0x0fu), VarLenNumber(1, 0x11u),
    VarLenNumber(1, 0x50u), VarLenNumber(1, 0x1fu)
  ));
  assert(range_result.size() == 0);

  tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point5), &fruits[5]);
  tree.VerifyIntegrity();
  find_result = tree.Find(point5).value();
  assert(find_result.GetObjectPtr() == &fruits[5]);
  assert(find_result.GetPointWithoutHilbertValue() == point5);

  tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point2), &fruits[7]);
  tree.VerifyIntegrity();
  find_result = tree.Find(point2).value();
  assert(find_result.GetObjectPtr() == &fruits[7]);
  assert(find_result.GetPointWithoutHilbertValue() == point2);

  tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point2), &fruits[2]);
  tree.VerifyIntegrity();
  find_result = tree.Find(point2).value();
  assert(find_result.GetObjectPtr() == &fruits[2]);
  assert(find_result.GetPointWithoutHilbertValue() == point2);
}

}
}

#define TEST(test_name) { ROCKSDB_NAMESPACE::TEST_ ## test_name(); std::cout << ("=== " #test_name " passed successfully") << std::endl; }

int main(void) {
  TEST(Smoke);
  return 0;
}

