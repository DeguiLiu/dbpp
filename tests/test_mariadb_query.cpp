// Copyright (c) 2024 liudegui. MIT License.
// Tests for dbpp::MariaQuery (requires running MySQL/MariaDB server).

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cstdlib>
#include <cstring>

#include "dbpp/db.hpp"

using namespace dbpp;

static const char* GetDsn() {
  const char* dsn = std::getenv("DBPP_MARIA_DSN");
  return (dsn != nullptr) ? dsn : "localhost:3306:root::dbpp_test";
}

static MDb OpenTestDb() {
  MDb db;
  db.Open(GetDsn());
  db.ExecDml("DROP TABLE IF EXISTS emp;");
  db.ExecDml("CREATE TABLE emp(empno INT, empname VARCHAR(64));");
  db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");
  db.ExecDml("INSERT INTO emp VALUES(2, 'Bob');");
  db.ExecDml("INSERT INTO emp VALUES(3, NULL);");
  return db;
}

TEST_CASE("MariaQuery: basic iteration", "[mariadb_query]") {
  auto db = OpenTestDb();
  auto q = db.ExecQuery("SELECT * FROM emp ORDER BY empno;");

  REQUIRE_FALSE(q.Eof());
  REQUIRE(q.NumFields() == 2);

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

TEST_CASE("MariaQuery: field by name", "[mariadb_query]") {
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

TEST_CASE("MariaQuery: null handling", "[mariadb_query]") {
  auto db = OpenTestDb();
  auto q = db.ExecQuery("SELECT * FROM emp WHERE empno = 3;");

  REQUIRE_FALSE(q.Eof());
  REQUIRE_FALSE(q.FieldIsNull(0));
  REQUIRE(q.FieldIsNull(1));

  REQUIRE(q.GetInt(1, 99) == 99);
  REQUIRE(std::strcmp(q.GetString(1, "default"), "default") == 0);
}

TEST_CASE("MariaQuery: empty result", "[mariadb_query]") {
  auto db = OpenTestDb();
  auto q = db.ExecQuery("SELECT * FROM emp WHERE empno = 999;");
  REQUIRE(q.Eof());
}

TEST_CASE("MariaQuery: move semantics", "[mariadb_query]") {
  auto db = OpenTestDb();
  auto q1 = db.ExecQuery("SELECT * FROM emp ORDER BY empno;");
  REQUIRE_FALSE(q1.Eof());

  auto q2 = std::move(q1);
  REQUIRE_FALSE(q2.Eof());
  REQUIRE(q1.Eof());

  REQUIRE(q2.GetInt(0) == 1);
}

TEST_CASE("MariaQuery: double field", "[mariadb_query]") {
  auto db = OpenTestDb();
  db.ExecDml("DROP TABLE IF EXISTS nums;");
  db.ExecDml("CREATE TABLE nums(val DOUBLE);");
  db.ExecDml("INSERT INTO nums VALUES(3.14);");

  auto q = db.ExecQuery("SELECT val FROM nums;");
  REQUIRE_FALSE(q.Eof());
  REQUIRE(q.GetDouble(0) == Catch::Approx(3.14));
}

TEST_CASE("MariaQuery: Finalize", "[mariadb_query]") {
  auto db = OpenTestDb();
  auto q = db.ExecQuery("SELECT * FROM emp;");
  REQUIRE_FALSE(q.Eof());

  q.Finalize();
  REQUIRE(q.Eof());
  REQUIRE(q.NumFields() == 0);
}

TEST_CASE("MariaQuery: error query", "[mariadb_query]") {
  auto db = OpenTestDb();
  Error err;
  auto q = db.ExecQuery("SELECT * FROM nonexistent_xyz;", &err);
  REQUIRE_FALSE(err.ok());
  REQUIRE(q.Eof());
}
