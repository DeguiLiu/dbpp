// Copyright (c) 2024 liudegui. MIT License.
// Tests for dbpp::Sqlite3Query.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cstring>

#include "dbpp/sqlite3_db.hpp"

using namespace dbpp;

static Sqlite3Db OpenTestDb() {
  Sqlite3Db db;
  db.Open(":memory:");
  db.ExecDml("CREATE TABLE emp(empno INTEGER, empname TEXT);");
  db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");
  db.ExecDml("INSERT INTO emp VALUES(2, 'Bob');");
  db.ExecDml("INSERT INTO emp VALUES(3, NULL);");
  return db;
}

TEST_CASE("Sqlite3Query: basic iteration", "[sqlite3_query]") {
  auto db = OpenTestDb();
  auto q = db.ExecQuery("SELECT * FROM emp ORDER BY empno;");

  REQUIRE_FALSE(q.Eof());
  REQUIRE(q.NumFields() == 2);

  // Row 1
  REQUIRE(q.GetInt(0) == 1);
  REQUIRE(std::strcmp(q.GetString(1), "Alice") == 0);

  q.NextRow();
  REQUIRE_FALSE(q.Eof());
  REQUIRE(q.GetInt(0) == 2);

  q.NextRow();
  REQUIRE_FALSE(q.Eof());
  REQUIRE(q.GetInt(0) == 3);

  q.NextRow();
  REQUIRE(q.Eof());
}

TEST_CASE("Sqlite3Query: field by name", "[sqlite3_query]") {
  auto db = OpenTestDb();
  auto q = db.ExecQuery("SELECT * FROM emp ORDER BY empno;");

  REQUIRE(q.FieldIndex("empno") == 0);
  REQUIRE(q.FieldIndex("empname") == 1);
  REQUIRE(q.FieldIndex("nonexistent") == -1);

  REQUIRE(std::strcmp(q.FieldName(0), "empno") == 0);
  REQUIRE(std::strcmp(q.FieldName(1), "empname") == 0);

  REQUIRE(q.GetInt("empno") == 1);
  REQUIRE(std::strcmp(q.GetString("empname"), "Alice") == 0);
}

TEST_CASE("Sqlite3Query: null handling", "[sqlite3_query]") {
  auto db = OpenTestDb();
  auto q = db.ExecQuery("SELECT * FROM emp WHERE empno = 3;");

  REQUIRE_FALSE(q.Eof());
  REQUIRE_FALSE(q.FieldIsNull(0));
  REQUIRE(q.FieldIsNull(1));

  REQUIRE(q.GetInt(1, 99) == 99);
  REQUIRE(std::strcmp(q.GetString(1, "default"), "default") == 0);
  REQUIRE(q.GetDouble(1, 3.14) == 3.14);
}

TEST_CASE("Sqlite3Query: empty result", "[sqlite3_query]") {
  auto db = OpenTestDb();
  auto q = db.ExecQuery("SELECT * FROM emp WHERE empno = 999;");
  REQUIRE(q.Eof());
}

TEST_CASE("Sqlite3Query: move semantics", "[sqlite3_query]") {
  auto db = OpenTestDb();
  auto q1 = db.ExecQuery("SELECT * FROM emp ORDER BY empno;");
  REQUIRE_FALSE(q1.Eof());

  auto q2 = std::move(q1);
  REQUIRE_FALSE(q2.Eof());
  REQUIRE(q1.Eof());  // moved-from

  REQUIRE(q2.GetInt(0) == 1);
}

TEST_CASE("Sqlite3Query: double field", "[sqlite3_query]") {
  Sqlite3Db db;
  db.Open(":memory:");
  db.ExecDml("CREATE TABLE nums(val REAL);");
  db.ExecDml("INSERT INTO nums VALUES(3.14);");

  auto q = db.ExecQuery("SELECT val FROM nums;");
  REQUIRE_FALSE(q.Eof());
  REQUIRE(q.GetDouble(0) == Catch::Approx(3.14));
}

TEST_CASE("Sqlite3Query: blob field", "[sqlite3_query]") {
  Sqlite3Db db;
  db.Open(":memory:");
  db.ExecDml("CREATE TABLE blobs(data BLOB);");

  auto stmt = db.CompileStatement("INSERT INTO blobs VALUES(?);");
  uint8_t blob_data[] = {0x01, 0x02, 0x03, 0x04};
  stmt.Bind(1, blob_data, 4);
  stmt.ExecDml();
  stmt.Finalize();

  auto q = db.ExecQuery("SELECT data FROM blobs;");
  REQUIRE_FALSE(q.Eof());

  int32_t len = 0;
  const uint8_t* blob = q.GetBlob(0, len);
  REQUIRE(len == 4);
  REQUIRE(blob != nullptr);
  REQUIRE(blob[0] == 0x01);
  REQUIRE(blob[3] == 0x04);
}

TEST_CASE("Sqlite3Query: Finalize", "[sqlite3_query]") {
  auto db = OpenTestDb();
  auto q = db.ExecQuery("SELECT * FROM emp;");
  REQUIRE_FALSE(q.Eof());

  q.Finalize();
  REQUIRE(q.Eof());
  REQUIRE(q.NumFields() == 0);
}

TEST_CASE("Sqlite3Query: error query", "[sqlite3_query]") {
  auto db = OpenTestDb();
  Error err;
  auto q = db.ExecQuery("SELECT * FROM nonexistent;", &err);
  REQUIRE_FALSE(err.ok());
  REQUIRE(q.Eof());
}
