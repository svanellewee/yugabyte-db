// Copyright (c) YugaByte, Inc.

#ifndef YB_DOCDB_DOC_ROWWISE_ITERATOR_H_
#define YB_DOCDB_DOC_ROWWISE_ITERATOR_H_

#include <string>
#include <atomic>

#include "rocksdb/db.h"

#include "yb/common/encoded_key.h"
#include "yb/common/iterator.h"
#include "yb/common/rowblock.h"
#include "yb/common/scan_spec.h"
#include "yb/common/hybrid_time.h"
#include "yb/docdb/doc_key.h"
#include "yb/docdb/value.h"
#include "yb/docdb/yql_scanspec.h"
#include "yb/util/status.h"
#include "yb/util/pending_op_counter.h"

namespace yb {
namespace docdb {

// An adapter between SQL-mapped-to-document-DB and Kudu's RowwiseIterator.
class DocRowwiseIterator : public RowwiseIterator {

 public:
  DocRowwiseIterator(const Schema &projection,
                     const Schema &schema,
                     rocksdb::DB *db,
                     HybridTime hybrid_time = HybridTime::kMax,
                     yb::util::PendingOperationCounter* pending_op_counter = nullptr);
  virtual ~DocRowwiseIterator();

  virtual CHECKED_STATUS Init(ScanSpec *spec) OVERRIDE;

  // This must always be called before NextBlock. The implementation actually finds the first row
  // to scan, and NextBlock expects the RocksDB iterator to already be properly positioned.
  virtual bool HasNext() const OVERRIDE;

  virtual std::string ToString() const OVERRIDE;

  virtual const Schema& schema() const OVERRIDE {
    // Note: this is the schema only for the columns in the projection, not all columns.
    return projection_;
  }

  // This may return one row at a time in the initial implementation, even though Kudu's scanning
  // interface supports returning multiple rows at a time.
  virtual CHECKED_STATUS NextBlock(RowBlock *dst) OVERRIDE;

  virtual void GetIteratorStats(std::vector<IteratorStats>* stats) const OVERRIDE;

  // Init YQL read scan.
  CHECKED_STATUS Init(const YQLScanSpec& spec);

  // Read next row into a value map.
  CHECKED_STATUS NextRow(const YQLScanSpec& spec, YQLValueMap* value_map);

  // Read next set of rows into YQL row block (note: we are reading just 1 row per call for now).
  CHECKED_STATUS NextBlock(const YQLScanSpec& spec, YQLRowBlock* rowblock);

 private:
  DocKey KuduToDocKey(const EncodedKey &encoded_key) {
    return DocKey::FromKuduEncodedKey(encoded_key, schema_);
  }

  // Get the non-key column values of a YQL row.
  CHECKED_STATUS GetValues(const Schema& projection, vector<PrimitiveValue>* values);

  // Processes a value for a column(subdoc_key) and determines if the value is valid or not based on
  // the hybrid time of subdoc_key. If valid, it is added to the values vector and is_null is set
  // to false. Otherwise, is_null is set to true.
  CHECKED_STATUS ProcessValues(const Value& value, const SubDocKey& subdoc_key,
                               vector<PrimitiveValue>* values,
                               bool *is_null) const;

  // Figures out whether the current sub_doc_key with the given top_level_value is a valid column
  // that has not expired. Sets column_found to true if this is a valid column, false otherwise.
  CHECKED_STATUS FindValidColumn(bool* column_found) const;

  // Figures out whether we have a valid column present indicating the existence of the row.
  // Sets column_found to true if a valid column is found, false otherwise.
  CHECKED_STATUS ProcessColumnsForHasNext(bool* column_found) const;

  // Verifies whether or not the column pointed to by subdoc_key is deleted by the current
  // row_delete_marker_key_.
  bool IsDeletedByRowDeletion(const SubDocKey& subdoc_key) const;

  // Given a subdoc_key pointing to a column and its associated value, determine whether or not
  // the column is valid based on TTL expiry, row level delete markers and column delete markers
  CHECKED_STATUS CheckColumnValidity(const SubDocKey& subdoc_key,
                                     const Value& value,
                                     bool* is_valid) const;

  const Schema& projection_;

  // The schema for all columns, not just the columns we're scanning.
  const Schema& schema_;

  HybridTime hybrid_time_;
  rocksdb::DB* const db_;

  // A copy of the exclusive upper bound key of the scan range (if any).
  bool has_upper_bound_key_;
  KeyBytes exclusive_upper_bound_key_;

  std::unique_ptr<rocksdb::Iterator> db_iter_;

  // We keep the "pending operation" counter incremented for the lifetime of this iterator so that
  // RocksDB does not get destroyed while the iterator is still in use.
  yb::util::ScopedPendingOperation pending_op_;

  // The mutable fields that follow are modified by HasNext, a const method.

  // Indicates whether we've already finished iterating.
  mutable bool done_;

  // HasNext sets this to the subdocument key corresponding to the top of the document
  // (document key and a generation hybrid time).
  mutable SubDocKey subdoc_key_;

  // HasNext sets this to the value of the first valid column found for a given row.
  mutable Value top_level_value_;

  // While iterating within a row we keep the delete timestamp for the row, to determine which
  // columns are valid.
  mutable HybridTime row_delete_marker_time_;
  mutable DocKey row_delete_marker_key_;

  // Used for keeping track of errors that happen in HasNext. Returned
  mutable Status status_;
};

}  // namespace docdb
}  // namespace yb

#endif  // YB_DOCDB_DOC_ROWWISE_ITERATOR_H_
