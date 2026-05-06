#include "table/hilbert/hilbert_table_builder.h"

#include "hilbert/util.h"

namespace ROCKSDB_NAMESPACE {

HilbertTableBuilder::HilbertTableBuilder(const BlockBasedTableOptions& table_options,
                                         const TableBuilderOptions& table_builder_options,
                                         WritableFileWriter* file,
                                         std::unique_ptr<WritableFileWriter> ser_tree_file,
                                         const IOOptions& io_options,
                                         Env* env,
                                         uint32_t leaf_capacity,
                                         uint32_t non_leaf_capacity)
  : BlockBasedTableBuilder(table_options, table_builder_options, file),
    ser_tree_builder(std::make_unique<SERTreeBuilder>(std::move(ser_tree_file), io_options, env, leaf_capacity, non_leaf_capacity)) { }

void HilbertTableBuilder::Add(const Slice& ikey, const Slice& value) {
  if (LIKELY(ser_tree_status.ok())) {
    bool parallel_compression_active = IsParallelCompressionActive();
    if (parallel_compression_active)
      parallel_emitting_mutex.Lock();

    ValueType value_type;
    SequenceNumber seq;
    UnPackSequenceAndType(ExtractInternalKeyFooter(ikey), &seq, &value_type);
    
    if (IsValueType(value_type)) {
      HilbertCode hilbert_code;
      if (!TryParseHilbertCode(ikey, &hilbert_code)) {
        ser_tree_status = IOStatus::InvalidArgument("HilbertTableBuilder::Add(): invalid Hilbert code key format (should be exactly 16 bytes)");
      } else {
        current_mbr = current_mbr.MBR(HilbertCodeToPoint(hilbert_code));
      }
    }

    if (parallel_compression_active)
      parallel_emitting_mutex.Unlock();
  }

  BlockBasedTableBuilder::Add(ikey, value);
}

void HilbertTableBuilder::Flush(const Slice* first_key_in_next_block) {
  BlockBasedTableBuilder::Flush(first_key_in_next_block);

  // Writing next block
  // If parallel compression is active, then this will be
  // handled in NotifyOnPreparingIndexEntry(), which is called by
  // EmitBlockForParallel(), which is called by Flush()
  // TODO (FedMam): maybe add last internal key?
  if (!IsParallelCompressionActive()) {
    if (LIKELY(ser_tree_status.ok())) {
      ser_tree_status = ser_tree_builder->Add(PendingHandle(), current_mbr);
      current_mbr = UInt64Rectangle::CreateInvalidRectangle();  // reset MBR

      if (UNLIKELY(!ser_tree_ok()))
        ser_tree_builder->Abandon();
    }
  }
}

void HilbertTableBuilder::NotifyOnPreparingIndexEntry(void* index_entry_pointer) {
  assert(IsParallelCompressionActive());
  
  if (LIKELY(ser_tree_status.ok())) {
    parallel_emitting_mutex.Lock();

    map_for_parallel_emitting[index_entry_pointer] = current_mbr;
    current_mbr = UInt64Rectangle::CreateInvalidRectangle();

    parallel_emitting_mutex.Unlock();
  }
}

void HilbertTableBuilder::NotifyOnFinishingIndexEntry(const BlockHandle& pending_handle, void* index_entry_pointer) {
  assert(IsParallelCompressionActive());

  if (LIKELY(ser_tree_status.ok())) {
    parallel_emitting_mutex.Lock();

    assert(map_for_parallel_emitting.find(index_entry_pointer) != map_for_parallel_emitting.end());
    ser_tree_status = ser_tree_builder->Add(pending_handle, map_for_parallel_emitting[index_entry_pointer]);
    map_for_parallel_emitting.erase(index_entry_pointer);

    if (UNLIKELY(ser_tree_status.ok())) {
      ser_tree_builder->Abandon();
    }

    parallel_emitting_mutex.Unlock();
  }
}

Status HilbertTableBuilder::Finish() {
  Status s = BlockBasedTableBuilder::Finish();

  bool parallel_compression_active = IsParallelCompressionActive();
  if (parallel_compression_active)
    parallel_emitting_mutex.Lock();

  if (LIKELY(ser_tree_status.ok())) {
    ser_tree_status = ser_tree_builder->Finish();

    if (UNLIKELY(!ser_tree_ok()))
      ser_tree_builder->Abandon();
  } else {
    ser_tree_builder->Abandon();
  }

  if (parallel_compression_active)
    parallel_emitting_mutex.Unlock();
  
  return s;
}

void HilbertTableBuilder::Abandon() {
  bool parallel_compression_active = IsParallelCompressionActive();
  if (parallel_compression_active)
    parallel_emitting_mutex.Lock();
  
  IOStatus s = ser_tree_builder->Abandon();
  if (ser_tree_ok())
    ser_tree_status = s;
  
  if (parallel_compression_active)
    parallel_emitting_mutex.Unlock();

  BlockBasedTableBuilder::Abandon();
}

} // namespace ROCKSDB_NAMESPACE
