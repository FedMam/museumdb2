// TODO(FedMam): rewrite into Google Test

#include "hilbert/rtree_builder.h"

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

void TEST_NoSortNoStrictOrder() {
  std::vector<std::pair<VarLenNumber, std::string>> items;
  std::mt19937 mt(51);

  for (size_t i = 0; i < 500000; ++i) {
    VarLenNumber hilbertValue = RandomNumber(mt, 2);
    items.emplace_back(hilbertValue, fruits[mt() % fruits.size()]);
  }
  sort(items.begin(), items.end());
  
  RTreeBuilder<std::string> tree_builder(1);
  for (auto [hilbert_value, item]: items) {
    tree_builder.Add(VarLenPoint2DWithHilbertValue(hilbert_value), item);
    assert(tree_builder.CheckItemsInOrder());
  }

  RTree<std::string> tree = tree_builder.Build(4, 4);
  tree.VerifyIntegrity();

  VarLenNumber prev_code;
  for (size_t i = 0; i < items.size(); ++i) {
    // not checking duplicates
    if (i > 0 && items[i].first == prev_code)
      continue;
    prev_code = items[i].first;

    auto find_result = tree.Find(HilbertValueToPoint(items[i].first)).value();
    assert(find_result.GetHilbertValue() == items[i].first);
    assert(find_result.GetItem() == items[i].second);
  }
}

void TEST_NoSortStrictOrder() {
  std::set<std::pair<VarLenNumber, std::string>> items;
  std::mt19937 mt(52);

  for (size_t i = 0; i < 500000; ++i) {
    VarLenNumber hilbertValue = RandomNumber(mt, 8);
    items.insert(std::make_pair(hilbertValue, fruits[mt() % fruits.size()]));
  }

  RTreeBuilder<std::string> tree_builder(4);
  for (auto [hilbert_value, item]: items) {
    tree_builder.Add(VarLenPoint2DWithHilbertValue(hilbert_value), item);
    assert(tree_builder.CheckItemsInOrder());
    assert(tree_builder.CheckItemsInStrictOrder());
  }

  RTree<std::string> tree = tree_builder.Build(2, 2);
  for (auto [hilbert_value, item]: items) {
    auto find_result = tree.Find(HilbertValueToPoint(hilbert_value)).value();
    assert(find_result.GetHilbertValue() == hilbert_value);
    assert(find_result.GetItem() == item);
  }
}

void TEST_SortWithDuplicates() {
  std::vector<VarLenNumber> hilbert_values;
  std::mt19937 mt(53);

  for (size_t i = 0; i < 500000; ++i) {
    VarLenNumber hilbert_value = RandomNumber(mt, 2);
    hilbert_values.push_back(hilbert_value);
  }
  sort(hilbert_values.begin(), hilbert_values.end());
  
  RTreeBuilder<std::string> tree_builder(1);
  for (auto hilbert_value: hilbert_values) {
    tree_builder.Add(VarLenPoint2DWithHilbertValue(hilbert_value), fruits[hilbert_value.NumericalValue32() % fruits.size()]);
  }

  RTree<std::string> tree = tree_builder.Build(40, 2);
  tree.VerifyIntegrity();

  for (auto hilbert_value: hilbert_values) {
    auto find_result = tree.Find(HilbertValueToPoint(hilbert_value)).value();
    assert(find_result.GetHilbertValue() == hilbert_value);
    assert(find_result.GetItem() == fruits[hilbert_value.NumericalValue32() % fruits.size()]);
  }
}

void TEST_SortNoDuplicates() {
  std::set<std::pair<VarLenNumber, std::string>> items_set;
  std::mt19937 mt(54);

  for (size_t i = 0; i < 500000; ++i) {
    VarLenNumber hilbertValue = RandomNumber(mt, 8);
    items_set.insert(std::make_pair(hilbertValue, fruits[mt() % fruits.size()]));
  }

  std::vector<std::pair<VarLenNumber, std::string>> items;
  std::copy(items_set.begin(), items_set.end(), std::back_inserter(items));
  std::shuffle(items.begin(), items.end(), mt);

  RTreeBuilder<std::string> tree_builder(4);
  for (auto [hilbert_value, item]: items)
    tree_builder.Add(VarLenPoint2DWithHilbertValue(hilbert_value), item);
  
  tree_builder.Sort();
  assert(tree_builder.CheckItemsInOrder());
  assert(tree_builder.CheckItemsInStrictOrder());

  RTree<std::string> tree = tree_builder.Build(40, 40);
  for (auto [hilbert_value, item]: items) {
    auto find_result = tree.Find(HilbertValueToPoint(hilbert_value)).value();
    assert(find_result.GetHilbertValue() == hilbert_value);
    assert(find_result.GetItem() == item);
  }
}

}
}

#define TEST(test_name) { ROCKSDB_NAMESPACE::TEST_ ## test_name(); std::cout << ("=== " #test_name " passed successfully") << std::endl; }

int main(void) {
  TEST(NoSortNoStrictOrder);
  TEST(NoSortStrictOrder);
  TEST(SortWithDuplicates);
  TEST(SortNoDuplicates);
  return 0;
}

