cmake_minimum_required(VERSION 3.14)
project(dbpp VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)

# --- SQLite3: build from bundled amalgamation ---
add_library(sqlite3_lib STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3/sqlite3.c)
target_include_directories(sqlite3_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite3)
target_compile_definitions(sqlite3_lib PRIVATE
    SQLITE_THREADSAFE=1)
if(UNIX)
    target_link_libraries(sqlite3_lib PUBLIC pthread dl)
endif()

# --- dbpp: header-only + sqlite3 ---
add_library(dbpp INTERFACE)
target_include_directories(dbpp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(dbpp INTERFACE sqlite3_lib)
target_compile_features(dbpp INTERFACE cxx_std_14)

# --- Examples ---
option(DBPP_BUILD_EXAMPLES "Build examples" ON)
if(DBPP_BUILD_EXAMPLES)
    add_executable(dbpp_sqlite3_demo examples/sqlite3_demo.cpp)
    target_link_libraries(dbpp_sqlite3_demo PRIVATE dbpp)
endif()

# --- Tests ---
option(DBPP_BUILD_TESTS "Build tests" ON)
if(DBPP_BUILD_TESTS)
    enable_testing()
    include(FetchContent)
    set(CATCH2_VERSION v3.5.2)
    FetchContent_Declare(Catch2
        URL https://ghfast.top/https://github.com/catchorg/Catch2/archive/refs/tags/${CATCH2_VERSION}.zip
        DOWNLOAD_EXTRACT_TIMESTAMP ON)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)

    add_executable(dbpp_tests
        tests/test_error.cpp
        tests/test_sqlite3_db.cpp
        tests/test_sqlite3_query.cpp
        tests/test_sqlite3_result_set.cpp
        tests/test_sqlite3_statement.cpp)
    target_link_libraries(dbpp_tests PRIVATE dbpp Catch2::Catch2WithMain)
    catch_discover_tests(dbpp_tests
        PROPERTIES SKIP_RETURN_CODE 4)
endif()
