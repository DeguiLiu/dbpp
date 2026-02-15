// Copyright (c) 2024 liudegui. MIT License.
// Tests for dbpp::Sqlite3ResultSet.

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "dbpp/sqlite3_db.hpp"

using namespace dbpp;

static Sqlite3Db OpenTestDb() {
  Sqlite3Db db;
  db.Open(":memory:");
  db.ExecDml("CREATE TABLE emp(empno INTEGER, empname TEXT);");
  db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");
  db.ExecDml("INSERT INTO emp VALUES(2, 'Bob');");
  db.ExecDml("INSERT INTO emp VALUES(3, 'Charlie');");
  return db;
}

TEST_CASE("Sqlite3ResultSet: basic", "[sqlite3_result_set]") {
  auto db = OpenTestDb();
  auto rs = db.GetResultSet("SELECT * FROM emp ORDER BY empno;");

  REQUIRE(rs.NumFields() == 2);
  REQUIRE(rs.NumRows() == 3);
}

TEST_CASE("Sqlite3ResultSet: field names", "[sqlite3_result_set]") {
  auto db = OpenTestDb();
  auto rs = db.GetResultSet("SELECT * FROM emp;");

  REQUIRE(std::strcmp(rs.FieldName(0), "empno") == 0);
  REQUIRE(std::strcmp(rs.FieldName(1), "empname") == 0);
  REQUIRE(rs.FieldName(-1) == nullptr);
  REQUIRE(rs.FieldName(2) == nullptr);
}

TEST_CASE("Sqlite3ResultSet: field index", "[sqlite3_result_set]") {
  auto db = OpenTestDb();
  auto rs = db.GetResultSet("SELECT * FROM emp;");

  REQUIRE(rs.FieldIndex("empno") == 0);
  REQUIRE(rs.FieldIndex("empname") == 1);
  REQUIRE(rs.FieldIndex("nonexistent") == -1);
}

TEST_CASE("Sqlite3ResultSet: forward iteration", "[sqlite3_result_set]") {
  auto db = OpenTestDb();
  auto rs = db.GetResultSet("SELECT * FROM emp ORDER BY empno;");

  REQUIRE_FALSE(rs.Eof());
  REQUIRE(std::strcmp(rs.FieldValue(0), "1") == 0);
  REQUIRE(std::strcmp(rs.FieldValue(1), "Alice") == 0);

  rs.NextRow();
  REQUIRE_FALSE(rs.Eof());
  REQUIRE(std::strcmp(rs.FieldValue(0), "2") == 0);

  rs.NextRow();
  REQUIRE_FALSE(rs.Eof());
  REQUIRE(std::strcmp(rs.FieldValue(0), "3") == 0);

  rs.NextRow();
  REQUIRE(rs.Eof());
}

TEST_CASE("Sqlite3ResultSet: SeekRow", "[sqlite3_result_set]") {
  auto db = OpenTestDb();
  auto rs = db.GetResultSet("SELECT * FROM emp ORDER BY empno;");

  // Seek to last row
  rs.SeekRow(2);
  REQUIRE(rs.CurrentRow() == 2);
  REQUIRE(std::strcmp(rs.FieldValue(1), "Charlie") == 0);

  // Seek back to first
  rs.SeekRow(0);
  REQUIRE(rs.CurrentRow() == 0);
  REQUIRE(std::strcmp(rs.FieldValue(1), "Alice") == 0);

  // Seek beyond range clamps
  rs.SeekRow(999);
  REQUIRE(rs.CurrentRow() == 2);
}

TEST_CASE("Sqlite3ResultSet: field by name", "[sqlite3_result_set]") {
  auto db = OpenTestDb();
  auto rs = db.GetResultSet("SELECT * FROM emp ORDER BY empno;");

  REQUIRE(std::strcmp(rs.FieldValue("empno"), "1") == 0);
  REQUIRE(std::strcmp(rs.FieldValue("empname"), "Alice") == 0);
}

TEST_CASE("Sqlite3ResultSet: null handling", "[sqlite3_result_set]") {
  Sqlite3Db db;
  db.Open(":memory:");
  db.ExecDml("CREATE TABLE t(a INTEGER, b TEXT);");
  db.ExecDml("INSERT INTO t VALUES(1, NULL);");

  auto rs = db.GetResultSet("SELECT * FROM t;");
  REQUIRE(rs.NumRows() == 1);
  REQUIRE_FALSE(rs.FieldIsNull(0));
  REQUIRE(rs.FieldIsNull(1));
}

TEST_CASE("Sqlite3ResultSet: empty result", "[sqlite3_result_set]") {
  auto db = OpenTestDb();
  auto rs = db.GetResultSet("SELECT * FROM emp WHERE empno = 999;");

  REQUIRE(rs.NumRows() == 0);
  REQUIRE(rs.Eof());
}

TEST_CASE("Sqlite3ResultSet: move semantics", "[sqlite3_result_set]") {
  auto db = OpenTestDb();
  auto rs1 = db.GetResultSet("SELECT * FROM emp ORDER BY empno;");
  REQUIRE(rs1.NumRows() == 3);

  auto rs2 = std::move(rs1);
  REQUIRE(rs2.NumRows() == 3);
  REQUIRE(rs1.NumRows() == 0);

  REQUIRE(std::strcmp(rs2.FieldValue(0), "1") == 0);
}

TEST_CASE("Sqlite3ResultSet: error query", "[sqlite3_result_set]") {
  auto db = OpenTestDb();
  Error err;
  auto rs = db.GetResultSet("SELECT * FROM nonexistent;", &err);
  REQUIRE_FALSE(err.ok());
  REQUIRE(rs.NumRows() == 0);
}

TEST_CASE("Sqlite3ResultSet: Finalize", "[sqlite3_result_set]") {
  auto db = OpenTestDb();
  auto rs = db.GetResultSet("SELECT * FROM emp;");
  REQUIRE(rs.NumRows() == 3);

  rs.Finalize();
  REQUIRE(rs.NumRows() == 0);
  REQUIRE(rs.NumFields() == 0);
}
