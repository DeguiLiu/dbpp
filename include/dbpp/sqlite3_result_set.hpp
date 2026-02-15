// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp::Sqlite3ResultSet -- random-access result set.
//
// Design:
//   - Wraps sqlite3_get_table() result with RAII
//   - Move-only (no copy)
//   - Supports SeekRow() for random access
//   - Forward iteration via Eof()/NextRow()

#pragma once

#include <cstdint>
#include <cstring>

#include "sqlite3.h"

#include "dbpp/error.hpp"

namespace dbpp {

class Sqlite3Db;

// ---------------------------------------------------------------------------
// Sqlite3ResultSet
// ---------------------------------------------------------------------------

class Sqlite3ResultSet {
 public:
  Sqlite3ResultSet() = default;

  ~Sqlite3ResultSet() { Finalize(); }

  // Move
  Sqlite3ResultSet(Sqlite3ResultSet&& other) noexcept
      : results_(other.results_),
        num_rows_(other.num_rows_),
        num_cols_(other.num_cols_),
        current_row_(other.current_row_) {
    other.results_ = nullptr;
    other.num_rows_ = 0;
    other.num_cols_ = 0;
    other.current_row_ = 0;
  }

  Sqlite3ResultSet& operator=(Sqlite3ResultSet&& other) noexcept {
    if (this != &other) {
      Finalize();
      results_ = other.results_;
      num_rows_ = other.num_rows_;
      num_cols_ = other.num_cols_;
      current_row_ = other.current_row_;
      other.results_ = nullptr;
      other.num_rows_ = 0;
      other.num_cols_ = 0;
      other.current_row_ = 0;
    }
    return *this;
  }

  // No copy
  Sqlite3ResultSet(const Sqlite3ResultSet&) = delete;
  Sqlite3ResultSet& operator=(const Sqlite3ResultSet&) = delete;

  // --- Field info ---

  int32_t NumFields() const { return num_cols_; }
  uint32_t NumRows() const { return num_rows_; }

  int32_t FieldIndex(const char* name) const {
    if (results_ == nullptr || name == nullptr) { return -1; }
    for (int32_t i = 0; i < num_cols_; ++i) {
      if (results_[i] != nullptr && std::strcmp(name, results_[i]) == 0) {
        return i;
      }
    }
    return -1;
  }

  const char* FieldName(int32_t col) const {
    if (results_ == nullptr || col < 0 || col >= num_cols_) {
      return nullptr;
    }
    return results_[col];
  }

  // --- Field values ---

  const char* FieldValue(int32_t col) const {
    if (results_ == nullptr || col < 0 || col >= num_cols_) {
      return nullptr;
    }
    int32_t idx = RealIndex(col);
    if (idx < 0) { return nullptr; }
    return results_[idx];
  }

  const char* FieldValue(const char* name) const {
    int32_t col = FieldIndex(name);
    return (col >= 0) ? FieldValue(col) : nullptr;
  }

  bool FieldIsNull(int32_t col) const {
    if (results_ == nullptr || col < 0 || col >= num_cols_) { return true; }
    int32_t idx = RealIndex(col);
    if (idx < 0) { return true; }
    return results_[idx] == nullptr;
  }

  // --- Navigation ---

  bool Eof() const {
    return current_row_ >= num_rows_;
  }

  void NextRow() {
    if (current_row_ < num_rows_) { ++current_row_; }
  }

  void SeekRow(uint32_t row) {
    if (row >= num_rows_ && num_rows_ > 0) {
      current_row_ = num_rows_ - 1;
    } else {
      current_row_ = row;
    }
  }

  uint32_t CurrentRow() const { return current_row_; }

  void Finalize() {
    if (results_ != nullptr) {
      sqlite3_free_table(results_);
      results_ = nullptr;
    }
    num_rows_ = 0;
    num_cols_ = 0;
    current_row_ = 0;
  }

 private:
  friend class Sqlite3Db;

  Sqlite3ResultSet(char** results, uint32_t rows, int32_t cols)
      : results_(results),
        num_rows_(rows),
        num_cols_(cols),
        current_row_(0) {}

  int32_t RealIndex(int32_t col) const {
    // sqlite3_get_table layout: first num_cols_ entries are column names,
    // then row data follows.
    int32_t idx = static_cast<int32_t>(current_row_) * num_cols_ +
                  num_cols_ + col;
    return idx;
  }

  char** results_ = nullptr;
  uint32_t num_rows_ = 0;
  int32_t num_cols_ = 0;
  uint32_t current_row_ = 0;
};

}  // namespace dbpp
