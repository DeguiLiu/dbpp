// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp::Database<Backend> -- backend-agnostic database facade.
//
// Design:
//   - Thin template wrapper over backend-specific types
//   - Zero overhead: all methods delegate directly to Backend::Db
//   - Users include this single header and use dbpp::Db (default SQLite3)
//   - To switch backend: using MyDb = dbpp::Database<dbpp::MariaBackend>;
//   - Move-only, RAII, no exceptions
//
// Usage (SQLite3, default):
//   #include "dbpp/db.hpp"
//   dbpp::Db db;
//   db.Open(":memory:");
//   db.ExecDml("CREATE TABLE t(id INTEGER);");
//
// Usage (MariaDB/MySQL, requires DBPP_HAS_MARIADB=1):
//   #include "dbpp/db.hpp"
//   dbpp::MDb db;
//   db.Open("localhost:3306:root:pass:testdb");

#pragma once

#include "dbpp/error.hpp"
#include "dbpp/sqlite3_backend.hpp"

#if defined(DBPP_HAS_MARIADB) && DBPP_HAS_MARIADB
#include "dbpp/maria_backend.hpp"
#endif

namespace dbpp {

// ---------------------------------------------------------------------------
// Database<Backend> -- unified facade
// ---------------------------------------------------------------------------

template <typename Backend = Sqlite3Backend>
class Database {
 public:
  using DbType        = typename Backend::Db;
  using QueryType     = typename Backend::Query;
  using ResultSetType = typename Backend::ResultSet;
  using StatementType = typename Backend::Statement;

  Database() = default;
  ~Database() = default;

  // Move
  Database(Database&& other) noexcept
      : impl_(std::move(other.impl_)) {}

  Database& operator=(Database&& other) noexcept {
    if (this != &other) {
      impl_ = std::move(other.impl_);
    }
    return *this;
  }

  // No copy
  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;

  // --- Open / Close ---

  Error Open(const char* path) { return impl_.Open(path); }
  void Close() { impl_.Close(); }
  bool IsOpen() const { return impl_.IsOpen(); }

  // --- DML ---

  int32_t ExecDml(const char* sql, Error* out_error = nullptr) {
    return impl_.ExecDml(sql, out_error);
  }

  // --- Scalar ---

  int32_t ExecScalar(const char* sql, int32_t null_value = 0,
                     Error* out_error = nullptr) {
    return impl_.ExecScalar(sql, null_value, out_error);
  }

  // --- Query ---

  QueryType ExecQuery(const char* sql, Error* out_error = nullptr) {
    return impl_.ExecQuery(sql, out_error);
  }

  // --- ResultSet ---

  ResultSetType GetResultSet(const char* sql,
                             Error* out_error = nullptr) {
    return impl_.GetResultSet(sql, out_error);
  }

  // --- Statement ---

  StatementType CompileStatement(const char* sql,
                                 Error* out_error = nullptr) {
    return impl_.CompileStatement(sql, out_error);
  }

  // --- Table exists ---

  bool TableExists(const char* table) {
    return impl_.TableExists(table);
  }

  // --- Transaction ---

  Error BeginTransaction() { return impl_.BeginTransaction(); }
  Error Commit() { return impl_.Commit(); }
  Error Rollback() { return impl_.Rollback(); }
  bool InTransaction() const { return impl_.InTransaction(); }

  // --- Misc ---

  void SetBusyTimeout(int32_t ms) { impl_.SetBusyTimeout(ms); }

  /// Access the underlying backend implementation.
  DbType& Impl() { return impl_; }
  const DbType& Impl() const { return impl_; }

 private:
  DbType impl_;
};

// ---------------------------------------------------------------------------
// Default type aliases -- users just use dbpp::Db
// ---------------------------------------------------------------------------

using Db        = Database<Sqlite3Backend>;
using Query     = Db::QueryType;
using ResultSet = Db::ResultSetType;
using Statement = Db::StatementType;

#if defined(DBPP_HAS_MARIADB) && DBPP_HAS_MARIADB
using MDb        = Database<MariaBackend>;
using MQuery     = MDb::QueryType;
using MResultSet = MDb::ResultSetType;
using MStatement = MDb::StatementType;
#endif

}  // namespace dbpp
