#include "table/hilbert/hilbert_table_builder.h"

#include "hilbert/util.h"

namespace ROCKSDB_NAMESPACE {

HilbertTableBuilder::HilbertTableBuilder(const BlockBasedTableOptions& table_options,
                                         const TableBuilderOptions& table_builder_options,
                                         WritableFileWriter* file,
                                         std::unique_ptr<WritableFileWriter> ser_tree_file,
                                         const IOOptions& io_options,
                                         Env* env)
  : BlockBasedTableBuilder(table_options, table_builder_options, file),
    ser_tree_builder(std::make_unique<SERTreeBuilder>(std::move(ser_tree_file), io_options, env)) { }

void HilbertTableBuilder::Add(const Slice& ikey, const Slice& value) {
  if (LIKELY(ser_tree_status.ok())) {
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
  }

  BlockBasedTableBuilder::Add(ikey, value);
}

void HilbertTableBuilder::Flush(const Slice* first_key_in_next_block) {
  // writing next block
  // TODO (FedMam): maybe add last internal key?
  if (LIKELY(ser_tree_status.ok())) {
    ser_tree_status = ser_tree_builder->Add(PendingHandle(), current_mbr);
    current_mbr = UInt64Rectangle::CreateInvalidRectangle();  // reset MBR

    if (UNLIKELY(!ser_tree_ok()))
      ser_tree_builder->Abandon();
  }

  BlockBasedTableBuilder::Flush(first_key_in_next_block);
}

Status HilbertTableBuilder::Finish() {
  if (LIKELY(ser_tree_status.ok())) {
    ser_tree_status = ser_tree_builder->Finish();

    if (UNLIKELY(!ser_tree_ok()))
      ser_tree_builder->Abandon();
  } else {
    ser_tree_builder->Abandon();
  }

  return BlockBasedTableBuilder::Finish();
}

void HilbertTableBuilder::Abandon() {
  IOStatus s = ser_tree_builder->Abandon();
  if (ser_tree_ok())
    ser_tree_status = s;

  BlockBasedTableBuilder::Abandon();
}

} // namespace ROCKSDB_NAMESPACE
