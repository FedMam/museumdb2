// TODO(FedMam): rewrite into Google Test

#include "hilbert/rtree.h"

#include "rocksdb/rocksdb_namespace.h"

#include <assert.h>
#include <stdint.h>

#include <iostream>
#include <vector>
#include <random>
#include <set>

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
  assert(!tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point0), &fruits[0]));
  tree.VerifyIntegrity();
  assert(!tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point1), &fruits[1]));
  tree.VerifyIntegrity();
  assert(!tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point2), &fruits[2]));
  tree.VerifyIntegrity();
  assert(!tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point3), &fruits[3]));
  tree.VerifyIntegrity();
  assert(!tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point4), &fruits[0])); // note that 0, not 4
  tree.VerifyIntegrity();
  assert(tree.GetSize() == 5);

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
  assert(find_result.GetObjectPtr() == &fruits[0]);

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

  assert(!tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point5), &fruits[5]));
  tree.VerifyIntegrity();
  assert(tree.GetSize() == 6);
  find_result = tree.Find(point5).value();
  assert(find_result.GetObjectPtr() == &fruits[5]);
  assert(find_result.GetPointWithoutHilbertValue() == point5);

  assert(tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point2), &fruits[7]));
  tree.VerifyIntegrity();
  assert(tree.GetSize() == 6);
  find_result = tree.Find(point2).value();
  assert(find_result.GetObjectPtr() == &fruits[7]);
  assert(find_result.GetPointWithoutHilbertValue() == point2);

  assert(tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point2), &fruits[2]));
  tree.VerifyIntegrity();
  assert(tree.GetSize() == 6);
  find_result = tree.Find(point2).value();
  assert(find_result.GetObjectPtr() == &fruits[2]);
  assert(find_result.GetPointWithoutHilbertValue() == point2);
}

VarLenNumber RandomNumber(std::mt19937& mt, int n_bytes) {
  std::string repr;
  int left_bytes = n_bytes;
  while (left_bytes > 0) {
    uint32_t rand_num = mt();
    for (unsigned i = 0; i < sizeof(uint32_t); ++i) {
      if (left_bytes > 0) {
        repr += static_cast<char>((rand_num >> (8 * i)) & 0xff);
        --left_bytes;
      } else break;
    }
  }
  return VarLenNumber(n_bytes, repr);
}

VarLenRectangle RandomRectangle(std::mt19937& mt, int n_bytes) {
  VarLenNumber left = RandomNumber(mt, n_bytes),
    top = RandomNumber(mt, n_bytes),
    right = RandomNumber(mt, n_bytes),
    bottom = RandomNumber(mt, n_bytes);
  
  VarLenNumber temp = left;
  left = std::min(left, right);
  right = std::max(temp, right);

  temp = top;
  top = std::min(top, bottom);
  bottom = std::max(temp, bottom);

  return VarLenRectangle(left, top, right, bottom);
}

// Returns a vector of inserted items and their points
std::vector<std::pair<VarLenPoint2D, const std::string*>> InsertNRandomItemsIntoTree(RTree<std::string>& tree, int n, int n_bytes, std::mt19937& mt) {
  std::vector<std::pair<VarLenPoint2D, const std::string*>> inserted_items;
  
  for (int i = 0; i < n; ++i) {
    while (true) {
      VarLenPoint2D point(RandomNumber(mt, n_bytes), RandomNumber(mt, n_bytes));
      if (tree.Find(point).has_value())
        continue;

      const std::string* item = &fruits[mt() % fruits.size()];

      assert(!tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(point), item));
      tree.VerifyIntegrity();
      assert(tree.GetSize() == (size_t)(i+1));
      inserted_items.emplace_back(point, item);
      break;
    }
  }

  return inserted_items;
}

void RandomTreeTest(RTree<std::string>& tree, std::mt19937& mt, int n_items, int n_rectangles) {
  auto inserted_items = InsertNRandomItemsIntoTree(tree, n_items, tree.GetNumberLength(), mt);

  for (size_t i = 0; i < inserted_items.size(); ++i) {
    auto find_result = tree.Find(inserted_items[i].first).value();

    assert(find_result.GetPointWithoutHilbertValue() == inserted_items[i].first);
    assert(find_result.GetObjectPtr() == inserted_items[i].second);
  }

  for (int i = 0; i < n_rectangles; ++i) {
    VarLenRectangle rect = RandomRectangle(mt, tree.GetNumberLength());

    auto range_result = tree.RangeQuery(rect);

    for (auto entry: range_result) {
      assert(rect.Contains(entry.GetPointWithoutHilbertValue()));
    }

    for (auto [point, item]: inserted_items) {
      if (rect.Contains(point))
        assert(ItemInRangeResult(range_result, item, point));
    }
  }
}

void TEST_SmallBothCapacities() {
  RTree<std::string> tree(4, 2, 2);
  std::mt19937 mt(43);

  RandomTreeTest(tree, mt, 1000, 500);
}

void TEST_BigNonLeafCapacity() {
  RTree<std::string> tree(4, 2, 40);
  std::mt19937 mt(44);

  RandomTreeTest(tree, mt, 1000, 500);
}

void TEST_BigLeafCapacity() {
  RTree<std::string> tree(4, 40, 2);
  std::mt19937 mt(45);

  RandomTreeTest(tree, mt, 1000, 500);
}

void TEST_BigBothCapacities() {
  RTree<std::string> tree(4, 40, 40);
  std::mt19937 mt(46);

  RandomTreeTest(tree, mt, 1000, 500);
}

void TEST_BigNumberLength() {
  RTree<std::string> tree(29, 4, 4);
  std::mt19937 mt(47);

  RandomTreeTest(tree, mt, 1000, 500);
}

void TEST_MemoryLeak() {
  std::mt19937 mt(48);
  for (int i = 0; i < 400000; ++i) {
    RTree<std::string> tree(1, 2, 2);

    RandomTreeTest(tree, mt, 5, 0);
  }
}

void TEST_LotsOfReplaces() {
  std::vector<VarLenPoint2D> points = {
    VarLenPoint2D(VarLenNumber(1, 0x10u), VarLenNumber(1, 0x20u)),
    VarLenPoint2D(VarLenNumber(1, 0x50u), VarLenNumber(1, 0x10u)),
    VarLenPoint2D(VarLenNumber(1, 0x20u), VarLenNumber(1, 0x60u)),
    VarLenPoint2D(VarLenNumber(1, 0x60u), VarLenNumber(1, 0x50u)),
    VarLenPoint2D(VarLenNumber(1, 0x38u), VarLenNumber(1, 0x38u))
  };

  VarLenRectangle full_square(
    VarLenNumber(1, 0x00u),
    VarLenNumber(1, 0x00u),
    VarLenNumber(1, 0xffu),
    VarLenNumber(1, 0xffu)
  );

  RTree<std::string> tree(1, 2, 2);

  for (size_t i = 0; i < 100000; ++i) {
    // we use fruits.size() - 1 so that it does not divide points.size()
    const std::string* item = &fruits[i % (fruits.size() - 1)];
    tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(points[i % points.size()]), item);

    if (i >= points.size()) {
      auto range_result = tree.RangeQuery(full_square);

      for (size_t j = i - points.size() + 1; j <= i; ++j) {
        auto find_result = tree.Find(points[j % points.size()]).value();
        assert(find_result.GetPointWithoutHilbertValue() == points[j % points.size()]);
        assert(find_result.GetObjectPtr() == &fruits[j % (fruits.size() - 1)]);

        assert(ItemInRangeResult(range_result, &fruits[j % (fruits.size() - 1)], points[j % points.size()]));
      }
    }
  }
}

void TEST_TimeComplexity() {
  RTree<std::string> tree(4, 4, 4);
  std::mt19937 mt(49);

  std::vector<VarLenNumber> codes;
  for (int i = 0; i < 500000; ++i)
    codes.push_back(RandomNumber(mt, 8));
  
  std::sort(codes.begin(), codes.end());
  std::reverse(codes.begin(), codes.end());

  for (size_t i = 0; i < codes.size(); ++i) {
    tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(codes[i]), &fruits[i % fruits.size()]);
  }

  std::vector<VarLenPoint2D> points;
  for (size_t i = 0; i < codes.size(); ++i)
    points.push_back(HilbertValueToPoint(codes[i]));

  for (int i = 0; i < 500000; ++i) {
    uint32_t index = mt() % codes.size();
    auto find_result = tree.Find(points[index]).value();
    assert(find_result.GetHilbertValue() == codes[index]);
    assert(find_result.GetPointWithoutHilbertValue() == points[index]);
    assert(find_result.GetObjectPtr() == &fruits[index % fruits.size()]);

    uint32_t x = points[index].GetX().NumericalValue32(), y = points[index].GetY().NumericalValue32();
    VarLenRectangle rect(
      VarLenNumber(4, x - std::min(5u, x)),
      VarLenNumber(4, y - std::min(5u, y)),
      VarLenNumber(4, x + std::min(5u, UINT32_MAX - x)),
      VarLenNumber(4, y + std::min(5u, UINT32_MAX - y))
    );

    auto range_result = tree.RangeQuery(rect);
    assert(ItemInRangeResult(range_result, &fruits[index % fruits.size()], points[index]));
  }
}

void TEST_LotsOfMisses() {
  std::set<VarLenNumber> codes;
  RTree<uint32_t> tree(4, 4, 4);
  std::mt19937 mt(50);

  uint32_t fake_item = 0x5555;

  for (int i = 0; i < 1000; ++i) {
    VarLenNumber code = RandomNumber(mt, 8);
    codes.insert(code);
    tree.InsertOrReplace(VarLenPoint2DWithHilbertValue(code), &fake_item);
  }

  for (int i = 0; i < 500000; ++i) {
    VarLenNumber code = RandomNumber(mt, 8);
    VarLenPoint2D point = HilbertValueToPoint(code);

    auto find_result_optional = tree.Find(point);
    assert(!find_result_optional.has_value() || (
      codes.find(find_result_optional.value().GetHilbertValue()) != codes.end() &&
      find_result_optional.value().GetObjectPtr() == &fake_item
    ));
  }
}

}
}

#define TEST(test_name) { ROCKSDB_NAMESPACE::TEST_ ## test_name(); std::cout << ("=== " #test_name " passed successfully") << std::endl; }

int main(void) {
  TEST(Smoke);
  TEST(SmallBothCapacities);
  TEST(BigNonLeafCapacity);
  TEST(BigLeafCapacity);
  TEST(BigBothCapacities);
  TEST(BigNumberLength);
  TEST(MemoryLeak);
  TEST(LotsOfReplaces);
  TEST(TimeComplexity);
  TEST(LotsOfMisses);
  return 0;
}

