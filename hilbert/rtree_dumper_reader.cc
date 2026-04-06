#include "hilbert/rtree_dumper_reader.h"

#include <vector>

namespace ROCKSDB_NAMESPACE {

// TODO(FedMam): Status::InvalidArgument is probably too uninformative

Status RTreeDumper::Dump(const RTree<uint32_t>& tree, WritableFile& file) {
  Status s = Status::OK();

  uint32_t root_position;
  s = DumpRec_(tree.root_, tree.number_length_, file, &root_position);
  if (!s.ok()) return s;

  // footer
  RTreeFileFooter footer;
  footer.number_length = tree.number_length_;
  footer.root_position = root_position;
  footer.magic_number = MUSEUMDB2_RTREE_MAGIC_NUMBER;
  s = file.Append(Slice(reinterpret_cast<const char*>(&footer), sizeof(RTreeFileFooter)));
  if (!s.ok()) return s;

  return file.Close();
}

Status RTreeDumper::DumpRec_(const RTreeNode* node, uint32_t number_length, WritableFile& file, uint32_t* node_start) {
  Status s = Status::OK();

  char is_leaf = node->IsLeaf() ? 1 : 0;
  uint32_t num_children = node->GetSize();

  // is leaf
  s = file.Append(Slice(&is_leaf, sizeof(char)));
  if (!s.ok()) return s;

  // MBR
  VarLenRectangle mbr = node->GetMBR();
  s = file.Append(Slice(mbr.GetLeft().GetReprLittleEndian().c_str(), number_length));
  if (!s.ok()) return s;
  s = file.Append(Slice(mbr.GetTop().GetReprLittleEndian().c_str(), number_length));
  if (!s.ok()) return s;
  s = file.Append(Slice(mbr.GetRight().GetReprLittleEndian().c_str(), number_length));
  if (!s.ok()) return s;
  s = file.Append(Slice(mbr.GetBottom().GetReprLittleEndian().c_str(), number_length));
  if (!s.ok()) return s;

  // number of children
  s = file.Append(Slice(reinterpret_cast<const char*>(&num_children), sizeof(uint32_t)));
  if (!s.ok()) return s;

  // children
  if (node->IsLeaf()) {
    *node_start = static_cast<uint32_t>(file.GetFileSize());

    for (auto iter = node->leaf_children_.begin(); iter != node->leaf_children_.end(); ++iter) {
      s = file.Append(Slice((*iter).GetPointWithoutHilbertValue().GetX().GetReprLittleEndian().c_str(), number_length));
      if (!s.ok()) return s;
      s = file.Append(Slice((*iter).GetPointWithoutHilbertValue().GetY().GetReprLittleEndian().c_str(), number_length));
      if (!s.ok()) return s;
      s = file.Append(Slice(reinterpret_cast<const char*>(&(*iter).GetItem()), sizeof(uint32_t)));
      if (!s.ok()) return s;
    }
  } else {
    std::vector<uint32_t> child_positions;
    for (auto iter = node->non_leaf_children_.begin(); iter != node->non_leaf_children_.end(); ++iter) {
      uint32_t child_pos;
      s = DumpRec_(*iter, number_length, file, &child_pos);
      if (!s.ok()) return s;
      child_positions.push_back(child_pos);
    }

    *node_start = static_cast<uint32_t>(file.GetFileSize());

    s = file.Append(Slice(reinterpret_cast<const char*>(child_positions.data()), sizeof(uint32_t) * child_positions.size()));
    if (!s.ok()) return s;
  }
}

Status RTreeReader::NewRTreeReader(RandomAccessFile& file, RTreeReader* out) {
  RTreeReader rtree_reader(file);
  Status s = Status::OK();

  uint64_t file_size_64;
  s = file.GetFileSize(&file_size_64);
  uint32_t file_size = static_cast<uint32_t>(file_size_64);

  if (file_size_64 >= UINT32_MAX)
    s = s.UpdateIfOk(Status::InvalidArgument("Invalid R-Tree file format: size too large"));
  if (file_size < sizeof(RTreeFileFooter))
    s = s.UpdateIfOk(Status::InvalidArgument("Invalid R-Tree file format: size too small"));
  
  if (!s.ok()) return s;

  rtree_reader.data_area_size_ = file_size - sizeof(RTreeFileFooter);

  char scratch[sizeof(RTreeFileFooter)];
  Slice footer_slice;

  s = file.Read(rtree_reader.data_area_size_,
                sizeof(RTreeFileFooter),
                &footer_slice,
                scratch);
  if (!s.ok()) return s;
  memcpy(&rtree_reader.footer_, footer_slice.data(), sizeof(RTreeFileFooter));

  if (rtree_reader.footer_.magic_number != MUSEUMDB2_RTREE_MAGIC_NUMBER)
    return Status::InvalidArgument("Invalid R-Tree file format: incorrect magic number");
  if (rtree_reader.footer_.number_length == 0)
    return Status::InvalidArgument("Invalid R-Tree file format: number length should be at least 1");
  if (rtree_reader.footer_.root_position >= rtree_reader.data_area_size_)
    return Status::InvalidArgument("Invalid R-Tree file format: invalid root node position");

  return Status::OK();
}

Status RTreeReader::SearchRec_(uint32_t node_pos, const VarLenRectangle& rect, std::vector<RTreeEntry<uint32_t>>& result) {
  if (node_pos >= data_area_size_)
    return Status::InvalidArgument(std::string("Error reading R-Tree file: node position ") + std::to_string(node_pos) + std::string(" is out of range"));
  Status s = Status::OK();

  uint32_t nl = footer_.number_length;

  uint32_t node_footer_size = sizeof(char) + 4 * nl + sizeof(uint32_t);
  Slice node_footer;
  char footer_scratch[node_footer_size];

  s = file_.Read(node_pos,
                 node_footer_size,
                 &node_footer,
                 footer_scratch);
  if (!s.ok()) return s;

  char is_leaf = *node_footer.data();
  const char *mbr_left = node_footer.data() + sizeof(char);
  const char *mbr_top = mbr_left + nl;
  const char *mbr_right = mbr_top + nl;
  const char *mbr_bottom = mbr_right + nl;
  uint32_t num_children = *reinterpret_cast<const uint32_t*>(mbr_bottom + nl);

  VarLenRectangle mbr(
    VarLenNumber(nl, std::string(mbr_left, nl)),
    VarLenNumber(nl, std::string(mbr_top, nl)),
    VarLenNumber(nl, std::string(mbr_right, nl)),
    VarLenNumber(nl, std::string(mbr_bottom, nl))
  );

  if (!rect.Intersects(mbr)) return Status::OK();

  if (is_leaf) {
    uint32_t node_children_data_size = (2 * nl + sizeof(uint32_t)) * num_children;
    Slice node_children_data;
    char node_scratch[node_children_data_size];

    s = file_.Read(node_pos + node_footer_size,
                   node_children_data_size,
                   &node_children_data,
                   node_scratch);

    const char* child_data = node_children_data.data();
    for (uint32_t i = 0; i < num_children; ++i) {
      VarLenPoint2D point(
        VarLenNumber(nl, std::string(child_data, nl)),
        VarLenNumber(nl, std::string(child_data + nl, nl))
      );
      uint32_t value = *reinterpret_cast<const char*>(child_data + 2 * nl);
      child_data += 2 * nl + sizeof(uint32_t);

      if (rect.Contains(point)) {
        result.push_back(RTreeEntry<uint32_t>(VarLenPoint2DWithHilbertValue(point), value));
      }
    }
  } else {
    uint32_t node_children_data_size = sizeof(uint32_t) * num_children;
    Slice node_children_data;
    char node_scratch[node_children_data_size];

    s = file_.Read(node_pos + node_footer_size,
                   node_children_data_size,
                   &node_children_data,
                   node_scratch);

    const char* child_data = node_children_data.data();
    for (uint32_t i = 0; i < num_children; ++i) {
      uint32_t child_pos = *reinterpret_cast<const uint32_t*>(child_data);
      child_data += sizeof(uint32_t);

      s = SearchRec_(child_pos, rect, result);
      if (!s.ok()) return s;
    }
  }

  return Status::OK();
}

} // namespace ROCKSDB_NAMESPACE
