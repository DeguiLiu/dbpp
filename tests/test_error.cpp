// Copyright (c) 2024 liudegui. MIT License.
// Tests for dbpp::Error.

#include <catch2/catch_test_macros.hpp>
#include <cstring>

#include "dbpp/error.hpp"

using namespace dbpp;

TEST_CASE("Error: default is ok", "[error]") {
  Error err;
  REQUIRE(err.ok());
  REQUIRE(static_cast<bool>(err));
  REQUIRE(err.code == ErrorCode::kOk);
}

TEST_CASE("Error: Ok() factory", "[error]") {
  Error err = Error::Ok();
  REQUIRE(err.ok());
}

TEST_CASE("Error: Make() factory", "[error]") {
  Error err = Error::Make(ErrorCode::kError, "something failed");
  REQUIRE_FALSE(err.ok());
  REQUIRE(err.code == ErrorCode::kError);
  REQUIRE(std::strstr(err.message, "something failed") != nullptr);
}

TEST_CASE("Error: Make() without message", "[error]") {
  Error err = Error::Make(ErrorCode::kNotOpen);
  REQUIRE_FALSE(err.ok());
  REQUIRE(err.code == ErrorCode::kNotOpen);
  REQUIRE(err.message[0] == '\0');
}

TEST_CASE("Error: Set()", "[error]") {
  Error err;
  err.Set(ErrorCode::kBusy, "database is busy");
  REQUIRE_FALSE(err.ok());
  REQUIRE(std::strcmp(err.message, "database is busy") == 0);
}

TEST_CASE("Error: Set() with nullptr message", "[error]") {
  Error err;
  err.Set(ErrorCode::kError, nullptr);
  REQUIRE_FALSE(err.ok());
  REQUIRE(err.message[0] == '\0');
}

TEST_CASE("Error: Clear()", "[error]") {
  Error err = Error::Make(ErrorCode::kError, "fail");
  REQUIRE_FALSE(err.ok());
  err.Clear();
  REQUIRE(err.ok());
}

TEST_CASE("Error: message truncation", "[error]") {
  // Build a string longer than 256
  char long_msg[512];
  std::memset(long_msg, 'x', sizeof(long_msg) - 1);
  long_msg[sizeof(long_msg) - 1] = '\0';

  Error err;
  err.Set(ErrorCode::kError, long_msg);
  REQUIRE(std::strlen(err.message) < Error::kMaxMessageLen);
  REQUIRE(err.message[Error::kMaxMessageLen - 1] == '\0');
}
