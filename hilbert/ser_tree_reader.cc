#include "ser_tree_reader.h"

namespace ROCKSDB_NAMESPACE {

SERTreeReader::SERTreeReader(
  std::unique_ptr<RandomAccessFileReader> ser_tree_file,
  const IOOptions& io_options)
  : file_(std::move(ser_tree_file)),
    io_options_(io_options) { }

SERTreeReader::~SERTreeReader() { }

Status SERTreeReader::Open(
    std::unique_ptr<RandomAccessFileReader> ser_tree_file,
    const IOOptions& io_options,
    std::unique_ptr<SERTreeReader>* ser_tree_reader) {
  std::unique_ptr<SERTreeReader> reader = std::make_unique<SERTreeReader>(
    std::move(ser_tree_file), io_options);

  Status s = reader->file_->file()->GetFileSize(&reader->file_size_);
  if (UNLIKELY(!s.ok())) return s;

  if (reader->file_size_ <= sizeof(SERTreeFooter))
    return Status::InvalidArgument("SERTreeReader::Open(): the file is not an SER-tree file (size too small)");
  
  char scratch[sizeof(SERTreeFooter)];
  Slice footer_slice;
  s = reader->file_->Read(
    reader->io_options_,
    reader->file_size_ - sizeof(SERTreeFooter),
    sizeof(SERTreeFooter),
    &footer_slice,
    scratch,
    /*aligned_buf=*/nullptr);
  if (UNLIKELY(!s.ok())) return s;
  memcpy((void*)&reader->footer_,
         (void*)footer_slice.data(),
         sizeof(SERTreeFooter));

  if (reader->footer_.magic_number != MUSEUMDB2_SER_TREE_FILE_MAGIC_NUMBER)
    return Status::InvalidArgument("SERTreeReader::Open(): the file is not an SER-tree file (incorrect magic number)");
  if (reader->footer_.root_offset >= reader->file_size_ - sizeof(SERTreeFooter))
    return Status::InvalidArgument("SERTreeReader::Open(): invalid SER-tree file format (root position out of range)");
  
  *ser_tree_reader = std::move(reader);
  return Status::OK();
}

Status SERTreeReader::Find(const UInt64Rectangle& rectangle, std::vector<BlockHandle>* result) {

}

Status SERTreeReader::FindRec_(uint64_t node_offset, const UInt64Rectangle& rectangle, std::vector<BlockHandle>& result) {
  SERTreeNodeHeader node_header;
  char node_header_scratch[sizeof(node_header)];
  Slice node_header_slice;

  Status s = file_->Read(io_options_, node_offset, sizeof(node_header),
    &node_header_slice, node_header_scratch, /*aligned_buf=*/nullptr);
  if (UNLIKELY(!s.ok())) return s;
  memcpy((void*)&node_header, (void*)node_header_slice.data(), sizeof(node_header));

  if (node_header.is_leaf != 0) {
    std::vector<SERTreeLeafNodeChildRepr> children(node_header.num_children);
    char* children_scratch = new char[node_header.num_children * sizeof(SERTreeLeafNodeChildRepr)];
    Slice children_slice;

    s = file_->Read(io_options_, node_offset + sizeof(node_header),
      node_header.num_children * sizeof(SERTreeLeafNodeChildRepr),
      &children_slice, children_scratch, /*aligned_buf=*/nullptr);
    if (UNLIKELY(!s.ok())) {
      delete[] children_scratch;
      return s;
    }
    memcpy((void*)children.data(), (void*)children_slice.data(),
      node_header.num_children * sizeof(SERTreeLeafNodeChildRepr));
    delete[] children_scratch;

    for (const auto& child: children) {
      if (rectangle.Intersects(child.mbr)) {
        result.push_back(child.block_handle);
      }
    }
  } else {
    std::vector<SERTreeNodeInfo> children(node_header.num_children);
    char* children_scratch = new char[node_header.num_children * sizeof(SERTreeNodeInfo)];
    Slice children_slice;

    s = file_->Read(io_options_, node_offset + sizeof(node_header),
      node_header.num_children * sizeof(SERTreeNodeInfo),
      &children_slice, children_scratch, /*aligned_buf=*/nullptr);
    if (UNLIKELY(!s.ok())) {
      delete[] children_scratch;
      return s;
    }
    memcpy((void*)children.data(), (void*)children_slice.data(),
      node_header.num_children * sizeof(SERTreeNodeInfo));
    delete[] children_scratch;
    
    for (const auto& child: children) {
      if (UNLIKELY(child.offset >= file_size_ - sizeof(SERTreeFooter))) {
        s = Status::InvalidArgument("SERTreeReader::FindRec_(): invalid SER-tree file format: node offset in file out of range");
        return s;
      }

      if (rectangle.Intersects(child.mbr)) {
        s = FindRec_(child.offset, rectangle, result);
        if (UNLIKELY(!s.ok())) return s;
      }
    }
  }
  return Status::OK();
}

}
