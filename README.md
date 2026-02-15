# dbpp (Database++)

[![CI](https://github.com/DeguiLiu/dbpp/actions/workflows/ci.yml/badge.svg)](https://github.com/DeguiLiu/dbpp/actions/workflows/ci.yml)

A lightweight, modern C++14 SQLite3 database abstraction layer with RAII resource management and exception-free error handling.

## Features

- Header-only library with bundled SQLite3 static library
- C++14 standard, move-only semantics, RAII resource management
- Exception-free design, compatible with `-fno-exceptions` builds
- Type-safe API with explicit error handling via `Error` struct
- Prepared statements with 1-based parameter binding
- Forward-only query (`Sqlite3Query`) and random-access result set (`Sqlite3ResultSet`)
- Transaction support (Begin / Commit / Rollback)
- 51 Catch2 test cases, all passing
- GitHub Actions CI on Linux and macOS with ASan and UBSan
- Google C++ Style Guide and MISRA C++ compliant

## Quick Start

```cpp
#include "dbpp/sqlite3_db.hpp"
#include <cstdio>

int main() {
    dbpp::Sqlite3Db db;
    db.Open(":memory:");
    db.ExecDml("CREATE TABLE users(id INTEGER PRIMARY KEY, name TEXT, age INTEGER);");

    // Prepared statement insert
    auto stmt = db.CompileStatement("INSERT INTO users VALUES(?, ?, ?);");
    stmt.Bind(1, 1);
    stmt.Bind(2, "Alice");
    stmt.Bind(3, 30);
    stmt.ExecDml();
    stmt.Reset();
    stmt.Bind(1, 2);
    stmt.Bind(2, "Bob");
    stmt.Bind(3, 25);
    stmt.ExecDml();
    stmt.Finalize();

    // Forward-only query
    auto q = db.ExecQuery("SELECT * FROM users ORDER BY id;");
    while (!q.Eof()) {
        std::printf("id=%d name=%s age=%d\n",
                    q.GetInt(0), q.GetString(1), q.GetInt(2));
        q.NextRow();
    }

    // Scalar query
    int32_t count = db.ExecScalar("SELECT count(*) FROM users;");
    std::printf("total: %d\n", count);

    return 0;
}
```

## Building

Requirements: CMake 3.14+, C++14 compiler (GCC 5+, Clang 5+).

```bash
cmake -B build -DDBPP_BUILD_TESTS=ON -DDBPP_BUILD_EXAMPLES=ON
cmake --build build -j
cd build && ctest --output-on-failure
```

CMake options:

| Option | Default | Description |
|--------|---------|-------------|
| `DBPP_BUILD_TESTS` | ON | Build Catch2 test suite |
| `DBPP_BUILD_EXAMPLES` | ON | Build example programs |

## Project Structure

```
include/dbpp/              -- Public headers
  error.hpp                -- ErrorCode enum + Error struct
  sqlite3_db.hpp           -- Database connection (RAII)
  sqlite3_query.hpp        -- Forward-only query result
  sqlite3_result_set.hpp   -- Random-access result set
  sqlite3_statement.hpp    -- Prepared statement
third_party/sqlite3/       -- Bundled SQLite3 amalgamation
tests/                     -- 51 Catch2 test cases
examples/
  sqlite3_demo.cpp         -- CRUD demo
docs/
  design_zh.md             -- Design document (Chinese)
.github/workflows/
  ci.yml                   -- GitHub Actions CI
```

## Core API

### Sqlite3Db

```cpp
dbpp::Sqlite3Db db;
Error err = db.Open("test.db");       // Open connection
db.Close();                            // Close (also called by destructor)
int32_t rows = db.ExecDml(sql);        // INSERT/UPDATE/DELETE, returns affected rows
int32_t val  = db.ExecScalar(sql);     // SELECT returning single int
Sqlite3Query q = db.ExecQuery(sql);    // SELECT returning forward-only result
Sqlite3ResultSet rs = db.GetResultSet(sql);  // SELECT returning random-access result
Sqlite3Statement st = db.CompileStatement(sql);  // Prepare statement
bool exists = db.TableExists("emp");   // Check table existence
db.BeginTransaction();                 // Transaction control
db.Commit();
db.Rollback();
```

All fallible methods accept an optional `Error* out_error` parameter.

### Sqlite3Query (forward-only)

```cpp
auto q = db.ExecQuery("SELECT id, name, score FROM students;");
while (!q.Eof()) {
    int32_t id       = q.GetInt(0);
    const char* name = q.GetString(1);
    double score     = q.GetDouble(2);
    int32_t missing  = q.GetInt(3, -1);   // null returns default
    bool is_null     = q.FieldIsNull(2);
    q.NextRow();
}
```

### Sqlite3ResultSet (random-access)

```cpp
auto rs = db.GetResultSet("SELECT * FROM emp;");
rs.SeekRow(5);                         // Jump to row 5
const char* val = rs.FieldValue(0);    // Get field as string
uint32_t total = rs.NumRows();
```

### Sqlite3Statement (prepared)

```cpp
auto stmt = db.CompileStatement("INSERT INTO emp VALUES(?, ?);");
stmt.Bind(1, 42);                     // int32_t
stmt.Bind(1, int64_t{9876543210});    // int64_t
stmt.Bind(1, 3.14);                   // double
stmt.Bind(1, "text");                 // const char*
stmt.Bind(1, blob_ptr, blob_len);     // blob
stmt.BindNull(2);                     // NULL
int32_t affected = stmt.ExecDml();    // Execute DML
stmt.Reset();                         // Reset for re-use
auto q = stmt.ExecQuery();            // Execute SELECT (transfers ownership)
```

### Error

```cpp
dbpp::Error err;
db.ExecDml("BAD SQL", &err);
if (!err.ok()) {
    printf("error %d: %s\n", static_cast<int>(err.code), err.message);
}
```

## Design Philosophy

Modernized rewrite of [DatabaseLayer](https://gitee.com/liudegui/DatabaseLayer), replacing raw `new`/`delete` with RAII, `const_cast` hacks with move semantics, global statics with zero shared state, and `sprintf` with `snprintf`.

Key principles: stack-first allocation, move-only resource types, explicit error handling without exceptions, fixed-width integer types, cache-friendly layout.

## License

MIT License. See [LICENSE](LICENSE).

## Links

- GitHub: [DeguiLiu/dbpp](https://github.com/DeguiLiu/dbpp)
- Gitee: [liudegui/dbpp](https://gitee.com/liudegui/dbpp)
- Original: [DatabaseLayer](https://gitee.com/liudegui/DatabaseLayer)
