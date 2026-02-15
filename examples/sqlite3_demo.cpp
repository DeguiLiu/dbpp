// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp SQLite3 demo -- basic CRUD operations.
//
// Usage:
//   ./dbpp_sqlite3_demo

#include <cstdio>
#include <cstring>

#include "dbpp/sqlite3_db.hpp"

int main() {
  dbpp::Sqlite3Db db;
  dbpp::Error err;

  // Open in-memory database
  err = db.Open(":memory:");
  if (!err.ok()) {
    std::fprintf(stderr, "Open failed: %s\n", err.message);
    return 1;
  }

  // Create table
  db.ExecDml("CREATE TABLE emp(empno INTEGER, empname TEXT);", &err);
  std::printf("emp table exists: %s\n",
              db.TableExists("emp") ? "true" : "false");

  // Insert
  db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");
  db.ExecDml("INSERT INTO emp VALUES(2, 'Bob');");
  db.ExecDml("INSERT INTO emp VALUES(3, 'Charlie');");
  std::printf("Inserted 3 rows\n");

  // Scalar query
  int32_t count = db.ExecScalar("SELECT count(*) FROM emp;");
  std::printf("Row count: %d\n", count);

  // Query iteration
  std::printf("\n--- Query ---\n");
  auto q = db.ExecQuery("SELECT * FROM emp ORDER BY empno;");
  while (!q.Eof()) {
    std::printf("  empno=%d  empname=%s\n",
                q.GetInt(0), q.GetString(1));
    q.NextRow();
  }
  q.Finalize();

  // ResultSet (random access)
  std::printf("\n--- ResultSet (reverse) ---\n");
  auto rs = db.GetResultSet("SELECT * FROM emp ORDER BY empno;");
  for (int32_t i = static_cast<int32_t>(rs.NumRows()) - 1; i >= 0; --i) {
    rs.SeekRow(static_cast<uint32_t>(i));
    std::printf("  %s | %s\n", rs.FieldValue(0), rs.FieldValue(1));
  }
  rs.Finalize();

  // Prepared statement with transaction
  std::printf("\n--- Batch insert with statement ---\n");
  db.ExecDml("DELETE FROM emp;");
  db.BeginTransaction();

  auto stmt = db.CompileStatement("INSERT INTO emp VALUES(?, ?);");
  for (int32_t i = 0; i < 10; ++i) {
    char name[32];
    std::snprintf(name, sizeof(name), "Employee%02d", i);
    stmt.Bind(1, i);
    stmt.Bind(2, name);
    stmt.ExecDml();
    stmt.Reset();
  }
  stmt.Finalize();
  db.Commit();

  count = db.ExecScalar("SELECT count(*) FROM emp;");
  std::printf("After batch insert: %d rows\n", count);

  // Update
  int32_t updated = db.ExecDml(
      "UPDATE emp SET empname = 'Boss' WHERE empno = 0;");
  std::printf("Updated %d row(s)\n", updated);

  // Delete
  int32_t deleted = db.ExecDml("DELETE FROM emp WHERE empno > 5;");
  std::printf("Deleted %d row(s)\n", deleted);

  count = db.ExecScalar("SELECT count(*) FROM emp;");
  std::printf("Final row count: %d\n", count);

  db.Close();
  std::printf("\nDone.\n");
  return 0;
}
