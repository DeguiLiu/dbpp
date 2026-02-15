// Copyright (c) 2024 liudegui. MIT License.
// Tests for dbpp::MariaStatement (requires running MySQL/MariaDB server).

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cstdlib>
#include <cstdio>
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
  return db;
}

TEST_CASE("MariaStatement: compile and exec", "[mariadb_statement]") {
  auto db = OpenTestDb();

  auto stmt = db.CompileStatement("INSERT INTO emp VALUES(?, ?);");
  REQUIRE(stmt.Valid());

  stmt.Bind(1, 1);
  stmt.Bind(2, "Alice");
  int32_t ret = stmt.ExecDml();
  REQUIRE(ret == 1);
}

TEST_CASE("MariaStatement: compile error", "[mariadb_statement]") {
  auto db = OpenTestDb();
  Error err;
  auto stmt = db.CompileStatement("INSERT INTO nonexistent VALUES(?);", &err);
  REQUIRE_FALSE(err.ok());
  REQUIRE_FALSE(stmt.Valid());
}

TEST_CASE("MariaStatement: bind and reset loop", "[mariadb_statement]") {
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

TEST_CASE("MariaStatement: bind double", "[mariadb_statement]") {
  auto db = OpenTestDb();
  db.ExecDml("DROP TABLE IF EXISTS nums;");
  db.ExecDml("CREATE TABLE nums(val DOUBLE);");

  auto stmt = db.CompileStatement("INSERT INTO nums VALUES(?);");
  stmt.Bind(1, 3.14);
  stmt.ExecDml();
  stmt.Finalize();

  auto q = db.ExecQuery("SELECT val FROM nums;");
  REQUIRE(q.GetDouble(0) == Catch::Approx(3.14));
}

TEST_CASE("MariaStatement: bind null", "[mariadb_statement]") {
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

TEST_CASE("MariaStatement: bind int64", "[mariadb_statement]") {
  auto db = OpenTestDb();
  db.ExecDml("DROP TABLE IF EXISTS big;");
  db.ExecDml("CREATE TABLE big(val BIGINT);");

  auto stmt = db.CompileStatement("INSERT INTO big VALUES(?);");
  int64_t big_val = 9876543210LL;
  stmt.Bind(1, big_val);
  stmt.ExecDml();
  stmt.Finalize();

  auto q = db.ExecQuery("SELECT val FROM big;");
  REQUIRE(q.GetInt64(0) == 9876543210LL);
}

TEST_CASE("MariaStatement: move semantics", "[mariadb_statement]") {
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

TEST_CASE("MariaStatement: exec on invalid", "[mariadb_statement]") {
  MariaStatement stmt;
  Error err;
  int32_t ret = stmt.ExecDml(&err);
  REQUIRE(ret == -1);
  REQUIRE(err.code == ErrorCode::kMisuse);
}

TEST_CASE("MariaStatement: update with bind", "[mariadb_statement]") {
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
