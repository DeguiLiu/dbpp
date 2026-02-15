// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp MariaDB demo -- basic CRUD operations via Database<MariaBackend>.
//
// Usage:
//   export DBPP_MARIA_DSN="localhost:3306:root:pass:dbpp_test"
//   ./dbpp_mariadb_demo
//
// Before running, create the database:
//   mysql -u root -e "CREATE DATABASE IF NOT EXISTS dbpp_test;"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "dbpp/db.hpp"

int main() {
  const char* dsn = std::getenv("DBPP_MARIA_DSN");
  if (dsn == nullptr) {
    dsn = "localhost:3306:root::dbpp_test";
  }

  // Use the template facade -- same API as SQLite3
  dbpp::MDb db;
  dbpp::Error err;

  err = db.Open(dsn);
  if (!err.ok()) {
    std::fprintf(stderr, "Open failed: %s\n", err.message);
    return 1;
  }
  std::printf("Connected to MariaDB/MySQL\n");

  // Create table
  db.ExecDml("DROP TABLE IF EXISTS emp;");
  db.ExecDml("CREATE TABLE emp(empno INT, empname VARCHAR(64));", &err);
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

  // Cleanup
  db.ExecDml("DROP TABLE IF EXISTS emp;");
  db.Close();
  std::printf("\nDone.\n");
  return 0;
}
