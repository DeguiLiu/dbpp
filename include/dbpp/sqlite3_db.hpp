// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp::Sqlite3Db -- SQLite3 database connection with RAII.
//
// Design:
//   - Wraps sqlite3* with RAII
//   - Move-only (no copy)
//   - Error reporting via Error* output parameter (no exceptions)
//   - Transaction support (Begin/Commit/Rollback)
//   - Zero global state, thread-safe per connection

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "sqlite3.h"

#include "dbpp/error.hpp"
#include "dbpp/sqlite3_query.hpp"
#include "dbpp/sqlite3_result_set.hpp"
#include "dbpp/sqlite3_statement.hpp"

namespace dbpp {

// ---------------------------------------------------------------------------
// Sqlite3Db
// ---------------------------------------------------------------------------

class Sqlite3Db {
 public:
  Sqlite3Db() = default;

  ~Sqlite3Db() { Close(); }

  // Move
  Sqlite3Db(Sqlite3Db&& other) noexcept : db_(other.db_) {
    other.db_ = nullptr;
  }

  Sqlite3Db& operator=(Sqlite3Db&& other) noexcept {
    if (this != &other) {
      Close();
      db_ = other.db_;
      other.db_ = nullptr;
    }
    return *this;
  }

  // No copy
  Sqlite3Db(const Sqlite3Db&) = delete;
  Sqlite3Db& operator=(const Sqlite3Db&) = delete;

  // --- Open / Close ---

  Error Open(const char* path) {
    if (path == nullptr) {
      return Error::Make(ErrorCode::kNullParam, "path is null");
    }
    Close();
    int32_t rc = sqlite3_open(path, &db_);
    if (rc != SQLITE_OK) {
      Error err = Error::Make(ErrorCode::kError,
                              db_ ? sqlite3_errmsg(db_) : "sqlite3_open failed");
      if (db_ != nullptr) {
        sqlite3_close(db_);
        db_ = nullptr;
      }
      return err;
    }
    return Error::Ok();
  }

  void Close() {
    if (db_ != nullptr) {
      sqlite3_close(db_);
      db_ = nullptr;
    }
  }

  bool IsOpen() const { return db_ != nullptr; }

  // --- DML ---

  /// Execute DML (CREATE/DROP/INSERT/UPDATE/DELETE).
  /// Returns number of affected rows, or -1 on error.
  int32_t ExecDml(const char* sql, Error* out_error = nullptr) {
    if (db_ == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kNotOpen, "Database not open");
      }
      return -1;
    }
    if (sql == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kNullParam, "sql is null");
      }
      return -1;
    }

    char* errmsg = nullptr;
    int32_t rc = sqlite3_exec(db_, sql, nullptr, nullptr, &errmsg);
    if (rc == SQLITE_OK) {
      return sqlite3_changes(db_);
    }

    if (out_error != nullptr) {
      out_error->Set(ErrorCode::kError,
                     errmsg ? errmsg : sqlite3_errmsg(db_));
    }
    if (errmsg != nullptr) { sqlite3_free(errmsg); }
    return -1;
  }

  // --- Scalar query ---

  /// Execute scalar query (e.g. SELECT count(*)).
  /// Returns the first column of the first row as int32_t.
  int32_t ExecScalar(const char* sql, int32_t null_value = 0,
                     Error* out_error = nullptr) {
    Sqlite3Query q = ExecQuery(sql, out_error);
    if (q.Eof() || q.NumFields() < 1) {
      if (out_error != nullptr && out_error->ok()) {
        out_error->Set(ErrorCode::kError, "Invalid scalar query");
      }
      return null_value;
    }
    return q.GetInt(0, null_value);
  }

  // --- Query ---

  /// Execute SELECT query. Returns Sqlite3Query for forward iteration.
  Sqlite3Query ExecQuery(const char* sql, Error* out_error = nullptr) {
    if (db_ == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kNotOpen, "Database not open");
      }
      return Sqlite3Query{};
    }
    if (sql == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kNullParam, "sql is null");
      }
      return Sqlite3Query{};
    }

    sqlite3_stmt* stmt = Compile(sql, out_error);
    if (stmt == nullptr) { return Sqlite3Query{}; }

    int32_t rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
      return Sqlite3Query(db_, stmt, true);
    }
    if (rc == SQLITE_ROW) {
      return Sqlite3Query(db_, stmt, false);
    }

    if (out_error != nullptr) {
      out_error->Set(ErrorCode::kError, sqlite3_errmsg(db_));
    }
    sqlite3_finalize(stmt);
    return Sqlite3Query{};
  }

  // --- ResultSet ---

  /// Execute query and load all results into memory (random access).
  Sqlite3ResultSet GetResultSet(const char* sql,
                                Error* out_error = nullptr) {
    if (db_ == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kNotOpen, "Database not open");
      }
      return Sqlite3ResultSet{};
    }
    if (sql == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kNullParam, "sql is null");
      }
      return Sqlite3ResultSet{};
    }

    char* errmsg = nullptr;
    char** results = nullptr;
    int32_t rows = 0;
    int32_t cols = 0;

    int32_t rc = sqlite3_get_table(db_, sql, &results, &rows, &cols,
                                    &errmsg);
    if (rc == SQLITE_OK) {
      return Sqlite3ResultSet(results, static_cast<uint32_t>(rows), cols);
    }

    if (out_error != nullptr) {
      out_error->Set(ErrorCode::kError,
                     errmsg ? errmsg : sqlite3_errmsg(db_));
    }
    if (errmsg != nullptr) { sqlite3_free(errmsg); }
    return Sqlite3ResultSet{};
  }

  // --- Statement ---

  /// Compile a prepared statement.
  Sqlite3Statement CompileStatement(const char* sql,
                                     Error* out_error = nullptr) {
    if (db_ == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kNotOpen, "Database not open");
      }
      return Sqlite3Statement{};
    }

    sqlite3_stmt* stmt = Compile(sql, out_error);
    if (stmt == nullptr) { return Sqlite3Statement{}; }
    return Sqlite3Statement(db_, stmt);
  }

  // --- Table exists ---

  bool TableExists(const char* table) {
    if (db_ == nullptr || table == nullptr) { return false; }
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT count(*) FROM sqlite_master "
        "WHERE type='table' AND name='%s'", table);
    return ExecScalar(sql) > 0;
  }

  // --- Transaction ---

  Error BeginTransaction() {
    Error err;
    ExecDml("BEGIN TRANSACTION;", &err);
    return err;
  }

  Error Commit() {
    Error err;
    ExecDml("COMMIT TRANSACTION;", &err);
    return err;
  }

  Error Rollback() {
    Error err;
    ExecDml("ROLLBACK;", &err);
    return err;
  }

  bool InTransaction() const {
    if (db_ == nullptr) { return false; }
    return sqlite3_get_autocommit(db_) == 0;
  }

  // --- Misc ---

  void SetBusyTimeout(int32_t ms) {
    if (db_ != nullptr) { sqlite3_busy_timeout(db_, ms); }
  }

  sqlite3* Handle() const { return db_; }

 private:
  sqlite3_stmt* Compile(const char* sql, Error* out_error) {
    const char* tail = nullptr;
    sqlite3_stmt* stmt = nullptr;
    int32_t rc = sqlite3_prepare_v2(db_, sql, -1, &stmt, &tail);
    if (rc != SQLITE_OK) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kError, sqlite3_errmsg(db_));
      }
      return nullptr;
    }
    return stmt;
  }

  sqlite3* db_ = nullptr;
};

}  // namespace dbpp
