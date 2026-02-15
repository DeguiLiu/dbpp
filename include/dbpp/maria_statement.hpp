// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp::MariaStatement -- prepared statement for MariaDB/MySQL.
//
// Design:
//   - Wraps MYSQL_STMT* with RAII
//   - Move-only (no copy)
//   - 1-based parameter binding (consistent with Sqlite3Statement)
//   - ExecDml() for INSERT/UPDATE/DELETE, ExecQuery() for SELECT
//   - API-compatible with Sqlite3Statement for Database<Backend> template

#pragma once

#include <cstdint>
#include <cstring>

#include <mysql.h>

#include "dbpp/error.hpp"
#include "dbpp/maria_query.hpp"

namespace dbpp {

class MariaDb;

// ---------------------------------------------------------------------------
// MariaStatement
// ---------------------------------------------------------------------------

class MariaStatement {
 public:
  MariaStatement() = default;

  ~MariaStatement() { Finalize(); }

  // Move
  MariaStatement(MariaStatement&& other) noexcept
      : conn_(other.conn_),
        stmt_(other.stmt_),
        binds_(other.binds_),
        num_params_(other.num_params_) {
    other.conn_ = nullptr;
    other.stmt_ = nullptr;
    other.binds_ = nullptr;
    other.num_params_ = 0;
  }

  MariaStatement& operator=(MariaStatement&& other) noexcept {
    if (this != &other) {
      Finalize();
      conn_ = other.conn_;
      stmt_ = other.stmt_;
      binds_ = other.binds_;
      num_params_ = other.num_params_;
      other.conn_ = nullptr;
      other.stmt_ = nullptr;
      other.binds_ = nullptr;
      other.num_params_ = 0;
    }
    return *this;
  }

  // No copy
  MariaStatement(const MariaStatement&) = delete;
  MariaStatement& operator=(const MariaStatement&) = delete;

  // --- Execute ---

  /// Execute DML. Returns affected row count, or -1 on error.
  int32_t ExecDml(Error* out_error = nullptr) {
    if (stmt_ == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kMisuse, "Statement not initialized");
      }
      return -1;
    }

    if (num_params_ > 0 && binds_ != nullptr) {
      if (mysql_stmt_bind_param(stmt_, binds_) != 0) {
        if (out_error != nullptr) {
          out_error->Set(ErrorCode::kError, mysql_stmt_error(stmt_));
        }
        return -1;
      }
    }

    if (mysql_stmt_execute(stmt_) != 0) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kError, mysql_stmt_error(stmt_));
      }
      return -1;
    }

    int64_t affected = static_cast<int64_t>(mysql_stmt_affected_rows(stmt_));
    return static_cast<int32_t>(affected);
  }

  /// Execute SELECT. Returns MariaQuery for iteration.
  /// Uses mysql_stmt_store_result + transfers to text-protocol result.
  /// Note: for simplicity, this executes the statement and fetches all
  /// results into a MYSQL_RES via the connection's store_result.
  MariaQuery ExecQuery(Error* out_error = nullptr) {
    if (stmt_ == nullptr || conn_ == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kMisuse, "Statement not initialized");
      }
      return MariaQuery{};
    }

    if (num_params_ > 0 && binds_ != nullptr) {
      if (mysql_stmt_bind_param(stmt_, binds_) != 0) {
        if (out_error != nullptr) {
          out_error->Set(ErrorCode::kError, mysql_stmt_error(stmt_));
        }
        return MariaQuery{};
      }
    }

    if (mysql_stmt_execute(stmt_) != 0) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kError, mysql_stmt_error(stmt_));
      }
      return MariaQuery{};
    }

    // Use metadata result set for column info
    MYSQL_RES* meta = mysql_stmt_result_metadata(stmt_);
    if (meta == nullptr) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kError, "No result metadata");
      }
      return MariaQuery{};
    }
    mysql_free_result(meta);

    // Store all results
    if (mysql_stmt_store_result(stmt_) != 0) {
      if (out_error != nullptr) {
        out_error->Set(ErrorCode::kError, mysql_stmt_error(stmt_));
      }
      return MariaQuery{};
    }

    uint64_t row_count = mysql_stmt_num_rows(stmt_);

    // Transfer to text-protocol result by re-executing as plain query
    // This is a design trade-off: we close the stmt result and re-query
    // to get a MYSQL_RES* compatible with MariaQuery.
    // For prepared SELECT, we use stmt_fetch approach instead.
    mysql_stmt_free_result(stmt_);

    // Fallback: re-execute via connection text protocol
    // This loses the prepared statement benefit for SELECT but keeps
    // the API uniform. For production use, a proper MYSQL_BIND result
    // binding would be needed.
    if (out_error != nullptr) {
      out_error->Set(ErrorCode::kError,
                     "Prepared SELECT not yet supported, use Db::ExecQuery");
    }
    return MariaQuery{};
  }

  // --- Bind (1-based index, converted to 0-based for MySQL API) ---

  Error Bind(int32_t param, const char* value) {
    int32_t idx = param - 1;
    if (!ValidParam(idx)) {
      return Error::Make(ErrorCode::kRange, "param out of range");
    }
    std::memset(&binds_[idx], 0, sizeof(MYSQL_BIND));
    binds_[idx].buffer_type = MYSQL_TYPE_STRING;
    binds_[idx].buffer = const_cast<char*>(value);
    binds_[idx].buffer_length =
        (value != nullptr) ? static_cast<unsigned long>(std::strlen(value)) : 0;
    binds_[idx].is_null_value = (value == nullptr) ? 1 : 0;
    return Error::Ok();
  }

  Error Bind(int32_t param, int32_t value) {
    int32_t idx = param - 1;
    if (!ValidParam(idx)) {
      return Error::Make(ErrorCode::kRange, "param out of range");
    }
    std::memset(&binds_[idx], 0, sizeof(MYSQL_BIND));
    // Store value in buffer_length field (reused as storage for small types)
    StoreInt32(idx, value);
    return Error::Ok();
  }

  Error Bind(int32_t param, int64_t value) {
    int32_t idx = param - 1;
    if (!ValidParam(idx)) {
      return Error::Make(ErrorCode::kRange, "param out of range");
    }
    std::memset(&binds_[idx], 0, sizeof(MYSQL_BIND));
    StoreInt64(idx, value);
    return Error::Ok();
  }

  Error Bind(int32_t param, double value) {
    int32_t idx = param - 1;
    if (!ValidParam(idx)) {
      return Error::Make(ErrorCode::kRange, "param out of range");
    }
    std::memset(&binds_[idx], 0, sizeof(MYSQL_BIND));
    StoreDouble(idx, value);
    return Error::Ok();
  }

  Error Bind(int32_t param, const uint8_t* blob, int32_t len) {
    int32_t idx = param - 1;
    if (!ValidParam(idx)) {
      return Error::Make(ErrorCode::kRange, "param out of range");
    }
    std::memset(&binds_[idx], 0, sizeof(MYSQL_BIND));
    binds_[idx].buffer_type = MYSQL_TYPE_BLOB;
    binds_[idx].buffer = const_cast<uint8_t*>(blob);
    binds_[idx].buffer_length = static_cast<unsigned long>(len);
    return Error::Ok();
  }

  Error BindNull(int32_t param) {
    int32_t idx = param - 1;
    if (!ValidParam(idx)) {
      return Error::Make(ErrorCode::kRange, "param out of range");
    }
    std::memset(&binds_[idx], 0, sizeof(MYSQL_BIND));
    binds_[idx].buffer_type = MYSQL_TYPE_NULL;
    return Error::Ok();
  }

  // --- Reset ---

  Error Reset() {
    if (stmt_ == nullptr) {
      return Error::Make(ErrorCode::kMisuse, "Statement not initialized");
    }
    if (mysql_stmt_reset(stmt_) != 0) {
      return Error::Make(ErrorCode::kError, mysql_stmt_error(stmt_));
    }
    // Clear bind buffers
    if (binds_ != nullptr) {
      std::memset(binds_, 0,
                  static_cast<uint32_t>(num_params_) * sizeof(MYSQL_BIND));
    }
    return Error::Ok();
  }

  void Finalize() {
    if (stmt_ != nullptr) {
      mysql_stmt_close(stmt_);
      stmt_ = nullptr;
    }
    delete[] binds_;
    binds_ = nullptr;
    delete[] int_storage_;
    int_storage_ = nullptr;
    delete[] int64_storage_;
    int64_storage_ = nullptr;
    delete[] double_storage_;
    double_storage_ = nullptr;
    num_params_ = 0;
  }

  bool Valid() const { return stmt_ != nullptr; }

 private:
  friend class MariaDb;

  MariaStatement(MYSQL* conn, MYSQL_STMT* stmt)
      : conn_(conn), stmt_(stmt) {
    if (stmt_ != nullptr) {
      num_params_ = static_cast<int32_t>(mysql_stmt_param_count(stmt_));
      if (num_params_ > 0) {
        binds_ = new MYSQL_BIND[static_cast<uint32_t>(num_params_)];
        std::memset(binds_, 0,
                    static_cast<uint32_t>(num_params_) * sizeof(MYSQL_BIND));
        int_storage_ = new int32_t[static_cast<uint32_t>(num_params_)]();
        int64_storage_ = new int64_t[static_cast<uint32_t>(num_params_)]();
        double_storage_ = new double[static_cast<uint32_t>(num_params_)]();
      }
    }
  }

  bool ValidParam(int32_t idx) const {
    return stmt_ != nullptr && binds_ != nullptr &&
           idx >= 0 && idx < num_params_;
  }

  void StoreInt32(int32_t idx, int32_t value) {
    int_storage_[idx] = value;
    binds_[idx].buffer_type = MYSQL_TYPE_LONG;
    binds_[idx].buffer = &int_storage_[idx];
    binds_[idx].is_unsigned = 0;
  }

  void StoreInt64(int32_t idx, int64_t value) {
    int64_storage_[idx] = value;
    binds_[idx].buffer_type = MYSQL_TYPE_LONGLONG;
    binds_[idx].buffer = &int64_storage_[idx];
    binds_[idx].is_unsigned = 0;
  }

  void StoreDouble(int32_t idx, double value) {
    double_storage_[idx] = value;
    binds_[idx].buffer_type = MYSQL_TYPE_DOUBLE;
    binds_[idx].buffer = &double_storage_[idx];
  }

  MYSQL* conn_ = nullptr;
  MYSQL_STMT* stmt_ = nullptr;
  MYSQL_BIND* binds_ = nullptr;
  int32_t* int_storage_ = nullptr;
  int64_t* int64_storage_ = nullptr;
  double* double_storage_ = nullptr;
  int32_t num_params_ = 0;
};

}  // namespace dbpp
