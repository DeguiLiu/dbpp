// Copyright (c) 2024 liudegui. MIT License.
// Tests for dbpp::MariaDb (requires running MySQL/MariaDB server).
//
// Environment variables:
//   DBPP_MARIA_DSN  -- DSN string, default "localhost:3306:root::dbpp_test"

#include <catch2/catch_test_macros.hpp>
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
  auto err = db.Open(GetDsn());
  REQUIRE(err.ok());
  db.ExecDml("DROP TABLE IF EXISTS emp;");
  db.ExecDml("CREATE TABLE emp(empno INT, empname VARCHAR(64));");
  return db;
}

TEST_CASE("MariaDb: open and close", "[mariadb]") {
  MDb db;
  REQUIRE_FALSE(db.IsOpen());

  auto err = db.Open(GetDsn());
  REQUIRE(err.ok());
  REQUIRE(db.IsOpen());

  db.Close();
  REQUIRE_FALSE(db.IsOpen());
}

TEST_CASE("MariaDb: ExecDml insert", "[mariadb]") {
  auto db = OpenTestDb();

  Error err;
  int32_t ret = db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');", &err);
  REQUIRE(err.ok());
  REQUIRE(ret == 1);
}

TEST_CASE("MariaDb: ExecDml error", "[mariadb]") {
  auto db = OpenTestDb();

  Error err;
  db.ExecDml("INSERT INTO nonexistent VALUES(1);", &err);
  REQUIRE_FALSE(err.ok());
}

TEST_CASE("MariaDb: ExecScalar", "[mariadb]") {
  auto db = OpenTestDb();
  db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");
  db.ExecDml("INSERT INTO emp VALUES(2, 'Bob');");

  int32_t count = db.ExecScalar("SELECT count(*) FROM emp;");
  REQUIRE(count == 2);
}

TEST_CASE("MariaDb: TableExists", "[mariadb]") {
  auto db = OpenTestDb();
  REQUIRE(db.TableExists("emp"));
  REQUIRE_FALSE(db.TableExists("nonexistent_table_xyz"));
}

TEST_CASE("MariaDb: transaction commit", "[mariadb]") {
  auto db = OpenTestDb();

  auto err = db.BeginTransaction();
  REQUIRE(err.ok());
  REQUIRE(db.InTransaction());

  db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");
  db.ExecDml("INSERT INTO emp VALUES(2, 'Bob');");

  err = db.Commit();
  REQUIRE(err.ok());
  REQUIRE_FALSE(db.InTransaction());

  REQUIRE(db.ExecScalar("SELECT count(*) FROM emp;") == 2);
}

TEST_CASE("MariaDb: transaction rollback", "[mariadb]") {
  auto db = OpenTestDb();

  db.BeginTransaction();
  db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");

  auto err = db.Rollback();
  REQUIRE(err.ok());
  REQUIRE_FALSE(db.InTransaction());

  REQUIRE(db.ExecScalar("SELECT count(*) FROM emp;") == 0);
}

TEST_CASE("MariaDb: move semantics", "[mariadb]") {
  MDb db1;
  db1.Open(GetDsn());
  REQUIRE(db1.IsOpen());

  MDb db2(std::move(db1));
  REQUIRE(db2.IsOpen());
  REQUIRE_FALSE(db1.IsOpen());
}
