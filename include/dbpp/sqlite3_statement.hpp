// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp::Sqlite3Statement -- prepared statement with RAII.
//
// Design:
//   - Wraps sqlite3_stmt* with RAII
//   - Move-only (no copy)
//   - 1-based parameter binding (matches SQLite3 convention)
//   - ExecDml() for INSERT/UPDATE/DELETE, ExecQuery() for SELECT

#pragma once

#include <cstdint>

#include "sqlite3.h"

#include "dbpp/error.hpp"
#include "dbpp/sqlite3_query.hpp"

namespace dbpp {

class Sqlite3Db;

// ---------------------------------------------------------------------------
// Sqlite3Statement
// ---------------------------------------------------------------------------

class Sqlite3Statement {
 public:
  Sqlite3Statement() = default;

  ~Sqlite3Statement() { Finalize(); }

  // Move
  Sqlite3Statement(Sqlite3Statement&& other) noexcept
      : db_(other.db_), stmt_(other.stmt_) {
    other.db_ = nullptr;
    other.stmt_ = nullptr;
  }

  Sqlite3Statement& operator=(Sqlite3Statement&& other) noexcept {
    if (this != &other) {
      Finalize();
      db_ = other.db_;
      stmt_ = other.stmt_;
      other.db_ = nullptr;
      other.stmt_ = nullptr;
    }
    return *this;
  }

  // No copy
  Sqlite3Statement(const Sqlite3Statement&) = delete;
  Sqlite3Statement& operator=(const Sqlite3Statement&) = delete;

  // --- Execute ---

  /// Execute DML (INSERT/UPDATE/DELETE). Returns affected row count.
  int32_t ExecDml(Error* out_error = nullptr) {
    if (db_ == nullptr || stmt_ == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kMisuse, "Statement not initialized");
      }
      return -1;
    }

    int32_t rc = sqlite3_step(stmt_);
    if (rc == SQLITE_DONE) {
      int32_t changes = sqlite3_changes(db_);
      int32_t reset_rc = sqlite3_reset(stmt_);
      if (reset_rc != SQLITE_OK && out_error != nullptr) {
        out_error->Set(ErrorCode::kError, sqlite3_errmsg(db_));
      }
      return changes;
    }

    sqlite3_reset(stmt_);
    if (out_error != nullptr) {
      out_error->Set(ErrorCode::kError, sqlite3_errmsg(db_));
    }
    return -1;
  }

  /// Execute SELECT query. Returns Sqlite3Query for iteration.
  /// Note: after ExecQuery(), the statement handle is transferred to
  /// the returned Sqlite3Query. This statement becomes empty.
  Sqlite3Query ExecQuery(Error* out_error = nullptr) {
    if (db_ == nullptr || stmt_ == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kMisuse, "Statement not initialized");
      }
      return Sqlite3Query{};
    }

    int32_t rc = sqlite3_step(stmt_);
    if (rc == SQLITE_DONE) {
      // No rows
      Sqlite3Query q(db_, stmt_, true);
      stmt_ = nullptr;  // ownership transferred
      return q;
    }
    if (rc == SQLITE_ROW) {
      Sqlite3Query q(db_, stmt_, false);
      stmt_ = nullptr;  // ownership transferred
      return q;
    }

    sqlite3_reset(stmt_);
    if (out_error != nullptr) {
      out_error->Set(ErrorCode::kError, sqlite3_errmsg(db_));
    }
    return Sqlite3Query{};
  }

  // --- Bind (1-based index) ---

  Error Bind(int32_t param, const char* value) {
    if (stmt_ == nullptr) {
      return Error::Make(ErrorCode::kMisuse, "Statement not initialized");
    }
    int32_t rc = sqlite3_bind_text(stmt_, param, value, -1,
                                    SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
      return Error::Make(ErrorCode::kError, "bind text failed");
    }
    return Error::Ok();
  }

  Error Bind(int32_t param, int32_t value) {
    if (stmt_ == nullptr) {
      return Error::Make(ErrorCode::kMisuse, "Statement not initialized");
    }
    int32_t rc = sqlite3_bind_int(stmt_, param, value);
    if (rc != SQLITE_OK) {
      return Error::Make(ErrorCode::kError, "bind int failed");
    }
    return Error::Ok();
  }

  Error Bind(int32_t param, int64_t value) {
    if (stmt_ == nullptr) {
      return Error::Make(ErrorCode::kMisuse, "Statement not initialized");
    }
    int32_t rc = sqlite3_bind_int64(stmt_, param, value);
    if (rc != SQLITE_OK) {
      return Error::Make(ErrorCode::kError, "bind int64 failed");
    }
    return Error::Ok();
  }

  Error Bind(int32_t param, double value) {
    if (stmt_ == nullptr) {
      return Error::Make(ErrorCode::kMisuse, "Statement not initialized");
    }
    int32_t rc = sqlite3_bind_double(stmt_, param, value);
    if (rc != SQLITE_OK) {
      return Error::Make(ErrorCode::kError, "bind double failed");
    }
    return Error::Ok();
  }

  Error Bind(int32_t param, const uint8_t* blob, int32_t len) {
    if (stmt_ == nullptr) {
      return Error::Make(ErrorCode::kMisuse, "Statement not initialized");
    }
    int32_t rc = sqlite3_bind_blob(stmt_, param, blob, len,
                                    SQLITE_TRANSIENT);
    if (rc != SQLITE_OK) {
      return Error::Make(ErrorCode::kError, "bind blob failed");
    }
    return Error::Ok();
  }

  Error BindNull(int32_t param) {
    if (stmt_ == nullptr) {
      return Error::Make(ErrorCode::kMisuse, "Statement not initialized");
    }
    int32_t rc = sqlite3_bind_null(stmt_, param);
    if (rc != SQLITE_OK) {
      return Error::Make(ErrorCode::kError, "bind null failed");
    }
    return Error::Ok();
  }

  // --- Reset ---

  Error Reset() {
    if (stmt_ == nullptr) {
      return Error::Make(ErrorCode::kMisuse, "Statement not initialized");
    }
    int32_t rc = sqlite3_reset(stmt_);
    if (rc != SQLITE_OK) {
      return Error::Make(ErrorCode::kError,
                         db_ ? sqlite3_errmsg(db_) : "reset failed");
    }
    return Error::Ok();
  }

  void Finalize() {
    if (stmt_ != nullptr) {
      sqlite3_finalize(stmt_);
      stmt_ = nullptr;
    }
  }

  bool Valid() const { return stmt_ != nullptr; }
  sqlite3_stmt* Handle() const { return stmt_; }

 private:
  friend class Sqlite3Db;

  Sqlite3Statement(sqlite3* db, sqlite3_stmt* stmt)
      : db_(db), stmt_(stmt) {}

  sqlite3* db_ = nullptr;
  sqlite3_stmt* stmt_ = nullptr;
};

}  // namespace dbpp
