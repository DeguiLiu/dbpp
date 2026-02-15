// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp::MariaBackend -- backend traits for MariaDB/MySQL.
//
// Design:
//   - Aggregates all MariaDB-specific types into a single traits struct
//   - Used as template parameter for Database<Backend>
//   - Zero overhead: just type aliases, no virtual dispatch

#pragma once

#include "dbpp/maria_db.hpp"

namespace dbpp {

// ---------------------------------------------------------------------------
// MariaBackend -- type traits for Database<Backend> template
// ---------------------------------------------------------------------------

struct MariaBackend {
  using Db        = MariaDb;
  using Query     = MariaQuery;
  using ResultSet = MariaResultSet;
  using Statement = MariaStatement;
};

}  // namespace dbpp
