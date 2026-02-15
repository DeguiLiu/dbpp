// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp::Sqlite3Query -- forward-only query result set.
//
// Design:
//   - Wraps sqlite3_stmt* with RAII
//   - Move-only (no copy)
//   - Forward iteration via Eof()/NextRow()
//   - Type-safe field accessors with null defaults

#pragma once

#include <cstdint>
#include <cstring>

#include "sqlite3.h"

#include "dbpp/error.hpp"

namespace dbpp {

class Sqlite3Db;
class Sqlite3Statement;

// ---------------------------------------------------------------------------
// Sqlite3Query
// ---------------------------------------------------------------------------

class Sqlite3Query {
 public:
  Sqlite3Query() = default;

  ~Sqlite3Query() { Finalize(); }

  // Move
  Sqlite3Query(Sqlite3Query&& other) noexcept
      : db_(other.db_),
        stmt_(other.stmt_),
        eof_(other.eof_),
        num_fields_(other.num_fields_) {
    other.db_ = nullptr;
    other.stmt_ = nullptr;
    other.eof_ = true;
    other.num_fields_ = 0;
  }

  Sqlite3Query& operator=(Sqlite3Query&& other) noexcept {
    if (this != &other) {
      Finalize();
      db_ = other.db_;
      stmt_ = other.stmt_;
      eof_ = other.eof_;
      num_fields_ = other.num_fields_;
      other.db_ = nullptr;
      other.stmt_ = nullptr;
      other.eof_ = true;
      other.num_fields_ = 0;
    }
    return *this;
  }

  // No copy
  Sqlite3Query(const Sqlite3Query&) = delete;
  Sqlite3Query& operator=(const Sqlite3Query&) = delete;

  // --- Field info ---

  int32_t NumFields() const { return num_fields_; }

  int32_t FieldIndex(const char* name) const {
    if (stmt_ == nullptr || name == nullptr) { return -1; }
    for (int32_t i = 0; i < num_fields_; ++i) {
      const char* col_name = sqlite3_column_name(stmt_, i);
      if (col_name != nullptr && std::strcmp(name, col_name) == 0) {
        return i;
      }
    }
    return -1;
  }

  const char* FieldName(int32_t col) const {
    if (stmt_ == nullptr || col < 0 || col >= num_fields_) {
      return nullptr;
    }
    return sqlite3_column_name(stmt_, col);
  }

  int32_t FieldDataType(int32_t col) const {
    if (stmt_ == nullptr || col < 0 || col >= num_fields_) { return -1; }
    return sqlite3_column_type(stmt_, col);
  }

  // --- Field values ---

  const char* FieldValue(int32_t col) const {
    if (stmt_ == nullptr || col < 0 || col >= num_fields_) {
      return nullptr;
    }
    return reinterpret_cast<const char*>(sqlite3_column_text(stmt_, col));
  }

  const char* FieldValue(const char* name) const {
    int32_t idx = FieldIndex(name);
    return (idx >= 0) ? FieldValue(idx) : nullptr;
  }

  bool FieldIsNull(int32_t col) const {
    if (stmt_ == nullptr || col < 0 || col >= num_fields_) { return true; }
    return sqlite3_column_type(stmt_, col) == SQLITE_NULL;
  }

  // --- Typed accessors ---

  int32_t GetInt(int32_t col, int32_t null_value = 0) const {
    if (FieldIsNull(col)) { return null_value; }
    return sqlite3_column_int(stmt_, col);
  }

  int32_t GetInt(const char* name, int32_t null_value = 0) const {
    int32_t idx = FieldIndex(name);
    return (idx >= 0) ? GetInt(idx, null_value) : null_value;
  }

  int64_t GetInt64(int32_t col, int64_t null_value = 0) const {
    if (FieldIsNull(col)) { return null_value; }
    return sqlite3_column_int64(stmt_, col);
  }

  double GetDouble(int32_t col, double null_value = 0.0) const {
    if (FieldIsNull(col)) { return null_value; }
    return sqlite3_column_double(stmt_, col);
  }

  double GetDouble(const char* name, double null_value = 0.0) const {
    int32_t idx = FieldIndex(name);
    return (idx >= 0) ? GetDouble(idx, null_value) : null_value;
  }

  const char* GetString(int32_t col, const char* null_value = "") const {
    if (FieldIsNull(col)) { return null_value; }
    const char* val = FieldValue(col);
    return (val != nullptr) ? val : null_value;
  }

  const char* GetString(const char* name,
                        const char* null_value = "") const {
    int32_t idx = FieldIndex(name);
    return (idx >= 0) ? GetString(idx, null_value) : null_value;
  }

  const uint8_t* GetBlob(int32_t col, int32_t& out_len) const {
    out_len = 0;
    if (stmt_ == nullptr || col < 0 || col >= num_fields_) {
      return nullptr;
    }
    out_len = sqlite3_column_bytes(stmt_, col);
    return static_cast<const uint8_t*>(sqlite3_column_blob(stmt_, col));
  }

  const uint8_t* GetBlob(const char* name, int32_t& out_len) const {
    int32_t idx = FieldIndex(name);
    return (idx >= 0) ? GetBlob(idx, out_len) : nullptr;
  }

  // --- Navigation ---

  bool Eof() const { return eof_; }

  void NextRow() {
    if (stmt_ == nullptr) { return; }
    int32_t rc = sqlite3_step(stmt_);
    if (rc == SQLITE_DONE) {
      eof_ = true;
    } else if (rc != SQLITE_ROW) {
      eof_ = true;
    }
  }

  void Finalize() {
    if (stmt_ != nullptr) {
      sqlite3_finalize(stmt_);
      stmt_ = nullptr;
    }
    eof_ = true;
    num_fields_ = 0;
  }

 private:
  friend class Sqlite3Db;
  friend class Sqlite3Statement;

  Sqlite3Query(sqlite3* db, sqlite3_stmt* stmt, bool eof)
      : db_(db), stmt_(stmt), eof_(eof) {
    if (stmt_ != nullptr) {
      num_fields_ = sqlite3_column_count(stmt_);
    }
  }

  sqlite3* db_ = nullptr;
  sqlite3_stmt* stmt_ = nullptr;
  bool eof_ = true;
  int32_t num_fields_ = 0;
};

}  // namespace dbpp
