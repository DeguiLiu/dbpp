// Copyright (c) 2024 liudegui. MIT License.
// Tests for dbpp::Sqlite3Statement.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cstring>

#include "dbpp/sqlite3_db.hpp"

using namespace dbpp;

static Sqlite3Db OpenTestDb() {
  Sqlite3Db db;
  db.Open(":memory:");
  db.ExecDml("CREATE TABLE emp(empno INTEGER, empname TEXT);");
  return db;
}

TEST_CASE("Sqlite3Statement: compile and exec", "[sqlite3_statement]") {
  auto db = OpenTestDb();

  auto stmt = db.CompileStatement("INSERT INTO emp VALUES(?, ?);");
  REQUIRE(stmt.Valid());

  stmt.Bind(1, 1);
  stmt.Bind(2, "Alice");
  int32_t ret = stmt.ExecDml();
  REQUIRE(ret == 1);
}

TEST_CASE("Sqlite3Statement: compile error", "[sqlite3_statement]") {
  auto db = OpenTestDb();
  Error err;
  auto stmt = db.CompileStatement("INSERT INTO nonexistent VALUES(?);",
                                   &err);
  REQUIRE_FALSE(err.ok());
  REQUIRE_FALSE(stmt.Valid());
}

TEST_CASE("Sqlite3Statement: bind and reset loop", "[sqlite3_statement]") {
  auto db = OpenTestDb();

  db.BeginTransaction();
  auto stmt = db.CompileStatement("INSERT INTO emp VALUES(?, ?);");

  for (int32_t i = 0; i < 10; ++i) {
    char name[32];
    std::snprintf(name, sizeof(name), "Emp%02d", i);
    stmt.Bind(1, i);
    stmt.Bind(2, name);
    int32_t ret = stmt.ExecDml();
    REQUIRE(ret == 1);
    stmt.Reset();
  }
  stmt.Finalize();
  db.Commit();

  REQUIRE(db.ExecScalar("SELECT count(*) FROM emp;") == 10);
}

TEST_CASE("Sqlite3Statement: bind double", "[sqlite3_statement]") {
  Sqlite3Db db;
  db.Open(":memory:");
  db.ExecDml("CREATE TABLE nums(val REAL);");

  auto stmt = db.CompileStatement("INSERT INTO nums VALUES(?);");
  stmt.Bind(1, 3.14);
  stmt.ExecDml();
  stmt.Finalize();

  auto q = db.ExecQuery("SELECT val FROM nums;");
  REQUIRE(q.GetDouble(0) == Catch::Approx(3.14));
}

TEST_CASE("Sqlite3Statement: bind blob", "[sqlite3_statement]") {
  Sqlite3Db db;
  db.Open(":memory:");
  db.ExecDml("CREATE TABLE blobs(data BLOB);");

  auto stmt = db.CompileStatement("INSERT INTO blobs VALUES(?);");
  uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
  stmt.Bind(1, data, 4);
  stmt.ExecDml();
  stmt.Finalize();

  auto q = db.ExecQuery("SELECT data FROM blobs;");
  int32_t len = 0;
  const uint8_t* blob = q.GetBlob(0, len);
  REQUIRE(len == 4);
  REQUIRE(blob[0] == 0xDE);
  REQUIRE(blob[3] == 0xEF);
}

TEST_CASE("Sqlite3Statement: bind null", "[sqlite3_statement]") {
  auto db = OpenTestDb();

  auto stmt = db.CompileStatement("INSERT INTO emp VALUES(?, ?);");
  stmt.Bind(1, 1);
  stmt.BindNull(2);
  stmt.ExecDml();
  stmt.Finalize();

  auto q = db.ExecQuery("SELECT * FROM emp;");
  REQUIRE_FALSE(q.Eof());
  REQUIRE(q.GetInt(0) == 1);
  REQUIRE(q.FieldIsNull(1));
}

TEST_CASE("Sqlite3Statement: bind int64", "[sqlite3_statement]") {
  Sqlite3Db db;
  db.Open(":memory:");
  db.ExecDml("CREATE TABLE big(val INTEGER);");

  auto stmt = db.CompileStatement("INSERT INTO big VALUES(?);");
  int64_t big_val = 9876543210LL;
  stmt.Bind(1, big_val);
  stmt.ExecDml();
  stmt.Finalize();

  auto q = db.ExecQuery("SELECT val FROM big;");
  REQUIRE(q.GetInt64(0) == 9876543210LL);
}

TEST_CASE("Sqlite3Statement: ExecQuery", "[sqlite3_statement]") {
  auto db = OpenTestDb();
  db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");
  db.ExecDml("INSERT INTO emp VALUES(2, 'Bob');");

  auto stmt = db.CompileStatement("SELECT * FROM emp ORDER BY empno;");
  auto q = stmt.ExecQuery();

  REQUIRE_FALSE(q.Eof());
  REQUIRE(q.GetInt(0) == 1);
  q.NextRow();
  REQUIRE(q.GetInt(0) == 2);
  q.NextRow();
  REQUIRE(q.Eof());

  // stmt should be empty after ExecQuery transfers ownership
  REQUIRE_FALSE(stmt.Valid());
}

TEST_CASE("Sqlite3Statement: move semantics", "[sqlite3_statement]") {
  auto db = OpenTestDb();
  auto stmt1 = db.CompileStatement("INSERT INTO emp VALUES(?, ?);");
  REQUIRE(stmt1.Valid());

  auto stmt2 = std::move(stmt1);
  REQUIRE(stmt2.Valid());
  REQUIRE_FALSE(stmt1.Valid());

  stmt2.Bind(1, 1);
  stmt2.Bind(2, "Test");
  REQUIRE(stmt2.ExecDml() == 1);
}

TEST_CASE("Sqlite3Statement: exec on invalid", "[sqlite3_statement]") {
  Sqlite3Statement stmt;
  Error err;
  int32_t ret = stmt.ExecDml(&err);
  REQUIRE(ret == -1);
  REQUIRE(err.code == ErrorCode::kMisuse);
}

TEST_CASE("Sqlite3Statement: update with bind", "[sqlite3_statement]") {
  auto db = OpenTestDb();
  db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");

  auto stmt = db.CompileStatement(
      "UPDATE emp SET empname = ? WHERE empno = ?;");
  stmt.Bind(1, "Alicia");
  stmt.Bind(2, 1);
  int32_t ret = stmt.ExecDml();
  REQUIRE(ret == 1);
  stmt.Finalize();

  auto q = db.ExecQuery("SELECT empname FROM emp WHERE empno = 1;");
  REQUIRE(std::strcmp(q.GetString(0), "Alicia") == 0);
}
