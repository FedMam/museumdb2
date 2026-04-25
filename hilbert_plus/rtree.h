#pragma once

#include "hilbert/geometry.h"
#include "hilbert/hilbert_curve.h"
#include "rocksdb/rocksdb_namespace.h"

#include <stddef.h>
#include <assert.h>

#include <list>
#include <algorithm>
#include <vector>
#include <memory>
#include <optional>
#include <stdexcept>

namespace ROCKSDB_NAMESPACE {

// needed to declare it as a friend to RTree
template<typename TItem>
struct RTreeBuilder;

// This structure will also be used as output of the queries,
// this is why its methods are publicly exposed
template<typename TItem>
struct RTreeEntry {
 public:
  RTreeEntry(const VarLenPoint2DWithHilbertValue& point,
             const TItem& item)
    : point_(point),
      item_(item) { }
  
  inline const VarLenPoint2DWithHilbertValue& GetPointWithHilbertCode() const { return point_; }
  inline const VarLenPoint2D& GetPointWithoutHilbertValue() const { return point_.GetPoint(); }
  inline const VarLenNumber& GetHilbertCode() const { return point_.GetHilbertCode(); }
  inline const TItem& GetItem() const { return item_; }

  // Returns a degenerate rectangle of one point
  inline const VarLenRectangle GetBoundingRectangle() const {
    return VarLenRectangle(point_.GetPoint(), point_.GetPoint());
  }

 private:
  VarLenPoint2DWithHilbertValue point_;
  VarLenNumber hilbert_code_;
  TItem item_;
};

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
 private:
  struct RTreeNode {
   public:
    // Will construct an empty node
    RTreeNode()
      : parent_(nullptr),
        non_leaf_children_(),
        leaf_children_() { }

    // Will construct a non-leaf node
    explicit RTreeNode(const std::list<RTreeNode*>& non_leaf_children)
      : parent_(nullptr),
        non_leaf_children_(non_leaf_children),
        leaf_children_() {
      assert(!non_leaf_children.empty());
      for (auto iter = non_leaf_children.begin(); iter != non_leaf_children.end(); ++iter)
        (*iter)->parent_ = this;
      RecalculateLHVAndMBR();
    }

    // Will construct a leaf node
    explicit RTreeNode(const std::list<RTreeEntry<TItem>>& leaf_children)
      : parent_(nullptr),
        non_leaf_children_(),
        leaf_children_(leaf_children) {
      assert(!leaf_children_.empty());
      RecalculateLHVAndMBR();
    }

    ~RTreeNode() {
      if (!IsLeaf()) {
        for (auto iter = non_leaf_children_.begin(); iter != non_leaf_children_.end(); ++iter)
          delete (*iter);
      }
    }

    RTreeNode(const RTreeNode& other) = delete;
    RTreeNode(RTreeNode&& other) = delete;

    inline bool IsLeaf() const { return IsEmpty() || !leaf_children_.empty(); }

    inline bool IsEmpty() const {
      return leaf_children_.empty() && non_leaf_children_.empty();
    }

    inline size_t GetSize() const {
      if (IsLeaf()) return leaf_children_.size();
      return non_leaf_children_.size();
    }

    inline RTreeNode* GetParent() const { return parent_; }

    // Largest Hilbert value of all the node's children.
    inline const VarLenNumber& GetLHV() const { return lhv_; }

    // Minimum bounding rectangle of all the node's children.
    inline const VarLenRectangle& GetMBR() const { return mbr_; }

    // Sets lhv_ to max(lhv_, newLhv). Does not propagate changes upwards.
    inline void AdjustLHV(const VarLenNumber& newLhv) {
      lhv_ = std::max(lhv_, newLhv);
    }

    // Sets mbr_ to MBR(mbr_, newMbr). Does not propagate changes upwards.
    inline void AdjustMBR(const VarLenRectangle& newMbr) {
      mbr_ = mbr_.MBR(newMbr);
    }

    // Recalculates LHV and MBR based on these values of children
    void RecalculateLHVAndMBR() {
      if (IsEmpty()) return;
      if (IsLeaf()) {
        lhv_ = leaf_children_.front().GetHilbertCode();
        mbr_ = leaf_children_.front().GetBoundingRectangle();

        auto iter = leaf_children_.begin();
        ++iter;
        while (iter != leaf_children_.end()) {
          lhv_ = std::max(lhv_, (*iter).GetHilbertCode());
          mbr_ = mbr_.MBR((*iter).GetBoundingRectangle());
          ++iter;
        }
      } else {
        lhv_ = non_leaf_children_.front()->GetLHV();
        mbr_ = non_leaf_children_.front()->GetMBR();

        auto iter = non_leaf_children_.begin();
        ++iter;
        while (iter != non_leaf_children_.end()) {
          lhv_ = std::max(lhv_, (*iter)->GetLHV());
          mbr_ = mbr_.MBR((*iter)->GetMBR());
          ++iter;
        }
      }
    }

    // Splits this node into two: left and right.
    // This node becomes the right node, and pointer to the left node
    // is returned. Don't forget to deallocate or own that pointer to prevent
    // memory leak.
    // Will automatically recalculate MBR and LHV, but will not propagate changes upward. Do not forget to do it manually.
    RTreeNode* Split() {
      if (IsLeaf()) {
        size_t left_size = leaf_children_.size() / 2;

        std::list<RTreeEntry<TItem>> left_leaf_children;
        auto middle_iter = leaf_children_.begin();
        std::advance(middle_iter, left_size);
        left_leaf_children.splice(left_leaf_children.begin(), leaf_children_, leaf_children_.begin(), middle_iter);
        RecalculateLHVAndMBR();

        RTreeNode* left_node = new RTreeNode(left_leaf_children);
        left_node->parent_ = this->parent_;
        return left_node;
      } else {
        size_t left_size = non_leaf_children_.size() / 2;

        std::list<RTreeNode*> left_non_leaf_children;
        auto middle_iter = non_leaf_children_.begin();
        std::advance(middle_iter, left_size);
        left_non_leaf_children.splice(left_non_leaf_children.begin(), non_leaf_children_, non_leaf_children_.begin(), middle_iter);
        RecalculateLHVAndMBR();

        RTreeNode* left_node = new RTreeNode(left_non_leaf_children);
        left_node->parent_ = this->parent_;
        return left_node;
      }
    }

    friend class RTree;
    friend class RTreeDumper;

   private:
    RTreeNode* parent_;

    // Convention: either non_leaf_children_ or leaf_children_ is empty,
    // and this determines if this node is a leaf or non-leaf node.
    // Only root node is allowed to have both lists empty. Then it
    // counts as a leaf node.
    // We do this to not mess up with inheritance and
    // dynamic_cast's which add unnecessary runtime overhead.
    // For nothing to go wrong, we define appropriate constructors.

    std::list<RTreeNode*> non_leaf_children_;
    std::list<RTreeEntry<TItem>> leaf_children_;

    VarLenNumber lhv_;  // largest Hilbert value
    VarLenRectangle mbr_;  // minimum bounding rectangle
  };

 public:
  // Parameters:
  // - number_length: the length in bytes of all point coordinates;
  // - leaf_capacity: maximum capacity of leaf nodes (only affects time and space usage). Must be at least 2;
  // - non_leaf_capacity: maximum capacity of non-leaf nodes (same). Must be at least 2.
  RTree(size_t number_length,
        size_t leaf_capacity,
        size_t non_leaf_capacity)
    : RTree(number_length, leaf_capacity, non_leaf_capacity, 0, new RTreeNode()) { }

  ~RTree() {
    delete root_;
  }

  RTree(const RTree<TItem>& other) = delete;
  RTree(RTree<TItem>&& other) = delete;
  
  inline size_t GetNumberLength() const { return number_length_; }
  inline size_t GetLeafCapacity() const { return leaf_capacity_; }
  inline size_t GetNonLeafCapacity() const { return non_leaf_capacity_; }

  inline size_t GetSize() const { return tree_size_; }

  std::optional<RTreeEntry<TItem>> Find(const VarLenPoint2D& key_point) const {
    std::vector<RTreeEntry<TItem>> result;

    SearchRec_(root_, VarLenRectangle(key_point, key_point), result);

    if (result.empty())
      return std::optional<RTreeEntry<TItem>>();
    // there can only be one object at a single point
    return result[0];
  }

  std::vector<RTreeEntry<TItem>> RangeQuery(const VarLenRectangle& rect) const  {
    std::vector<RTreeEntry<TItem>> result;

    SearchRec_(root_, rect, result);

    return result;
  }

  // Returns true if the item at key_point already existed and had been replaced.
  bool InsertOrReplace(const VarLenPoint2DWithHilbertValue& key_point, const TItem& item) {
    VerifyPointLength_(key_point);

    RTreeEntry<TItem> entry(key_point, item);

    auto [was_replaced, maybe_new_node] = InsertOrReplaceRec_(root_, entry);
    
    if (maybe_new_node) {
      std::list<RTreeNode*> new_root_children = { maybe_new_node, root_ };
      RTreeNode* new_root = new RTreeNode(new_root_children);
      root_ = new_root;
    }

    return was_replaced;
  }

  // Throws an error if:
  // - the tree size does not match the number of elements in it;
  // - any node's parent is not equal to the node which has it as a child;
  // - any node's number of children exceeds its capacity;
  // - any of the Hilbert values in the leaf node have the wrong byte length;
  // - any node's stored LHV and MBR values are not equal to the actual LHV and MBR of their children;
  // - all the Hilbert values in the leaves are not stored in strict order.
  //
  // Should be only used in tests.
  void VerifyIntegrity() const {
    size_t actual_size = 0;
    VerifyIntegrityRec_(root_, VarLenNumber(0), &actual_size);
    assert(actual_size == tree_size_);
  }

  friend class RTreeBuilder<TItem>;
  friend class RTreeDumper;

 private:
  RTree(size_t number_length,
        size_t leaf_capacity,
        size_t non_leaf_capacity,
        size_t tree_size,
        RTreeNode* root)
    : number_length_(number_length),
      leaf_capacity_(leaf_capacity),
      non_leaf_capacity_(non_leaf_capacity),
      tree_size_(tree_size),
      root_(root) {
    assert(number_length >= 1);
    assert(leaf_capacity >= 2);
    assert(non_leaf_capacity >= 2);
  }

  // Throws an error if point's coordinates byte length does not match number_length (used in queries)
  inline void VerifyPointLength_(const VarLenPoint2DWithHilbertValue& point) const {
    assert(point.GetNumberLength() == number_length_);
  }

  inline void VerifyHilbertValueLength_(const VarLenNumber& hilbertValue) const {
    assert(hilbertValue.GetLength() == number_length_ * 2);
  }

  void SearchRec_(RTreeNode* curr_node, const VarLenRectangle& rect, std::vector<RTreeEntry<TItem>>& result) const {
    assert(curr_node != nullptr);

    if (curr_node->IsLeaf()) {
      for (auto iter = curr_node->leaf_children_.begin(); iter != curr_node->leaf_children_.end(); ++iter) {
        if (rect.Contains((*iter).GetPointWithoutHilbertValue()))
          result.push_back((*iter));
      }
    } else {
      for (auto iter = curr_node->non_leaf_children_.begin(); iter != curr_node->non_leaf_children_.end(); ++iter) {
        if (!rect.Intersects((*iter)->GetMBR()))
          continue;
        
        SearchRec_(*iter, rect, result);
      }
    }
  }

  // First return result: true if the element at this position already existed and has
  // been replaced, or false if it was inserted.
  // Second return result: a pointer to a new node if curr_node has been split during the process,
  // or nullptr if it hasn't.
  std::pair<bool, RTreeNode*> InsertOrReplaceRec_(RTreeNode* curr_node, const RTreeEntry<TItem>& item) {
    assert(curr_node != nullptr);

    if (curr_node->IsLeaf()) {
      for (auto iter = curr_node->leaf_children_.begin(); iter != curr_node->leaf_children_.end(); ++iter) {
        int cmp = (*iter).GetHilbertCode().Compare(item.GetHilbertCode());
        if (cmp == 0) {
          // replace
          (*iter) = item;
          curr_node->RecalculateLHVAndMBR();
          // don't increase tree_size_
          return std::make_pair(true, nullptr);
        } else if (cmp >= 0) {
          curr_node->leaf_children_.insert(iter, item);
          ++tree_size_;
          if (curr_node->GetSize() > leaf_capacity_) {
            RTreeNode* left_node = curr_node->Split();
            return std::make_pair(false, left_node);
          } else {
            curr_node->RecalculateLHVAndMBR();
            return std::make_pair(false, nullptr);
          }
        }
      }

      curr_node->leaf_children_.insert(curr_node->leaf_children_.end(), item);
      ++tree_size_;
      if (curr_node->GetSize() > leaf_capacity_) {
        RTreeNode* left_node = curr_node->Split();
        return std::make_pair(false, left_node);
      } else {
        curr_node->RecalculateLHVAndMBR();
        return std::make_pair(false, nullptr);
      }
    } else {
      auto pos_to_insert = curr_node->non_leaf_children_.begin();

      for (auto iter = curr_node->non_leaf_children_.begin(); iter != curr_node->non_leaf_children_.end(); ++iter) {
        pos_to_insert = iter;
        int cmp = (*iter)->GetLHV().Compare(item.GetHilbertCode());
        if (cmp >= 0) {
          break;
        }
      }
      // if we didn't found a child node to insert, then
      // pos_to_insert will carry the position of the last child node
      // (one before end())

      auto [was_replaced, maybe_new_node] = InsertOrReplaceRec_(*pos_to_insert, item);
      if (!maybe_new_node) {
        curr_node->RecalculateLHVAndMBR();
        return std::make_pair(was_replaced, nullptr);
      } else {
        curr_node->non_leaf_children_.insert(pos_to_insert, maybe_new_node);
        maybe_new_node->parent_ = curr_node;

        if (curr_node->GetSize() > non_leaf_capacity_) {
          RTreeNode* left_node = curr_node->Split();
          return std::make_pair(was_replaced, left_node);
        } else {
          curr_node->RecalculateLHVAndMBR();
          return std::make_pair(was_replaced, nullptr);
        }
      }
    }
  }

  // prev_lhv: largest Hilbert value found in previously visited nodes (value of 0 with length 0 is used as -INF)
  // actual_size: pointer to a value in which we'll record the number of items in it
  // Returns: largest Hilbert value found yet
  VarLenNumber VerifyIntegrityRec_(RTreeNode* curr_node, const VarLenNumber& prev_lhv, size_t* actual_size) const {
    assert(curr_node != nullptr);

    VarLenNumber lhv = prev_lhv;

    if (curr_node->IsLeaf())
      assert(curr_node->GetSize() <= leaf_capacity_);
    else
      assert(curr_node->GetSize() <= non_leaf_capacity_);

    // This checks that the node's stored LHV and MBR values are valid
    VarLenNumber old_node_lhv = curr_node->GetLHV();
    VarLenRectangle old_node_mbr = curr_node->GetMBR();
    curr_node->RecalculateLHVAndMBR();
    assert(old_node_lhv == curr_node->GetLHV() && old_node_mbr == curr_node->GetMBR());
    
    if (curr_node->IsLeaf()) {
      (*actual_size) += curr_node->GetSize();

      for (auto iter = curr_node->leaf_children_.begin(); iter != curr_node->leaf_children_.end(); ++iter) {
        VerifyHilbertValueLength_((*iter).GetHilbertCode());
        assert((lhv.GetLength() == 0) || ((*iter).GetHilbertCode() > lhv));
        lhv = (*iter).GetHilbertCode();
      }
    } else {
      for (auto iter = curr_node->non_leaf_children_.begin(); iter != curr_node->non_leaf_children_.end(); ++iter) {
        assert((*iter)->parent_ == curr_node);
        lhv = VerifyIntegrityRec_(*iter, lhv, actual_size);
      }
    }

    return lhv;
  }

  size_t number_length_;
  size_t leaf_capacity_;
  size_t non_leaf_capacity_;

  size_t tree_size_;
  RTreeNode* root_;
};

} // namespace ROCKSDB_NAMESPACE
