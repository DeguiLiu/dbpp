cmake_minimum_required(VERSION 3.14)
project(dbpp VERSION 2.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

include(FetchContent)

# ---------------------------------------------------------------------------
# Proxy: ghfast.top mirror for China, empty string for direct access
# ---------------------------------------------------------------------------
if(NOT DEFINED DBPP_GITHUB_PROXY)
    set(DBPP_GITHUB_PROXY "https://ghfast.top/" CACHE STRING
        "GitHub download proxy prefix (empty for direct)")
endif()

# ---------------------------------------------------------------------------
# SQLite3: azadkuh/sqlite-amalgamation (CMake-ready mirror)
# ---------------------------------------------------------------------------
FetchContent_Declare(sqlite3
    URL ${DBPP_GITHUB_PROXY}https://github.com/azadkuh/sqlite-amalgamation/archive/refs/heads/master.zip
    DOWNLOAD_EXTRACT_TIMESTAMP ON)

set(BUILD_SHELL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(sqlite3)

# azadkuh's CMakeLists.txt only sets INSTALL_INTERFACE include dir,
# so we add BUILD_INTERFACE manually for FetchContent usage.
FetchContent_GetProperties(sqlite3 SOURCE_DIR SQLITE3_SRC_DIR)
target_include_directories(SQLite3 PUBLIC
    $<BUILD_INTERFACE:${SQLITE3_SRC_DIR}>)

# ---------------------------------------------------------------------------
# MariaDB Connector/C (optional, for MariaDB/MySQL backend)
# ---------------------------------------------------------------------------
option(DBPP_WITH_MARIADB "Build MariaDB/MySQL backend" OFF)

if(DBPP_WITH_MARIADB)
    FetchContent_Declare(mariadb_connector_c
        URL ${DBPP_GITHUB_PROXY}https://github.com/mariadb-corporation/mariadb-connector-c/archive/refs/tags/v3.3.8.zip
        DOWNLOAD_EXTRACT_TIMESTAMP ON)

    set(WITH_UNIT_TESTS OFF CACHE BOOL "" FORCE)
    set(WITH_SSL OFF CACHE BOOL "" FORCE)
    set(WITH_EXTERNAL_ZLIB OFF CACHE BOOL "" FORCE)
    set(WITH_CURL OFF CACHE BOOL "" FORCE)
    set(CLIENT_PLUGIN_DIALOG OFF CACHE BOOL "" FORCE)
    set(CLIENT_PLUGIN_CLIENT_ED25519 OFF CACHE BOOL "" FORCE)
    set(CLIENT_PLUGIN_CACHING_SHA2_PASSWORD OFF CACHE BOOL "" FORCE)
    set(CLIENT_PLUGIN_SHA256_PASSWORD OFF CACHE BOOL "" FORCE)
    set(CLIENT_PLUGIN_AUTH_GSSAPI_CLIENT OFF CACHE BOOL "" FORCE)
    set(CLIENT_PLUGIN_REMOTE_IO OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(mariadb_connector_c)
endif()

# ---------------------------------------------------------------------------
# dbpp: header-only interface library (SQLite3 backend always available)
# ---------------------------------------------------------------------------
add_library(dbpp INTERFACE)
target_include_directories(dbpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(dbpp INTERFACE SQLite3)
target_compile_features(dbpp INTERFACE cxx_std_14)

# MariaDB backend target (separate to avoid forcing mariadb dep)
if(DBPP_WITH_MARIADB)
    add_library(dbpp_mariadb INTERFACE)
    target_include_directories(dbpp_mariadb INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
    target_link_libraries(dbpp_mariadb INTERFACE dbpp mariadbclient)
    target_compile_definitions(dbpp_mariadb INTERFACE DBPP_HAS_MARIADB=1)
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
option(DBPP_BUILD_EXAMPLES "Build examples" ON)
if(DBPP_BUILD_EXAMPLES)
    add_executable(dbpp_sqlite3_demo examples/sqlite3_demo.cpp)
    target_link_libraries(dbpp_sqlite3_demo PRIVATE dbpp)

    if(DBPP_WITH_MARIADB)
        add_executable(dbpp_mariadb_demo examples/mariadb_demo.cpp)
        target_link_libraries(dbpp_mariadb_demo PRIVATE dbpp_mariadb)
    endif()
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
option(DBPP_BUILD_TESTS "Build tests" ON)
if(DBPP_BUILD_TESTS)
    enable_testing()

    set(CATCH2_VERSION v3.5.2)
    FetchContent_Declare(Catch2
        URL ${DBPP_GITHUB_PROXY}https://github.com/catchorg/Catch2/archive/refs/tags/${CATCH2_VERSION}.zip
        DOWNLOAD_EXTRACT_TIMESTAMP ON)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)

    # SQLite3 tests + template tests
    add_executable(dbpp_tests
        tests/test_error.cpp
        tests/test_sqlite3_db.cpp
        tests/test_sqlite3_query.cpp
        tests/test_sqlite3_result_set.cpp
        tests/test_sqlite3_statement.cpp
        tests/test_db_template.cpp)
    target_link_libraries(dbpp_tests PRIVATE dbpp Catch2::Catch2WithMain)
    catch_discover_tests(dbpp_tests
        PROPERTIES SKIP_RETURN_CODE 4)

    # MariaDB tests (require running MySQL/MariaDB server)
    if(DBPP_WITH_MARIADB)
        add_executable(dbpp_mariadb_tests
            tests/test_mariadb_db.cpp
            tests/test_mariadb_query.cpp
            tests/test_mariadb_statement.cpp)
        target_link_libraries(dbpp_mariadb_tests PRIVATE dbpp_mariadb Catch2::Catch2WithMain)
        catch_discover_tests(dbpp_mariadb_tests
            PROPERTIES SKIP_RETURN_CODE 4)
    endif()
endif()
