# dbpp: C++14 轻量数据库抽象层 -- 设计文档

## 1. 概述

dbpp (Database++) 是对 [DatabaseLayer](https://gitee.com/liudegui/DatabaseLayer) 的 C++14 现代化重写。原始项目基于 C++03 风格，存在裸 new/delete、全局静态变量、const_cast 模拟移动语义等问题。

dbpp 保留原始项目的核心设计思想（统一 SQLite3/MySQL 操作接口），但以 C++14 标准重新实现，遵循 MISRA C++ 和 Google C++ Style Guide。

## 2. 原始代码问题分析

| 问题 | 位置 | 严重程度 |
|------|------|----------|
| 裸 new/delete | CppMySQLException (new std::string), CppMySQLResultSet (new vector), CppMySQLStatement (new vector) | 高 |
| const_cast 模拟移动语义 | 所有 Query/ResultSet/Statement 的拷贝构造和赋值 | 高 |
| 全局静态变量 | s_DBName, s_nValue, s_dwValue (CppMySQL.cpp) | 高 (线程不安全) |
| 宏污染 | `#undef NULL` / `#define NULL 0` | 中 |
| sprintf 缓冲区溢出 | tableExists, createDB, dropDB 中的 char[256] | 中 |
| 内嵌 sqlite3 源码 | sqlite3.c (7386 行) + 大量 MySQL 头文件 | 低 (维护负担) |
| CppUnit 测试框架 | 过时，不如 Catch2 | 低 |
| 无 CI/CD | 无 GitHub Actions | 低 |
| 虚基类 + typedef 切换 | DatabaseLayer.h 中 #ifdef 选择实现 | 设计缺陷 |

## 3. 设计目标

- C++14 标准 (嵌入式兼容，不要求 C++17)
- RAII 管理所有数据库资源 (连接、语句、结果集)
- 移动语义替代 const_cast hack
- 零全局状态，线程安全
- FetchContent 管理 SQLite3 依赖 (不内嵌源码)
- Catch2 v3 测试框架
- GitHub Actions CI (Linux + macOS, Debug + Release, Sanitizers)
- mccc-bus 目录结构

## 4. 架构

```
dbpp/
  include/dbpp/
    error.hpp              -- 错误码 (enum class, 不用异常)
    sqlite3_db.hpp         -- SQLite3 数据库连接
    sqlite3_query.hpp      -- SQLite3 查询结果 (前向遍历)
    sqlite3_result_set.hpp -- SQLite3 结果集 (随机访问)
    sqlite3_statement.hpp  -- SQLite3 预编译语句
  tests/
    test_error.cpp
    test_sqlite3_db.cpp
    test_sqlite3_query.cpp
    test_sqlite3_result_set.cpp
    test_sqlite3_statement.cpp
  examples/
    sqlite3_demo.cpp       -- 基本 CRUD 示例
  .github/
    workflows/
      ci.yml               -- CI workflow
  CMakeLists.txt
  CPPLINT.cfg
  LICENSE
  README.md
  README_zh.md
```

### 4.1 为什么不做 MySQL

原始项目的 MySQL 支持依赖 libmysqlclient 头文件（内嵌了 40+ 个 MySQL 私有头文件），这些头文件版本过旧且不可移植。现代 MySQL 连接应使用 MySQL Connector/C++ 或 MariaDB Connector/C，但这会引入重量级外部依赖。

dbpp 第一版只实现 SQLite3 后端。SQLite3 是嵌入式场景最常用的数据库，通过 FetchContent 可以零配置集成。MySQL 后端作为未来扩展预留接口。

### 4.2 为什么不用虚基类

原始项目用虚基类 `DatabaseLayer` + `#ifdef` typedef 切换实现。这种设计既有虚函数开销，又无法在运行时切换后端。

dbpp 采用具体类直接使用的方式。如果未来需要多后端，可以通过模板参数化或编译期策略模式实现，不引入虚函数。

## 5. 模块设计

### 5.1 error.hpp -- 错误处理

```cpp
namespace dbpp {

enum class ErrorCode : int32_t {
  kOk = 0,
  kError = -1,
  kNotOpen = -2,
  kBusy = -3,
  kNotFound = -4,
  kConstraint = -5,
  kMismatch = -6,
  kMisuse = -7,
  kRange = -8,
  kNullParam = -9,
};

// 错误信息: 错误码 + 消息字符串
struct Error {
  ErrorCode code = ErrorCode::kOk;
  char message[256] = {};

  bool ok() const { return code == ErrorCode::kOk; }
  explicit operator bool() const { return ok(); }
};

}  // namespace dbpp
```

不使用异常。所有可能失败的操作返回 `Error` 或通过输出参数返回。这与嵌入式 C++ 的 `-fno-exceptions` 约束兼容。

### 5.2 sqlite3_db.hpp -- 数据库连接

```cpp
namespace dbpp {

class Sqlite3Db {
 public:
  Sqlite3Db() = default;
  ~Sqlite3Db();  // 自动 close

  // 移动语义
  Sqlite3Db(Sqlite3Db&& other) noexcept;
  Sqlite3Db& operator=(Sqlite3Db&& other) noexcept;

  // 禁止拷贝
  Sqlite3Db(const Sqlite3Db&) = delete;
  Sqlite3Db& operator=(const Sqlite3Db&) = delete;

  Error Open(const char* path);
  void Close();
  bool IsOpen() const;

  // DML: CREATE/DROP/INSERT/UPDATE/DELETE
  // 返回受影响行数，错误通过 out_error 返回
  int32_t ExecDml(const char* sql, Error* out_error = nullptr);

  // 标量查询: SELECT count(*), SELECT max(x) 等
  int32_t ExecScalar(const char* sql, int32_t null_value = 0,
                     Error* out_error = nullptr);

  // 查询
  Sqlite3Query ExecQuery(const char* sql, Error* out_error = nullptr);

  // 结果集 (全量加载，支持随机访问)
  Sqlite3ResultSet GetResultSet(const char* sql, Error* out_error = nullptr);

  // 预编译语句
  Sqlite3Statement CompileStatement(const char* sql,
                                     Error* out_error = nullptr);

  // 表是否存在
  bool TableExists(const char* table);

  // 事务
  Error BeginTransaction();
  Error Commit();
  Error Rollback();
  bool InTransaction() const;

  // 忙等超时
  void SetBusyTimeout(int32_t ms);

  sqlite3* Handle() const { return db_; }

 private:
  sqlite3* db_ = nullptr;
};

}  // namespace dbpp
```

### 5.3 sqlite3_query.hpp -- 查询结果 (前向遍历)

```cpp
namespace dbpp {

class Sqlite3Query {
 public:
  Sqlite3Query() = default;
  ~Sqlite3Query();

  // 移动语义
  Sqlite3Query(Sqlite3Query&& other) noexcept;
  Sqlite3Query& operator=(Sqlite3Query&& other) noexcept;

  // 禁止拷贝
  Sqlite3Query(const Sqlite3Query&) = delete;
  Sqlite3Query& operator=(const Sqlite3Query&) = delete;

  int32_t NumFields() const;
  int32_t FieldIndex(const char* name) const;
  const char* FieldName(int32_t col) const;
  int32_t FieldDataType(int32_t col) const;

  const char* FieldValue(int32_t col) const;
  const char* FieldValue(const char* name) const;
  bool FieldIsNull(int32_t col) const;

  int32_t GetInt(int32_t col, int32_t null_value = 0) const;
  int32_t GetInt(const char* name, int32_t null_value = 0) const;
  double GetDouble(int32_t col, double null_value = 0.0) const;
  double GetDouble(const char* name, double null_value = 0.0) const;
  const char* GetString(int32_t col, const char* null_value = "") const;
  const char* GetString(const char* name, const char* null_value = "") const;
  const uint8_t* GetBlob(int32_t col, int32_t& out_len) const;

  bool Eof() const;
  void NextRow();
  void Finalize();

 private:
  friend class Sqlite3Db;
  friend class Sqlite3Statement;
  Sqlite3Query(sqlite3* db, sqlite3_stmt* stmt, bool eof);

  sqlite3* db_ = nullptr;
  sqlite3_stmt* stmt_ = nullptr;
  bool eof_ = true;
  int32_t num_fields_ = 0;
};

}  // namespace dbpp
```

### 5.4 sqlite3_result_set.hpp -- 结果集 (随机访问)

```cpp
namespace dbpp {

class Sqlite3ResultSet {
 public:
  Sqlite3ResultSet() = default;
  ~Sqlite3ResultSet();

  Sqlite3ResultSet(Sqlite3ResultSet&& other) noexcept;
  Sqlite3ResultSet& operator=(Sqlite3ResultSet&& other) noexcept;

  Sqlite3ResultSet(const Sqlite3ResultSet&) = delete;
  Sqlite3ResultSet& operator=(const Sqlite3ResultSet&) = delete;

  int32_t NumFields() const;
  uint32_t NumRows() const;

  int32_t FieldIndex(const char* name) const;
  const char* FieldName(int32_t col) const;
  const char* FieldValue(int32_t col) const;
  const char* FieldValue(const char* name) const;
  bool FieldIsNull(int32_t col) const;

  bool Eof() const;
  void NextRow();
  void SeekRow(uint32_t row);
  void Finalize();

 private:
  friend class Sqlite3Db;
  Sqlite3ResultSet(char** results, uint32_t rows, int32_t cols);

  char** results_ = nullptr;
  uint32_t num_rows_ = 0;
  int32_t num_cols_ = 0;
  uint32_t current_row_ = 0;
};

}  // namespace dbpp
```

### 5.5 sqlite3_statement.hpp -- 预编译语句

```cpp
namespace dbpp {

class Sqlite3Statement {
 public:
  Sqlite3Statement() = default;
  ~Sqlite3Statement();

  Sqlite3Statement(Sqlite3Statement&& other) noexcept;
  Sqlite3Statement& operator=(Sqlite3Statement&& other) noexcept;

  Sqlite3Statement(const Sqlite3Statement&) = delete;
  Sqlite3Statement& operator=(const Sqlite3Statement&) = delete;

  // DML 执行，返回受影响行数
  int32_t ExecDml(Error* out_error = nullptr);

  // SELECT 执行
  Sqlite3Query ExecQuery(Error* out_error = nullptr);

  // 参数绑定 (1-based index)
  Error Bind(int32_t param, const char* value);
  Error Bind(int32_t param, int32_t value);
  Error Bind(int32_t param, double value);
  Error Bind(int32_t param, const uint8_t* blob, int32_t len);
  Error BindNull(int32_t param);

  Error Reset();
  void Finalize();

 private:
  friend class Sqlite3Db;
  Sqlite3Statement(sqlite3* db, sqlite3_stmt* stmt);

  sqlite3* db_ = nullptr;
  sqlite3_stmt* stmt_ = nullptr;
};

}  // namespace dbpp
```

## 6. 资源管理

所有类遵循 RAII:

| 类 | 持有资源 | 析构行为 |
|----|----------|----------|
| Sqlite3Db | sqlite3* | sqlite3_close() |
| Sqlite3Query | sqlite3_stmt* | sqlite3_finalize() |
| Sqlite3ResultSet | char** (sqlite3_get_table) | sqlite3_free_table() |
| Sqlite3Statement | sqlite3_stmt* | sqlite3_finalize() |

所有类支持移动语义，禁止拷贝。移动后源对象的指针置 nullptr。

## 7. 错误处理策略

- 不使用 C++ 异常 (兼容 `-fno-exceptions`)
- 可能失败的操作通过 `Error*` 输出参数返回错误信息
- 简单查询方法 (GetInt/GetString 等) 返回默认值，不报错
- `Error::ok()` 和 `operator bool()` 方便检查

## 8. 依赖管理

```cmake
# SQLite3: FetchContent 从 GitHub 拉取 amalgamation
FetchContent_Declare(sqlite3
    URL https://www.sqlite.org/2024/sqlite-amalgamation-3450000.zip
    DOWNLOAD_EXTRACT_TIMESTAMP ON)

# Catch2: FetchContent
FetchContent_Declare(Catch2
    URL https://ghfast.top/https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
```

SQLite3 amalgamation 编译为静态库，dbpp 头文件 include sqlite3.h。

## 9. CI Workflow

```yaml
jobs:
  build-and-test:
    strategy:
      matrix:
        os: [ubuntu-latest, macos-latest]
        build_type: [Debug, Release]

  sanitizers:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        sanitizer: [address, undefined]
```

## 10. 与原始 DatabaseLayer 对比

| 维度 | DatabaseLayer | dbpp |
|------|--------------|------|
| 语言标准 | C++03 | C++14 |
| 资源管理 | 裸 new/delete | RAII + 移动语义 |
| 错误处理 | throw 异常 | Error 结构体 (无异常) |
| 线程安全 | 全局 static 变量 | 零全局状态 |
| 拷贝语义 | const_cast hack | 禁止拷贝，支持移动 |
| 数据库后端 | SQLite3 + MySQL | SQLite3 (MySQL 预留) |
| 依赖管理 | 内嵌源码 | FetchContent |
| 测试框架 | CppUnit | Catch2 v3 |
| CI | 无 | GitHub Actions |
| 代码规范 | 无 | MISRA C++ / Google Style |
| 缓冲区安全 | sprintf | snprintf |

## 11. 命名约定

- 命名空间: `dbpp`
- 类名: PascalCase (`Sqlite3Db`, `Sqlite3Query`)
- 方法名: PascalCase (`ExecDml`, `GetInt`)
- 成员变量: snake_case + 下划线后缀 (`db_`, `stmt_`)
- 常量: k 前缀 + PascalCase (`kOk`, `kError`)
- 文件名: snake_case (`sqlite3_db.hpp`)

## 12. 未来扩展

- MySQL/MariaDB 后端 (通过模板参数化或独立头文件)
- 连接池
- 异步查询
- WAL 模式配置
