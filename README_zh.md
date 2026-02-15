# dbpp - 轻量级 SQLite3 数据库抽象层

[![CI](https://github.com/DeguiLiu/dbpp/actions/workflows/ci.yml/badge.svg)](https://github.com/DeguiLiu/dbpp/actions/workflows/ci.yml)

[English](README.md) | 中文

dbpp (Database++) 是对 [DatabaseLayer](https://gitee.com/liudegui/DatabaseLayer) 的 C++14 现代化重写，提供 RAII 资源管理、move-only 语义和零异常错误处理。

## 核心特性

- C++14 header-only，内置 SQLite3 静态库
- RAII 自动资源管理，禁止拷贝，仅支持移动
- 零异常设计，兼容 `-fno-exceptions`，通过 `Error` 结构体返回错误
- 零全局状态，每个连接独立，线程安全
- 前向查询 (`Sqlite3Query`) 和随机访问结果集 (`Sqlite3ResultSet`)
- 预编译语句，1-based 参数绑定
- 事务支持 (Begin / Commit / Rollback)
- 51 个 Catch2 测试用例，ASan + UBSan 验证通过
- GitHub Actions CI (Linux + macOS, Debug + Release)
- 遵循 MISRA C++ 和 Google C++ Style Guide

## 快速开始

```cpp
#include "dbpp/sqlite3_db.hpp"
#include <cstdio>

int main() {
    dbpp::Sqlite3Db db;
    db.Open(":memory:");
    db.ExecDml("CREATE TABLE emp(empno INTEGER, empname TEXT);");

    // 预编译语句插入
    auto stmt = db.CompileStatement("INSERT INTO emp VALUES(?, ?);");
    stmt.Bind(1, 1);
    stmt.Bind(2, "Alice");
    stmt.ExecDml();
    stmt.Reset();
    stmt.Bind(1, 2);
    stmt.Bind(2, "Bob");
    stmt.ExecDml();
    stmt.Finalize();

    // 前向查询
    auto q = db.ExecQuery("SELECT * FROM emp ORDER BY empno;");
    while (!q.Eof()) {
        std::printf("empno=%d empname=%s\n",
                    q.GetInt(0), q.GetString(1));
        q.NextRow();
    }

    // 标量查询
    int32_t count = db.ExecScalar("SELECT count(*) FROM emp;");
    std::printf("total: %d\n", count);

    return 0;
}
```

## 核心类

| 类 | 功能 |
|----|------|
| `dbpp::Error` | 错误码 (ErrorCode enum) + 消息字符串，无异常 |
| `dbpp::Sqlite3Db` | 数据库连接 (Open/Close/ExecDml/ExecQuery/事务) |
| `dbpp::Sqlite3Query` | 前向只读查询结果 (Eof/NextRow/GetInt/GetString) |
| `dbpp::Sqlite3ResultSet` | 随机访问结果集 (SeekRow/NumRows/FieldValue) |
| `dbpp::Sqlite3Statement` | 预编译语句 (Bind/ExecDml/ExecQuery/Reset) |

## 构建与测试

依赖: CMake 3.14+, C++14 编译器 (GCC 5+, Clang 5+), SQLite3 (内置), Catch2 v3.5.2 (FetchContent 自动下载)

```bash
cmake -B build -DDBPP_BUILD_TESTS=ON -DDBPP_BUILD_EXAMPLES=ON
cmake --build build -j$(nproc)
cd build && ctest --output-on-failure
```

| CMake 选项 | 默认值 | 说明 |
|------------|--------|------|
| `DBPP_BUILD_TESTS` | ON | 构建测试 |
| `DBPP_BUILD_EXAMPLES` | ON | 构建示例 |

## 项目结构

```
include/dbpp/              -- 公共头文件
  error.hpp                -- ErrorCode + Error 结构体
  sqlite3_db.hpp           -- 数据库连接 (RAII)
  sqlite3_query.hpp        -- 前向查询结果
  sqlite3_result_set.hpp   -- 随机访问结果集
  sqlite3_statement.hpp    -- 预编译语句
third_party/sqlite3/       -- SQLite3 amalgamation
tests/                     -- 51 个 Catch2 测试
examples/
  sqlite3_demo.cpp         -- CRUD 示例
docs/
  design_zh.md             -- 设计文档
.github/workflows/
  ci.yml                   -- GitHub Actions CI
```

## API 示例

### 事务处理

```cpp
dbpp::Sqlite3Db db;
db.Open(":memory:");
db.ExecDml("CREATE TABLE emp(empno INTEGER, empname TEXT);");

db.BeginTransaction();
db.ExecDml("INSERT INTO emp VALUES(1, 'Alice');");
db.ExecDml("INSERT INTO emp VALUES(2, 'Bob');");
db.Commit();  // 或 db.Rollback()
```

### 预编译语句批量插入

```cpp
auto stmt = db.CompileStatement("INSERT INTO emp VALUES(?, ?);");
for (int32_t i = 0; i < 100; ++i) {
    char name[32];
    std::snprintf(name, sizeof(name), "Emp%02d", i);
    stmt.Bind(1, i);
    stmt.Bind(2, name);
    stmt.ExecDml();
    stmt.Reset();
}
stmt.Finalize();
```

### 类型安全访问

```cpp
auto q = db.ExecQuery("SELECT id, name, score FROM students;");
while (!q.Eof()) {
    int32_t id       = q.GetInt(0);
    const char* name = q.GetString(1);
    double score     = q.GetDouble(2);
    int32_t missing  = q.GetInt(3, -1);   // NULL 返回默认值
    bool is_null     = q.FieldIsNull(2);
    q.NextRow();
}
```

### 错误处理

```cpp
dbpp::Error err;
db.ExecDml("INVALID SQL", &err);
if (!err.ok()) {
    std::printf("error %d: %s\n", static_cast<int>(err.code), err.message);
}
```

## 与原 DatabaseLayer 对比

| 维度 | DatabaseLayer | dbpp |
|------|---------------|------|
| C++ 标准 | C++03 | C++14 |
| 资源管理 | 裸 new/delete | RAII + move 语义 |
| 错误处理 | throw 异常 | Error 结构体 (无异常) |
| 线程安全 | 全局静态变量 | 零全局状态 |
| 拷贝语义 | const_cast hack | 禁止拷贝，仅 move |
| 后端支持 | SQLite3 + MySQL | SQLite3 (MySQL 预留) |
| 依赖管理 | 嵌入源码 | bundled amalgamation + FetchContent |
| 测试框架 | CppUnit | Catch2 v3 |
| CI | 无 | GitHub Actions (Linux + macOS) |
| 代码规范 | 无 | MISRA C++ / Google Style |
| 缓冲区安全 | sprintf | snprintf |

## 许可证

MIT License - 详见 [LICENSE](LICENSE)

## 仓库

- GitHub: [DeguiLiu/dbpp](https://github.com/DeguiLiu/dbpp)
- Gitee: [liudegui/dbpp](https://gitee.com/liudegui/dbpp)
- 原项目: [DatabaseLayer](https://gitee.com/liudegui/DatabaseLayer)
- 设计文档: [docs/design_zh.md](docs/design_zh.md)
