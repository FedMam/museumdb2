#include "hilbert/rtree.h"

#include <assert.h>

#include <stdexcept>

namespace ROCKSDB_NAMESPACE {

template<typename TItem>
struct RTreeNode {
 public:
  // Will construct an empty node
  RTreeNode()
    : parent_(nullptr),
      non_leaf_children_(),
      leaf_children_() { }

  // Will construct a non-leaf node
  explicit RTreeNode(const std::list<std::unique_ptr<RTreeNode<TItem>>>& non_leaf_children)
    : parent_(nullptr),
      non_leaf_children_(non_leaf_children),
      leaf_children_() {
    assert(!non_leaf_children.empty());
    for (auto iter = non_leaf_children.begin(); iter != non_leaf_children.end(); ++iter)
      (*iter)->parent_ = this;
    RecalculateLHVAndMBR();
  }

  // Will construct a leaf node
  explicit RTreeNode(const std::list<RTreeLeafNodeEntry<TItem>>& leaf_children)
    : parent_(nullptr),
      non_leaf_children_(),
      leaf_children_(leaf_children) {
    assert(!leaf_children_.empty());
    RecalculateLHVAndMBR();
  }

  inline bool IsLeaf() const { return IsEmpty() || !leaf_children_.empty(); }

  inline bool IsEmpty() const {
    return leaf_children_.empty() && non_leaf_children_.empty()
  }

  inline size_t GetSize() const {
    if (IsLeaf()) return leaf_children_.size();
    return non_leaf_children_.size();
  }

  inline RTreeNode<TItem>* GetParent() const { return parent_; }

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
      lhv_ = leaf_children.front().GetHilbertValue();
      mbr_ = leaf_children.front().GetBoundingRectangle();

      auto iter = leaf_children.begin();
      ++iter;
      while (iter != leaf_children.end()) {
        lhv_ = std::max(lhv_, (*iter).GetHilbertValue());
        mbr_ = mbr_.MBR((*iter).GetBoundingRectangle());
        ++iter;
      }
    } else {
      lhv_ = non_leaf_children.front()->GetLHV();
      mbr_ = non_leaf_children.front()->GetMBR();

      auto iter = non_leaf_children.begin();
      ++iter;
      while (iter != non_leaf_children.end()) {
        lhv_ = std::max(lhv_, (*iter)->GetLHV());
        mbr_ = mbr_.MBR((*iter)->GetMBR());
        ++iter;
      }
    }
    if (propagate)
      if (parent_)
        parent_->RecalculateLHVAndMBR(propagate);
  }

  // Fails if lhv_ and mbr_ are not equal to the actual LHV and MBR
  // of all children. Checks not only this node but its whole subtree.
  // Should be only used in tests.
  void VerifyLHVAndMBRInSubTree() {
    VarLenNumber old_lhv = lhv_;
    VarLenRectangle old_mbr = mbr_;
    
    RecalculateLHVAndMBR(false);

    assert (lhv_ == old_lhv && mbr_ == old_mbr);

    if (!IsLeaf()) {
      for (auto iter = non_leaf_children_.begin(); iter != non_leaf_children_.end(); ++iter) {
        (*iter)->VerifyLHVAndMBRInSubTree();
      }
    }
  }

  // Inserts a new item into the node (replaces if the item with the same Hilbert value already exists).
  // Warning: throws an error if this node is not a leaf node.
  // Warning: recalculates MBR and LHV, but does not propagate changes upward. Do not forget to do it manually.
  void InsertOrReplaceInLeaf(const RTreeLeafNodeEntry<TItem>& item) {
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
  // Warning: new_node will be owned by unique_ptr upon inserting.
  // Warning: throws an error if this node is not a non-leaf node or
  // node_to_position was not found in this node.
  // Warning: recalculates MBR and LHV, but does not propagate changes upward. Do not forget to do it manually.
  void InsertInNonLeafAtPositionOfAnotherNode(RTreeNode<TItem>* new_node, RTreeNode<TItem>* node_to_position) {
    assert(!isLeaf());

    for (auto iter = non_leaf_children_.begin(); iter != non_leaf_children_.end(); ++iter) {
      if ((*iter).get() == node_to_position) {
        non_leaf_children_.insert(std::make_unique<new_node>);
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
  RTreeNode<TItem>* Split() {
    if (IsLeaf()) {
      size_t left_size = leaf_children_.size() / 2;

      std::list<RTreeLeafNodeEntry<TItem>> left_leaf_children;
      left_leaf_children.splice(left_leaf_children.begin(), leaf_children_, leaf_children_.begin(), leaf_children_.begin() + left_size);
      RecalculateLHVAndMBR(false);

      return new RTreeNode<TItem>(left_leaf_children);
    } else {
      size_t leaf_size = non_leaf_children_.size() / 2;

      std::list<std::unique_ptr<RTreeNode<TItem>>> left_non_leaf_children;
      left_non_leaf_children.splice(left_non_leaf_children.begin(), non_leaf_children_, non_leaf_children_.begin(), non_leaf_children_.begin() + left_size);
      RecalculateLHVAndMBR(false);

      return new RTreeNode<TItem>(left_non_leaf_children);
    }
  }

  friend class RTree<TItem>;

 private:
  RTreeNode<TItem>* parent_;

  // Convention: either non_leaf_children_ or leaf_children_ is empty,
  // and this determines if this node is a leaf or non-leaf node.
  // Only root node is allowed to have both lists empty. Then it
  // counts as a leaf node.
  // We do this to not mess up with inheritance and
  // dynamic_cast's which add unnecessary runtime overhead.
  // For nothing to go wrong, we define appropriate constructors.

  std::list<std::unique_ptr<RTreeNode<TItem>>> non_leaf_children_;
  std::list<RTreeLeafNodeEntry<TItem>> leaf_children_;

  VarLenNumber lhv_;  // largest Hilbert value
  VarLenRectangle mbr_;  // minimum bounding rectangle
};

template<typename TItem>
std::optional<RTreeLeafNodeEntry<TItem>> RTree<TItem>::Find(const VarLenPoint2D& key_point) const {
  std::vector<RTreeLeafNodeEntry<TItem>> result;

  SearchRec_(root_.get(), VarLenRectangle(key_point, key_point), result);

  if (result.empty())
    return std::optional<RTreeLeafNodeEntry<TItem>>();
  // there can only be one object at a single point
  return result[0];
}

template<typename TItem>
std::vector<RTreeLeafNodeEntry<TItem>> RTree<TItem>::RangeQuery(const VarLenRectangle& rect) const {
  std::vector<RTreeLeafNodeEntry<TItem> result;

  SearchRec_(root_.get(), rect, result);

  return result;
}

template<typename TItem>
bool RTree<TItem>::InsertOrReplace(const VarLenPoint2DWithHilbertValue& key_point, const TItem* item) {
  VerifyPointLength_(key_point);

  RTreeNode<TItem>* leaf_node = ChooseLeaf_(key_point);

  return InsertIntoLeafNode_(leaf_node, RTreeLeafNodeEntry(key_point, item));
}

template<typename TItem>
bool RTree<TItem>::InsertIntoLeafNode_(RTreeNode<TItem>* node, const RTreeLeafNodeEntry<TItem>& item) {
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

template<typename TItem>
void RTree<TItem>::SplitNode_(RTreeNode<TItem>* node, const RTreeLeafNodeEntry<TItem>& added_item) {
  if (node->parent_ == nullptr) {
    // node is the root
    assert(node == root_.get());

    RTree<TItem>* left_root_child = root_->Split();
    std::list<std::unique_ptr<RTreeNode<TItem>>> new_root_children = { std::make_unique(left_root_child), std::move(root_) };
    root_ = std::make_unique(new RTreeNode<TItem>(new_root_children));
    return;
  }

  RTree<TItem>* left_node = node->Split();
  node->parent_->InsertInNonLeafAtPositionOfAnotherNode(left_node, node);
  if (node->parent_->GetSize() > non_leaf_capacity_) {
    SplitNode_(node->parent_, added_item);
  } else {
    node->parent_->AdjustLHVAndPropagate(added_item.GetHilbertValue());
    node->parent_->AdjustMBRAndPropagate(added_item.GetBoundingRectangle());
  }
}

template<typename TItem>
void RTree<TItem>::SearchRec_(RTreeNode<TItem>* curr_node, const VarLenRectangle& rect, std::vector<RTreeLeafNodeEntry<TItem>>& result) const {
  assert(node != nullptr);

  if (curr_node->IsLeaf()) {
    for (auto iter = curr_node->leaf_children_.begin(); iter != curr_node->leaf_children_.end(); ++iter) {
      if (rect.Contains((*iter).GetPointWithoutHilbertValue()))
        result.push_back((*iter));
    }
  } else {
    for (auto iter = curr_node->non_leaf_children_.begin(); iter != curr_node->non_leaf_children_.end(); ++iter) {
      if (!rect.Intersects((*iter)->GetMBR()))
        continue;
      
      SearchRec_((*iter).get(), rect, result);
    }
  }
}

template<typename TItem>
RTreeNode<TItem>* RTree<TItem>::ChooseLeaf_(const VarLenPoint2DWithHilbertValue& point) {
  RTreeNode<TItem> *node = root_.get();
  while (true) {
    assert(node != nullptr);
    
    if (node->IsLeaf())
      return node;
    
    bool next_node_found = false;
    for (auto iter = node->non_leaf_children_.begin(); iter != node->non_leaf_children_.end(); ++iter) {
      if (point.GetHilbertValue() >= (*iter)->GetLHV()) {
        node = (*iter).get();
        next_node_found = true;
        break;
      }
    }
    if (!next_node_found) {
      node = node->non_leaf_children_.back().get();
    }
  }
}

} // namespace ROCKSDB_NAMESPACE
