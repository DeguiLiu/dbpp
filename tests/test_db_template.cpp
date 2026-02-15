// Copyright (c) 2024 liudegui. MIT License.
// Tests for dbpp::Database<Backend> template layer (using SQLite3 backend).

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "dbpp/db.hpp"

using namespace dbpp;

TEST_CASE("Database<Sqlite3Backend>: basic CRUD via template", "[db_template]") {
  Db db;
  auto err = db.Open(":memory:");
  REQUIRE(err.ok());
  REQUIRE(db.IsOpen());

  db.ExecDml("CREATE TABLE emp(empno INTEGER, empname TEXT);");
  REQUIRE(db.TableExists("emp"));

  db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");
  db.ExecDml("INSERT INTO emp VALUES(2, 'Bob');");

  REQUIRE(db.ExecScalar("SELECT count(*) FROM emp;") == 2);

  auto q = db.ExecQuery("SELECT * FROM emp ORDER BY empno;");
  REQUIRE_FALSE(q.Eof());
  REQUIRE(q.GetInt(0) == 1);
  REQUIRE(std::strcmp(q.GetString(1), "Alice") == 0);
  q.NextRow();
  REQUIRE(q.GetInt(0) == 2);
}

TEST_CASE("Database<Sqlite3Backend>: transaction via template", "[db_template]") {
  Db db;
  db.Open(":memory:");
  db.ExecDml("CREATE TABLE t(id INTEGER);");

  db.BeginTransaction();
  REQUIRE(db.InTransaction());
  db.ExecDml("INSERT INTO t VALUES(1);");
  db.ExecDml("INSERT INTO t VALUES(2);");
  db.Commit();
  REQUIRE_FALSE(db.InTransaction());

  REQUIRE(db.ExecScalar("SELECT count(*) FROM t;") == 2);
}

TEST_CASE("Database<Sqlite3Backend>: rollback via template", "[db_template]") {
  Db db;
  db.Open(":memory:");
  db.ExecDml("CREATE TABLE t(id INTEGER);");

  db.BeginTransaction();
  db.ExecDml("INSERT INTO t VALUES(1);");
  db.Rollback();

  REQUIRE(db.ExecScalar("SELECT count(*) FROM t;") == 0);
}

TEST_CASE("Database<Sqlite3Backend>: prepared statement via template", "[db_template]") {
  Db db;
  db.Open(":memory:");
  db.ExecDml("CREATE TABLE emp(empno INTEGER, empname TEXT);");

  auto stmt = db.CompileStatement("INSERT INTO emp VALUES(?, ?);");
  stmt.Bind(1, 1);
  stmt.Bind(2, "Alice");
  REQUIRE(stmt.ExecDml() == 1);
  stmt.Reset();
  stmt.Bind(1, 2);
  stmt.Bind(2, "Bob");
  REQUIRE(stmt.ExecDml() == 1);
  stmt.Finalize();

  REQUIRE(db.ExecScalar("SELECT count(*) FROM emp;") == 2);
}

TEST_CASE("Database<Sqlite3Backend>: result set via template", "[db_template]") {
  Db db;
  db.Open(":memory:");
  db.ExecDml("CREATE TABLE emp(empno INTEGER, empname TEXT);");
  db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");
  db.ExecDml("INSERT INTO emp VALUES(2, 'Bob');");

  auto rs = db.GetResultSet("SELECT * FROM emp ORDER BY empno;");
  REQUIRE(rs.NumRows() == 2);
  REQUIRE(std::strcmp(rs.FieldValue(0), "1") == 0);
  rs.SeekRow(1);
  REQUIRE(std::strcmp(rs.FieldValue(1), "Bob") == 0);
}

TEST_CASE("Database<Sqlite3Backend>: move semantics", "[db_template]") {
  Db db1;
  db1.Open(":memory:");
  REQUIRE(db1.IsOpen());

  Db db2 = std::move(db1);
  REQUIRE(db2.IsOpen());
  REQUIRE_FALSE(db1.IsOpen());
}

TEST_CASE("Database<Sqlite3Backend>: Impl() access", "[db_template]") {
  Db db;
  db.Open(":memory:");

  // Access underlying Sqlite3Db
  auto& impl = db.Impl();
  REQUIRE(impl.Handle() != nullptr);
}

TEST_CASE("Database<Sqlite3Backend>: error handling via template", "[db_template]") {
  Db db;
  db.Open(":memory:");

  Error err;
  db.ExecDml("INSERT INTO nonexistent VALUES(1);", &err);
  REQUIRE_FALSE(err.ok());
}
