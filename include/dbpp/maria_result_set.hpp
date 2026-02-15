// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp::MariaResultSet -- random-access result set for MariaDB/MySQL.
//
// Design:
//   - Wraps MYSQL_RES* with RAII (mysql_store_result based)
//   - Move-only (no copy)
//   - Supports SeekRow() via mysql_data_seek()
//   - API-compatible with Sqlite3ResultSet for Database<Backend> template

#pragma once

#include <cstdint>
#include <cstring>

#include <mysql.h>

#include "dbpp/error.hpp"

namespace dbpp {

class MariaDb;

// ---------------------------------------------------------------------------
// MariaResultSet
// ---------------------------------------------------------------------------

class MariaResultSet {
 public:
  MariaResultSet() = default;

  ~MariaResultSet() { Finalize(); }

  // Move
  MariaResultSet(MariaResultSet&& other) noexcept
      : res_(other.res_),
        row_(other.row_),
        fields_(other.fields_),
        num_rows_(other.num_rows_),
        num_cols_(other.num_cols_),
        current_row_(other.current_row_) {
    other.res_ = nullptr;
    other.row_ = nullptr;
    other.fields_ = nullptr;
    other.num_rows_ = 0;
    other.num_cols_ = 0;
    other.current_row_ = 0;
  }

  MariaResultSet& operator=(MariaResultSet&& other) noexcept {
    if (this != &other) {
      Finalize();
      res_ = other.res_;
      row_ = other.row_;
      fields_ = other.fields_;
      num_rows_ = other.num_rows_;
      num_cols_ = other.num_cols_;
      current_row_ = other.current_row_;
      other.res_ = nullptr;
      other.row_ = nullptr;
      other.fields_ = nullptr;
      other.num_rows_ = 0;
      other.num_cols_ = 0;
      other.current_row_ = 0;
    }
    return *this;
  }

  // No copy
  MariaResultSet(const MariaResultSet&) = delete;
  MariaResultSet& operator=(const MariaResultSet&) = delete;

  // --- Field info ---

  int32_t NumFields() const { return num_cols_; }
  uint32_t NumRows() const { return static_cast<uint32_t>(num_rows_); }

  int32_t FieldIndex(const char* name) const {
    if (fields_ == nullptr || name == nullptr) { return -1; }
    for (int32_t i = 0; i < num_cols_; ++i) {
      if (fields_[i].name != nullptr &&
          std::strcmp(name, fields_[i].name) == 0) {
        return i;
      }
    }
    return -1;
  }

  const char* FieldName(int32_t col) const {
    if (fields_ == nullptr || col < 0 || col >= num_cols_) {
      return nullptr;
    }
    return fields_[col].name;
  }

  // --- Field values ---

  const char* FieldValue(int32_t col) const {
    if (row_ == nullptr || col < 0 || col >= num_cols_) {
      return nullptr;
    }
    return row_[col];
  }

  const char* FieldValue(const char* name) const {
    int32_t col = FieldIndex(name);
    return (col >= 0) ? FieldValue(col) : nullptr;
  }

  bool FieldIsNull(int32_t col) const {
    if (row_ == nullptr || col < 0 || col >= num_cols_) { return true; }
    return row_[col] == nullptr;
  }

  // --- Navigation ---

  bool Eof() const {
    return current_row_ >= num_rows_;
  }

  void NextRow() {
    if (res_ == nullptr || current_row_ >= num_rows_) { return; }
    ++current_row_;
    if (current_row_ < num_rows_) {
      row_ = mysql_fetch_row(res_);
    } else {
      row_ = nullptr;
    }
  }

  void SeekRow(uint32_t row) {
    if (res_ == nullptr || num_rows_ == 0) { return; }
    uint64_t target = row;
    if (target >= num_rows_) {
      target = num_rows_ - 1;
    }
    current_row_ = target;
    mysql_data_seek(res_, target);
    row_ = mysql_fetch_row(res_);
  }

  uint32_t CurrentRow() const { return static_cast<uint32_t>(current_row_); }

  void Finalize() {
    if (res_ != nullptr) {
      mysql_free_result(res_);
      res_ = nullptr;
    }
    row_ = nullptr;
    fields_ = nullptr;
    num_rows_ = 0;
    num_cols_ = 0;
    current_row_ = 0;
  }

 private:
  friend class MariaDb;

  MariaResultSet(MYSQL_RES* res)
      : res_(res) {
    if (res_ != nullptr) {
      num_rows_ = mysql_num_rows(res_);
      num_cols_ = static_cast<int32_t>(mysql_num_fields(res_));
      fields_ = mysql_fetch_fields(res_);
      if (num_rows_ > 0) {
        row_ = mysql_fetch_row(res_);
      }
    }
  }

  MYSQL_RES* res_ = nullptr;
  MYSQL_ROW row_ = nullptr;
  MYSQL_FIELD* fields_ = nullptr;
  uint64_t num_rows_ = 0;
  int32_t num_cols_ = 0;
  uint64_t current_row_ = 0;
};

}  // namespace dbpp
