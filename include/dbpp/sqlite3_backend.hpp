// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp::Sqlite3Backend -- backend traits for SQLite3.
//
// Design:
//   - Aggregates all SQLite3-specific types into a single traits struct
//   - Used as template parameter for Database<Backend>
//   - Zero overhead: just type aliases, no virtual dispatch

#pragma once

#include "dbpp/sqlite3_db.hpp"

namespace dbpp {

// ---------------------------------------------------------------------------
// Sqlite3Backend -- type traits for Database<Backend> template
// ---------------------------------------------------------------------------

struct Sqlite3Backend {
  using Db        = Sqlite3Db;
  using Query     = Sqlite3Query;
  using ResultSet = Sqlite3ResultSet;
  using Statement = Sqlite3Statement;
};

}  // namespace dbpp
