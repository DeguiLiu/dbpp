// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp::MariaQuery -- forward-only query result for MariaDB/MySQL.
//
// Design:
//   - Wraps MYSQL_RES* (mysql_store_result) with RAII
//   - Move-only (no copy)
//   - Forward iteration via Eof()/NextRow()
//   - Type-safe field accessors with null defaults
//   - API-compatible with Sqlite3Query for Database<Backend> template

#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <mysql.h>

#include "dbpp/error.hpp"

namespace dbpp {

class MariaDb;
class MariaStatement;

// ---------------------------------------------------------------------------
// MariaQuery
// ---------------------------------------------------------------------------

class MariaQuery {
 public:
  MariaQuery() = default;

  ~MariaQuery() { Finalize(); }

  // Move
  MariaQuery(MariaQuery&& other) noexcept
      : res_(other.res_),
        row_(other.row_),
        lengths_(other.lengths_),
        fields_(other.fields_),
        eof_(other.eof_),
        num_fields_(other.num_fields_),
        num_rows_(other.num_rows_) {
    other.res_ = nullptr;
    other.row_ = nullptr;
    other.lengths_ = nullptr;
    other.fields_ = nullptr;
    other.eof_ = true;
    other.num_fields_ = 0;
    other.num_rows_ = 0;
  }

  MariaQuery& operator=(MariaQuery&& other) noexcept {
    if (this != &other) {
      Finalize();
      res_ = other.res_;
      row_ = other.row_;
      lengths_ = other.lengths_;
      fields_ = other.fields_;
      eof_ = other.eof_;
      num_fields_ = other.num_fields_;
      num_rows_ = other.num_rows_;
      other.res_ = nullptr;
      other.row_ = nullptr;
      other.lengths_ = nullptr;
      other.fields_ = nullptr;
      other.eof_ = true;
      other.num_fields_ = 0;
      other.num_rows_ = 0;
    }
    return *this;
  }

  // No copy
  MariaQuery(const MariaQuery&) = delete;
  MariaQuery& operator=(const MariaQuery&) = delete;

  // --- Field info ---

  int32_t NumFields() const { return num_fields_; }

  int32_t FieldIndex(const char* name) const {
    if (fields_ == nullptr || name == nullptr) { return -1; }
    for (int32_t i = 0; i < num_fields_; ++i) {
      if (fields_[i].name != nullptr &&
          std::strcmp(name, fields_[i].name) == 0) {
        return i;
      }
    }
    return -1;
  }

  const char* FieldName(int32_t col) const {
    if (fields_ == nullptr || col < 0 || col >= num_fields_) {
      return nullptr;
    }
    return fields_[col].name;
  }

  // --- Field values ---

  const char* FieldValue(int32_t col) const {
    if (row_ == nullptr || col < 0 || col >= num_fields_) {
      return nullptr;
    }
    return row_[col];
  }

  const char* FieldValue(const char* name) const {
    int32_t idx = FieldIndex(name);
    return (idx >= 0) ? FieldValue(idx) : nullptr;
  }

  bool FieldIsNull(int32_t col) const {
    if (row_ == nullptr || col < 0 || col >= num_fields_) { return true; }
    return row_[col] == nullptr;
  }

  // --- Typed accessors ---

  int32_t GetInt(int32_t col, int32_t null_value = 0) const {
    if (FieldIsNull(col)) { return null_value; }
    return static_cast<int32_t>(std::strtol(row_[col], nullptr, 10));
  }

  int32_t GetInt(const char* name, int32_t null_value = 0) const {
    int32_t idx = FieldIndex(name);
    return (idx >= 0) ? GetInt(idx, null_value) : null_value;
  }

  int64_t GetInt64(int32_t col, int64_t null_value = 0) const {
    if (FieldIsNull(col)) { return null_value; }
    return static_cast<int64_t>(std::strtoll(row_[col], nullptr, 10));
  }

  double GetDouble(int32_t col, double null_value = 0.0) const {
    if (FieldIsNull(col)) { return null_value; }
    return std::strtod(row_[col], nullptr);
  }

  double GetDouble(const char* name, double null_value = 0.0) const {
    int32_t idx = FieldIndex(name);
    return (idx >= 0) ? GetDouble(idx, null_value) : null_value;
  }

  const char* GetString(int32_t col, const char* null_value = "") const {
    if (FieldIsNull(col)) { return null_value; }
    return row_[col];
  }

  const char* GetString(const char* name,
                        const char* null_value = "") const {
    int32_t idx = FieldIndex(name);
    return (idx >= 0) ? GetString(idx, null_value) : null_value;
  }

  const uint8_t* GetBlob(int32_t col, int32_t& out_len) const {
    out_len = 0;
    if (FieldIsNull(col) || lengths_ == nullptr) { return nullptr; }
    out_len = static_cast<int32_t>(lengths_[col]);
    return reinterpret_cast<const uint8_t*>(row_[col]);
  }

  const uint8_t* GetBlob(const char* name, int32_t& out_len) const {
    int32_t idx = FieldIndex(name);
    return (idx >= 0) ? GetBlob(idx, out_len) : nullptr;
  }

  // --- Navigation ---

  bool Eof() const { return eof_; }

  void NextRow() {
    if (res_ == nullptr) { return; }
    row_ = mysql_fetch_row(res_);
    if (row_ != nullptr) {
      lengths_ = mysql_fetch_lengths(res_);
    } else {
      eof_ = true;
      lengths_ = nullptr;
    }
  }

  void Finalize() {
    if (res_ != nullptr) {
      mysql_free_result(res_);
      res_ = nullptr;
    }
    row_ = nullptr;
    lengths_ = nullptr;
    fields_ = nullptr;
    eof_ = true;
    num_fields_ = 0;
    num_rows_ = 0;
  }

 private:
  friend class MariaDb;
  friend class MariaStatement;

  MariaQuery(MYSQL_RES* res, bool eof)
      : res_(res), eof_(eof) {
    if (res_ != nullptr) {
      num_fields_ = static_cast<int32_t>(mysql_num_fields(res_));
      num_rows_ = static_cast<uint64_t>(mysql_num_rows(res_));
      fields_ = mysql_fetch_fields(res_);
      if (!eof_) {
        row_ = mysql_fetch_row(res_);
        if (row_ != nullptr) {
          lengths_ = mysql_fetch_lengths(res_);
        } else {
          eof_ = true;
        }
      }
    }
  }

  MYSQL_RES* res_ = nullptr;
  MYSQL_ROW row_ = nullptr;
  unsigned long* lengths_ = nullptr;
  MYSQL_FIELD* fields_ = nullptr;
  bool eof_ = true;
  int32_t num_fields_ = 0;
  uint64_t num_rows_ = 0;
};

}  // namespace dbpp
