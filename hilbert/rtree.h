#pragma once

#include "hilbert/geometry.h"
#include "hilbert/hilbert_curve.h"
#include "rocksdb/rocksdb_namespace.h"

#include <memory>
#include <optional>

#include <stddef.h>

#include <list>
#include <algorithm>
#include <vector>

namespace ROCKSDB_NAMESPACE {

// This structure will also be used as output of the queries,
// this is why its methods are publicly exposed
template<typename TItem>
struct RTreeLeafNodeEntry {
 public:
  RTreeLeafNodeEntry(const VarLenPoint2DWithHilbertValue& point,
                     const TItem* object_ptr)
    : point_(point),
      object_ptr(object_ptr) { }
  
  inline const VarLenPoint2DWithHilbertValue& GetPointWithHilbertValue() const { return point_; }
  inline const VarLenPoint2D& GetPointWithoutHilbertValue() const { return point_.GetPoint(); }
  inline const VarLenNumber& GetHilbertValue() const { return point_.GetHilbertValue(); }
  inline const TItem* GetObjectPtr() const { return object_ptr_; }

  // Returns a degenerate rectangle of one point
  inline const VarLenRectangle GetBoundingRectangle() const {
    return VarLenRectangle(point_.GetPoint(), point_.GetPoint());
  }

 private:
  VarLenPoint2DWithHilbertValue point_;
  VarLenNumber hilbert_value_;
  const TItem* object_ptr_;
};

template<typename TItem>
struct RTreeNode;

// This is a Hilbert R-Tree implementation. This structure is
// a map which stores 2D points, represented as VarLenPoint2DWithHilbertValue's,
// associated with their Hilbert values and pointers to objects
// of custom type (be careful not to use these after the object
// is destroyed). It supports effective search, rectangular range queries, and
// additions/replacements of objects. Multiple objects at a single point are not allowed.
// However note that this structure does not support deletions, since we
// are not going to need them for the SER-tree index implementation.
// WARNING: all point parameters of the InsertOrReplace queries to this structure must
// have their coordinates be of the same byte length, defined by the number_length
// constructor parameter. An error will be thrown if this condition is not
// satisfied.
template<typename TItem>
class RTree {
 public:
  // Parameters:
  // - number_length: the length in bytes of all point coordinates;
  // - leaf_capacity: maximum capacity of leaf nodes (only affects time and space usage). Must be at least 2;
  // - non_leaf_capacity: maximum capacity of non-leaf nodes (same). Must be at least 2.
  RTree(size_t number_length,
        size_t leaf_capacity,
        size_t non_leaf_capacity)
    : number_length_(number_length),
      leaf_capacity_(leaf_capacity),
      non_leaf_capacity_(non_leaf_capacity) {
    assert(leaf_capacity >= 2);
    assert(non_leaf_capacity >= 2);
  }
  
  inline size_t GetNumberLength() const { return number_length_; }
  inline size_t GetLeafCapacity() const { return leaf_capacity_; }
  inline size_t GetNonLeafCapacity() const { return non_leaf_capacity_; }

  std::optional<RTreeLeafNodeEntry<TItem>> Find(const VarLenPoint2D& key_point) const;

  std::vector<RTreeLeafNodeEntry<TItem>> RangeQuery(const VarLenRectangle& rect) const;

  // Returns true if the item at key_point already existed and had been replaced.
  bool InsertOrReplace(const VarLenPoint2DWithHilbertValue& key_point, const TItem* item);

 private:
  // Throws an error if point's coordinates byte length does not match number_length (used in queries)
  inline void VerifyPointLength_(const VarLenPoint2DWithHilbertValue& point) {
    assert(point.GetNumberLength() == number_length_);
  }

  // Returns true if item's key point already existed in this node and
  // the item was replaces instead of inserted
  // Warning: throws an error if node is not leaf
  bool InsertIntoLeafNode_(RTreeNode<TItem>* node, const RTreeLeafNodeEntry<TItem>& item);

  // Note: parameter added_item is needed so we can call AdjustLHV/MBRAndPropagate() instead of RecalculateLHVAndMBR()
  // since we know that the only value which could increase the node's LHV and MBR is this item.
  void SplitNode_(RTreeNode<TItem>* node, const RTreeLeafNodeEntry<TItem>& added_item);

  void SearchRec_(RTreeNode<TItem>* curr_node, const VarLenRectangle& rect, std::vector<RTreeLeafNodeEntry<TItem>>& result) const;

  // Choose the leaf node in which to place a new point.
  RTreeNode<TItem>* ChooseLeaf_(const VarLenPoint2DWithHilbertValue& point);

  size_t number_length_;
  size_t leaf_capacity_;
  size_t non_leaf_capacity_;

  std::unique_ptr<RTreeNode<TItem>> root_;
};

} // namespace ROCKSDB_NAMESPACE
