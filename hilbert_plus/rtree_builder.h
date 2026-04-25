#pragma once

#include "hilbert/rtree.h"

namespace ROCKSDB_NAMESPACE {

// This structure will accept a list of arbitrary objects associated
// with VarLenPoint2D coordinates and will build an RTree over them once needed.
// Like RTree, this structure follows the strict constraint that all points
// must have the same byte length of their coordinates.
// It is preferred to insert the items into it in their Hilbert order so that
// you don't have to call Sort() after inserting them.
template<typename TItem>
struct RTreeBuilder {
 private:
  struct RTreeEntryComparator {
    bool operator()(const RTreeEntry<TItem>& itemA, const RTreeEntry<TItem>& itemB) const {
      return itemA.GetHilbertCode() < itemB.GetHilbertCode();
    }
  };

  using RTreeNode = typename RTree<TItem>::RTreeNode;

 public:
  RTreeBuilder(size_t number_length)
    : number_length_(number_length),
      inserted_items_(),
      items_in_order_(true),
      items_in_strict_order_(true) {
    assert(number_length >= 1);
  }

  inline size_t GetSize() const { return inserted_items_.size(); }
  
  inline void Add(const VarLenPoint2DWithHilbertValue& point, const TItem& item) {
    VerifyPointLength_(point);

    if (items_in_order_ && GetSize() > 0) {
      int cmp = point.GetHilbertCode().Compare(inserted_items_[GetSize()-1].GetHilbertCode());
      if (cmp <= 0) items_in_strict_order_ = false;
      if (cmp < 0) items_in_order_ = false;
    }

    inserted_items_.push_back(RTreeEntry<TItem>(point, item));
  }

  inline void Sort() {
    std::sort(inserted_items_.begin(), inserted_items_.end(), RTreeEntryComparator{});
    items_in_order_ = true;
    items_in_strict_order_ = true;
    for (size_t i = 1; i < inserted_items_.size(); ++i) {
      if (inserted_items_[i].GetHilbertCode() == inserted_items_[i-1].GetHilbertCode()) {
        items_in_strict_order_ = false;
        break;
      }
    }
  }

  // Returns true if the items' points are in non-strict ascending Hilbert order (O(1), because pre-calculated).
  inline bool CheckItemsInOrder() const { return items_in_order_; }

  // Returns true if the items' points are in strict ascending Hilbert order
  // (which means there also cannot be two items with the same points) (O(1), because pre-calculated).
  inline bool CheckItemsInStrictOrder() const { return items_in_strict_order_; }

  // WARNING: throws an error if the items' points are not in their Hilbert order. Either ensure that or call Sort() before calling Build().
  // WARNING: if there are items with equal points, only the first of them will be used to build the tree, the rest will be discarded.
  RTree<TItem> Build(size_t leaf_capacity, size_t non_leaf_capacity) const {
    assert(items_in_order_);

    if (inserted_items_.empty()) {
      return RTree<TItem>(number_length_, leaf_capacity, non_leaf_capacity);
    }

    std::vector<RTreeNode*> curr_level_nodes;
    
    // bottom level
    VarLenNumber prev_hilbert_value;
    size_t num_inserted_items = 0;
    std::list<RTreeEntry<TItem>> leaf_children;
    for (size_t el_index = 0; el_index < inserted_items_.size(); ++el_index) {
      if (el_index > 0 && inserted_items_[el_index].GetHilbertCode() == prev_hilbert_value)
        continue;
      prev_hilbert_value = inserted_items_[el_index].GetHilbertCode();

      leaf_children.push_back(inserted_items_[el_index]);
      ++num_inserted_items;
      if (leaf_children.size() == leaf_capacity) {
        curr_level_nodes.push_back(new RTreeNode(leaf_children));
        leaf_children.clear();
      }
    }
    if (leaf_children.size() > 0)
      curr_level_nodes.push_back(new RTreeNode(leaf_children));
    
    while (curr_level_nodes.size() > 1) {
      std::vector<RTreeNode*> next_level_nodes;

      std::list<RTreeNode*> non_leaf_children;
      for (size_t nd_index = 0; nd_index < curr_level_nodes.size(); ++nd_index) {
        non_leaf_children.push_back(curr_level_nodes[nd_index]);

        if (non_leaf_children.size() == non_leaf_capacity) {
          next_level_nodes.push_back(new RTreeNode(non_leaf_children));
          non_leaf_children.clear();
        }
      }
      if (non_leaf_children.size() > 0)
        next_level_nodes.push_back(new RTreeNode(non_leaf_children));
      
      curr_level_nodes = next_level_nodes;
    }

    return RTree<TItem>(number_length_, leaf_capacity, non_leaf_capacity, num_inserted_items, curr_level_nodes[0]);
  }

 private:
  inline void VerifyPointLength_(const VarLenPoint2DWithHilbertValue& point) const {
    assert(point.GetNumberLength() == number_length_);
  }

  size_t number_length_;

  std::vector<RTreeEntry<TItem>> inserted_items_;

  bool items_in_order_;
  bool items_in_strict_order_;
};

}

