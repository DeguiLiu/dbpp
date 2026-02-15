// Copyright (c) 2024 liudegui. MIT License.
// Tests for dbpp::Sqlite3Db.

#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <cstring>

#include "dbpp/sqlite3_db.hpp"

using namespace dbpp;

static const char* kTestDb = ":memory:";

// Helper: open an in-memory db with emp table
static Sqlite3Db OpenTestDb() {
  Sqlite3Db db;
  auto err = db.Open(kTestDb);
  REQUIRE(err.ok());
  db.ExecDml("CREATE TABLE emp(empno INTEGER, empname TEXT);");
  return db;
}

TEST_CASE("Sqlite3Db: open and close", "[sqlite3_db]") {
  Sqlite3Db db;
  REQUIRE_FALSE(db.IsOpen());

  auto err = db.Open(kTestDb);
  REQUIRE(err.ok());
  REQUIRE(db.IsOpen());

  db.Close();
  REQUIRE_FALSE(db.IsOpen());
}

TEST_CASE("Sqlite3Db: open null path", "[sqlite3_db]") {
  Sqlite3Db db;
  auto err = db.Open(nullptr);
  REQUIRE_FALSE(err.ok());
  REQUIRE(err.code == ErrorCode::kNullParam);
}

TEST_CASE("Sqlite3Db: move semantics", "[sqlite3_db]") {
  Sqlite3Db db1;
  db1.Open(kTestDb);
  REQUIRE(db1.IsOpen());

  Sqlite3Db db2(std::move(db1));
  REQUIRE(db2.IsOpen());
  REQUIRE_FALSE(db1.IsOpen());

  Sqlite3Db db3;
  db3 = std::move(db2);
  REQUIRE(db3.IsOpen());
  REQUIRE_FALSE(db2.IsOpen());
}

TEST_CASE("Sqlite3Db: ExecDml create table", "[sqlite3_db]") {
  Sqlite3Db db;
  db.Open(kTestDb);

  Error err;
  int32_t ret = db.ExecDml(
      "CREATE TABLE test(id INTEGER, name TEXT);", &err);
  REQUIRE(err.ok());
  REQUIRE(ret == 0);
}

TEST_CASE("Sqlite3Db: ExecDml insert", "[sqlite3_db]") {
  auto db = OpenTestDb();

  Error err;
  int32_t ret = db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');", &err);
  REQUIRE(err.ok());
  REQUIRE(ret == 1);
}

TEST_CASE("Sqlite3Db: ExecDml error", "[sqlite3_db]") {
  auto db = OpenTestDb();

  Error err;
  db.ExecDml("INSERT INTO nonexistent VALUES(1);", &err);
  REQUIRE_FALSE(err.ok());
}

TEST_CASE("Sqlite3Db: ExecDml on closed db", "[sqlite3_db]") {
  Sqlite3Db db;
  Error err;
  int32_t ret = db.ExecDml("SELECT 1;", &err);
  REQUIRE(ret == -1);
  REQUIRE(err.code == ErrorCode::kNotOpen);
}

TEST_CASE("Sqlite3Db: ExecScalar", "[sqlite3_db]") {
  auto db = OpenTestDb();
  db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");
  db.ExecDml("INSERT INTO emp VALUES(2, 'Bob');");

  int32_t count = db.ExecScalar("SELECT count(*) FROM emp;");
  REQUIRE(count == 2);

  int32_t max_val = db.ExecScalar("SELECT max(empno) FROM emp;");
  REQUIRE(max_val == 2);
}

TEST_CASE("Sqlite3Db: TableExists", "[sqlite3_db]") {
  auto db = OpenTestDb();
  REQUIRE(db.TableExists("emp"));
  REQUIRE_FALSE(db.TableExists("nonexistent"));
}

TEST_CASE("Sqlite3Db: transaction commit", "[sqlite3_db]") {
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

TEST_CASE("Sqlite3Db: transaction rollback", "[sqlite3_db]") {
  auto db = OpenTestDb();

  db.BeginTransaction();
  db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");
  db.ExecDml("INSERT INTO emp VALUES(2, 'Bob');");

  auto err = db.Rollback();
  REQUIRE(err.ok());
  REQUIRE_FALSE(db.InTransaction());

  REQUIRE(db.ExecScalar("SELECT count(*) FROM emp;") == 0);
}

TEST_CASE("Sqlite3Db: busy timeout", "[sqlite3_db]") {
  Sqlite3Db db;
  db.Open(kTestDb);
  db.SetBusyTimeout(1000);  // Should not crash
  REQUIRE(db.IsOpen());
}
