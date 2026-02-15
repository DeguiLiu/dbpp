// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp::MariaDb -- MariaDB/MySQL database connection with RAII.
//
// Design:
//   - Wraps MYSQL* with RAII
//   - Move-only (no copy)
//   - Error reporting via Error* output parameter (no exceptions)
//   - Transaction support (Begin/Commit/Rollback)
//   - Zero global state, thread-safe per connection
//   - API-compatible with Sqlite3Db for Database<Backend> template
//
// Open() format: "host:port:user:password:database"
//   e.g. "localhost:3306:root:pass:testdb"
//   or   "127.0.0.1:3306:root::mydb" (empty password)

#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <mysql.h>

#include "dbpp/error.hpp"
#include "dbpp/maria_query.hpp"
#include "dbpp/maria_result_set.hpp"
#include "dbpp/maria_statement.hpp"

namespace dbpp {

// ---------------------------------------------------------------------------
// MariaDb
// ---------------------------------------------------------------------------

class MariaDb {
 public:
  MariaDb() = default;

  ~MariaDb() { Close(); }

  // Move
  MariaDb(MariaDb&& other) noexcept
      : conn_(other.conn_), in_transaction_(other.in_transaction_) {
    other.conn_ = nullptr;
    other.in_transaction_ = false;
  }

  MariaDb& operator=(MariaDb&& other) noexcept {
    if (this != &other) {
      Close();
      conn_ = other.conn_;
      in_transaction_ = other.in_transaction_;
      other.conn_ = nullptr;
      other.in_transaction_ = false;
    }
    return *this;
  }

  // No copy
  MariaDb(const MariaDb&) = delete;
  MariaDb& operator=(const MariaDb&) = delete;

  // --- Open / Close ---

  /// Open connection. Format: "host:port:user:password:database"
  /// Fields can be empty. Minimal: "localhost:3306:root::testdb"
  Error Open(const char* dsn) {
    if (dsn == nullptr) {
      return Error::Make(ErrorCode::kNullParam, "dsn is null");
    }
    Close();

    // Parse DSN: host:port:user:password:database
    char buf[512];
    std::strncpy(buf, dsn, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    const char* host = "localhost";
    uint16_t port = 3306;
    const char* user = "root";
    const char* password = nullptr;
    const char* database = nullptr;

    char* parts[5] = {};
    int32_t count = 0;
    char* p = buf;
    parts[0] = p;
    count = 1;
    while (*p != '\0' && count < 5) {
      if (*p == ':') {
        *p = '\0';
        parts[count++] = p + 1;
      }
      ++p;
    }

    if (count >= 1 && parts[0][0] != '\0') { host = parts[0]; }
    if (count >= 2 && parts[1][0] != '\0') {
      port = static_cast<uint16_t>(std::strtoul(parts[1], nullptr, 10));
    }
    if (count >= 3 && parts[2][0] != '\0') { user = parts[2]; }
    if (count >= 4 && parts[3][0] != '\0') { password = parts[3]; }
    if (count >= 5 && parts[4][0] != '\0') { database = parts[4]; }

    conn_ = mysql_init(nullptr);
    if (conn_ == nullptr) {
      return Error::Make(ErrorCode::kError, "mysql_init failed");
    }

    if (mysql_real_connect(conn_, host, user, password, database,
                           port, nullptr, 0) == nullptr) {
      Error err = Error::Make(ErrorCode::kError, mysql_error(conn_));
      mysql_close(conn_);
      conn_ = nullptr;
      return err;
    }

    // Set UTF-8
    mysql_set_character_set(conn_, "utf8mb4");

    return Error::Ok();
  }

  void Close() {
    if (conn_ != nullptr) {
      mysql_close(conn_);
      conn_ = nullptr;
    }
    in_transaction_ = false;
  }

  bool IsOpen() const { return conn_ != nullptr; }

  // --- DML ---

  int32_t ExecDml(const char* sql, Error* out_error = nullptr) {
    if (conn_ == nullptr) {
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

    if (mysql_query(conn_, sql) != 0) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kError, mysql_error(conn_));
      }
      return -1;
    }

    int64_t affected = static_cast<int64_t>(mysql_affected_rows(conn_));
    return static_cast<int32_t>(affected);
  }

  // --- Scalar query ---

  int32_t ExecScalar(const char* sql, int32_t null_value = 0,
                     Error* out_error = nullptr) {
    MariaQuery q = ExecQuery(sql, out_error);
    if (q.Eof() || q.NumFields() < 1) {
      return null_value;
    }
    return q.GetInt(0, null_value);
  }

  // --- Query ---

  MariaQuery ExecQuery(const char* sql, Error* out_error = nullptr) {
    if (conn_ == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kNotOpen, "Database not open");
      }
      return MariaQuery{};
    }
    if (sql == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kNullParam, "sql is null");
      }
      return MariaQuery{};
    }

    if (mysql_query(conn_, sql) != 0) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kError, mysql_error(conn_));
      }
      return MariaQuery{};
    }

    MYSQL_RES* res = mysql_store_result(conn_);
    if (res == nullptr) {
      // Could be a non-SELECT or an error
      if (mysql_field_count(conn_) > 0) {
        if (out_error != nullptr) {
          out_error->Set(ErrorCode::kError, mysql_error(conn_));
        }
      }
      return MariaQuery{};
    }

    bool eof = (mysql_num_rows(res) == 0);
    return MariaQuery(res, eof);
  }

  // --- ResultSet ---

  MariaResultSet GetResultSet(const char* sql,
                              Error* out_error = nullptr) {
    if (conn_ == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kNotOpen, "Database not open");
      }
      return MariaResultSet{};
    }
    if (sql == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kNullParam, "sql is null");
      }
      return MariaResultSet{};
    }

    if (mysql_query(conn_, sql) != 0) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kError, mysql_error(conn_));
      }
      return MariaResultSet{};
    }

    MYSQL_RES* res = mysql_store_result(conn_);
    if (res == nullptr) {
      if (mysql_field_count(conn_) > 0) {
        if (out_error != nullptr) {
          out_error->Set(ErrorCode::kError, mysql_error(conn_));
        }
      }
      return MariaResultSet{};
    }

    return MariaResultSet(res);
  }

  // --- Statement ---

  MariaStatement CompileStatement(const char* sql,
                                  Error* out_error = nullptr) {
    if (conn_ == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kNotOpen, "Database not open");
      }
      return MariaStatement{};
    }
    if (sql == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kNullParam, "sql is null");
      }
      return MariaStatement{};
    }

    MYSQL_STMT* stmt = mysql_stmt_init(conn_);
    if (stmt == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kError, "mysql_stmt_init failed");
      }
      return MariaStatement{};
    }

    if (mysql_stmt_prepare(stmt, sql,
                           static_cast<unsigned long>(std::strlen(sql))) != 0) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kError, mysql_stmt_error(stmt));
      }
      mysql_stmt_close(stmt);
      return MariaStatement{};
    }

    return MariaStatement(conn_, stmt);
  }

  // --- Table exists ---

  bool TableExists(const char* table) {
    if (conn_ == nullptr || table == nullptr) { return false; }
    char sql[256];
    std::snprintf(sql, sizeof(sql),
        "SELECT COUNT(*) FROM information_schema.tables "
        "WHERE table_schema = DATABASE() AND table_name = '%s'", table);
    return ExecScalar(sql) > 0;
  }

  // --- Transaction ---

  Error BeginTransaction() {
    Error err;
    ExecDml("START TRANSACTION;", &err);
    if (err.ok()) { in_transaction_ = true; }
    return err;
  }

  Error Commit() {
    Error err;
    ExecDml("COMMIT;", &err);
    in_transaction_ = false;
    return err;
  }

  Error Rollback() {
    Error err;
    ExecDml("ROLLBACK;", &err);
    in_transaction_ = false;
    return err;
  }

  bool InTransaction() const { return in_transaction_; }

  // --- Misc ---

  void SetBusyTimeout(int32_t ms) {
    if (conn_ == nullptr) { return; }
    // MySQL uses wait_timeout (seconds), map ms to seconds
    char sql[64];
    std::snprintf(sql, sizeof(sql),
                  "SET wait_timeout = %d", (ms + 999) / 1000);
    mysql_query(conn_, sql);
  }

  MYSQL* Handle() const { return conn_; }

 private:
  MYSQL* conn_ = nullptr;
  bool in_transaction_ = false;
};

}  // namespace dbpp
