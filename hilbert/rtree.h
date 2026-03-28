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

// This structure will also be used as output of the queries,
// this is why its methods are publicly exposed
template<typename TItem>
struct RTreeEntry {
 public:
  RTreeEntry(const VarLenPoint2DWithHilbertValue& point,
                     const TItem* object_ptr)
    : point_(point),
      object_ptr_(object_ptr) { }
  
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

    // Sets lhv_ to max(lhv_, newLhv). Propagates changes upwards, but only if any. 
    void AdjustLHVAndPropagate(const VarLenNumber& newLhv) {
      if (newLhv > lhv_) {
        lhv_ = newLhv;
        if (parent_)
          parent_->AdjustLHVAndPropagate(lhv_);
      }
    }

    // Sets mbr_ to MBR(mbr_, newMbr). Propagates changes upwards, but only if any. 
    void AdjustMBRAndPropagate(const VarLenRectangle& newMbr) {
      VarLenRectangle commonMbr = mbr_.MBR(newMbr);
      if (commonMbr != mbr_) {
        mbr_ = commonMbr;
        if (parent_)
          parent_->AdjustMBRAndPropagate(mbr_);
      }
    }

    // Recalculates LHV and MBR and propagates changes upwards if propagate is true.
    void RecalculateLHVAndMBR(bool propagate = false) {
      if (IsEmpty()) return;
      if (IsLeaf()) {
        lhv_ = leaf_children_.front().GetHilbertValue();
        mbr_ = leaf_children_.front().GetBoundingRectangle();

        auto iter = leaf_children_.begin();
        ++iter;
        while (iter != leaf_children_.end()) {
          lhv_ = std::max(lhv_, (*iter).GetHilbertValue());
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
      if (propagate)
        if (parent_)
          parent_->RecalculateLHVAndMBR(propagate);
    }

    // Fails if the node's parent_ does not equal expected_parent or
    // lhv_ and mbr_ are not equal to the actual LHV and MBR
    // of all children. Checks not only this node but its whole subtree.
    // Should be only used in tests.
    void VerifyIntegrityInSubTree(const RTreeNode* expected_parent) {
      assert(parent_ == expected_parent);
      
      VarLenNumber old_lhv = lhv_;
      VarLenRectangle old_mbr = mbr_;
      
      RecalculateLHVAndMBR(false);

      assert (lhv_ == old_lhv && mbr_ == old_mbr);

      if (!IsLeaf()) {
        for (auto iter = non_leaf_children_.begin(); iter != non_leaf_children_.end(); ++iter) {
          (*iter)->VerifyIntegrityInSubTree(this);
        }
      }
    }

    // Inserts a new item into the node (replaces if the item with the same Hilbert value already exists).
    // Warning: throws an error if this node is not a leaf node.
    // Warning: recalculates MBR and LHV, but does not propagate changes upward. Do not forget to do it manually.
    void InsertOrReplaceInLeaf(const RTreeEntry<TItem>& item) {
      assert(IsLeaf());

      mbr_ = mbr_.MBR(item.GetBoundingRectangle());
      
      for (auto iter = leaf_children_.begin(); iter != leaf_children_.end(); ++iter) {
        int cmp = (*iter).GetHilbertValue().Compare(item.GetHilbertValue());
        if (cmp == 0) {
          (*iter) = item;
          return;
        } else if (cmp > 0) {
          leaf_children_.insert(iter, item);
          return;
        }
      }

      leaf_children_.insert(leaf_children_.end(), item);
      lhv_ = item.GetHilbertValue();
    }

    // Title.
    // Used only after Split() to insert a newly-created left node
    // at position of the right node in the splitted node's parent.
    // Warning: throws an error if this node is not a non-leaf node or
    // node_to_position was not found in this node.
    // Warning: recalculates MBR and LHV, but does not propagate changes upward. Do not forget to do it manually.
    void InsertInNonLeafAtPositionOfAnotherNode(RTreeNode* new_node, RTreeNode* node_to_position) {
      assert(!IsLeaf());

      for (auto iter = non_leaf_children_.begin(); iter != non_leaf_children_.end(); ++iter) {
        if (*iter == node_to_position) {
          non_leaf_children_.insert(iter, new_node);
          new_node->parent_ = this;
          lhv_ = std::max(lhv_, new_node->GetLHV());
          mbr_ = mbr_.MBR(new_node->GetMBR());
          return;
        }
      }

      throw std::logic_error("node node_to_position was not found in this node");
    }
    
    // Splits this node into two: left and right.
    // This node becomes the right node, and pointer to the left node
    // is returned. Don't forget to deallocate or own that pointer to prevent
    // memory leak.
    // Warning: recalculates MBR and LHV, but does not propagate changes upward. Do not forget to do it manually.
    RTreeNode* Split() {
      if (IsLeaf()) {
        size_t left_size = leaf_children_.size() / 2;

        std::list<RTreeEntry<TItem>> left_leaf_children;
        auto middle_iter = leaf_children_.begin();
        std::advance(middle_iter, left_size);
        left_leaf_children.splice(left_leaf_children.begin(), leaf_children_, leaf_children_.begin(), middle_iter);
        RecalculateLHVAndMBR(false);

        RTreeNode* left_node = new RTreeNode(left_leaf_children);
        left_node->parent_ = this->parent_;
        return left_node;
      } else {
        size_t left_size = non_leaf_children_.size() / 2;

        std::list<RTreeNode*> left_non_leaf_children;
        auto middle_iter = non_leaf_children_.begin();
        std::advance(middle_iter, left_size);
        left_non_leaf_children.splice(left_non_leaf_children.begin(), non_leaf_children_, non_leaf_children_.begin(), middle_iter);
        RecalculateLHVAndMBR(false);

        RTreeNode* left_node = new RTreeNode(left_non_leaf_children);
        left_node->parent_ = this->parent_;
        return left_node;
      }
    }

    friend class RTree<TItem>;

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
    : number_length_(number_length),
      leaf_capacity_(leaf_capacity),
      non_leaf_capacity_(non_leaf_capacity) {
    assert(leaf_capacity >= 2);
    assert(non_leaf_capacity >= 2);
  }

  ~RTree() {
    delete root_;
  }

  RTree(const RTree<TItem>& other) = delete;
  RTree(RTree<TItem>&& other) = delete;
  
  inline size_t GetNumberLength() const { return number_length_; }
  inline size_t GetLeafCapacity() const { return leaf_capacity_; }
  inline size_t GetNonLeafCapacity() const { return non_leaf_capacity_; }

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
  bool InsertOrReplace(const VarLenPoint2DWithHilbertValue& key_point, const TItem* item) {
    VerifyPointLength_(key_point);

    RTreeNode* leaf_node = ChooseLeaf_(key_point);

    return InsertIntoLeafNode_(leaf_node, RTreeEntry(key_point, item));
  }

  // Should be only used in tests
  void VerifyIntegrity() const {
    root_->VerifyIntegrityInSubTree(nullptr);
  }

 private:
  // Throws an error if point's coordinates byte length does not match number_length (used in queries)
  inline void VerifyPointLength_(const VarLenPoint2DWithHilbertValue& point) {
    assert(point.GetNumberLength() == number_length_);
  }

  // Returns true if item's key point already existed in this node and
  // the item was replaces instead of inserted
  // Warning: throws an error if node is not leaf
  bool InsertIntoLeafNode_(RTreeNode* node, const RTreeEntry<TItem>& item) {
    size_t old_node_size = node->GetSize();
    
    node->InsertOrReplaceInLeaf(item);

    if (node->GetSize() == old_node_size) {
      // the node size hasn't been increased, this means the item
      // was replaced instead of inserted
      return true;
    }

    if (node->GetSize() > leaf_capacity_) {
      SplitNode_(node, item);
    } else {
      node->AdjustLHVAndPropagate(item.GetHilbertValue());
      node->AdjustMBRAndPropagate(item.GetBoundingRectangle());
    }
    return false;
  }

  // Note: parameter added_item is needed so we can call AdjustLHV/MBRAndPropagate() instead of RecalculateLHVAndMBR()
  // since we know that the only value which could increase the node's LHV and MBR is this item.
  void SplitNode_(RTreeNode* node, const RTreeEntry<TItem>& added_item) {
    if (node->parent_ == nullptr) {
      // node is the root
      assert(node == root_);

      RTreeNode* left_root_child = root_->Split();
      std::list<RTreeNode*> new_root_children = { left_root_child, root_ };
      root_ = new RTreeNode(new_root_children);
      return;
    }

    RTreeNode* left_node = node->Split();
    node->parent_->InsertInNonLeafAtPositionOfAnotherNode(left_node, node);
    if (node->parent_->GetSize() > non_leaf_capacity_) {
      SplitNode_(node->parent_, added_item);
    } else {
      node->parent_->AdjustLHVAndPropagate(added_item.GetHilbertValue());
      node->parent_->AdjustMBRAndPropagate(added_item.GetBoundingRectangle());
    }
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

  // Choose the leaf node in which to place a new point.
  RTreeNode* ChooseLeaf_(const VarLenPoint2DWithHilbertValue& point) {
    RTreeNode *node = root_;
    while (true) {
      assert(node != nullptr);
      
      if (node->IsLeaf())
        return node;
      
      bool next_node_found = false;
      for (auto iter = node->non_leaf_children_.begin(); iter != node->non_leaf_children_.end(); ++iter) {
        if (point.GetHilbertValue() >= (*iter)->GetLHV()) {
          node = *iter;
          next_node_found = true;
          break;
        }
      }
      if (!next_node_found) {
        node = node->non_leaf_children_.back();
      }
    }
  }

  size_t number_length_;
  size_t leaf_capacity_;
  size_t non_leaf_capacity_;

  RTreeNode* root_;
};

} // namespace ROCKSDB_NAMESPACE
