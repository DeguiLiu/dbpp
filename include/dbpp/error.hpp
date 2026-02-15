// Copyright (c) 2024 liudegui. MIT License.
//
// dbpp::Error -- error handling without exceptions.
//
// Design:
//   - ErrorCode enum class with fixed-width underlying type
//   - Error struct: code + fixed-size message buffer
//   - Compatible with -fno-exceptions
//   - Maps SQLite3 error codes to dbpp error codes

#pragma once

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace dbpp {

// ---------------------------------------------------------------------------
// ErrorCode
// ---------------------------------------------------------------------------

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
  kIoError = -10,
  kFull = -11,
};

// ---------------------------------------------------------------------------
// Error
// ---------------------------------------------------------------------------

struct Error {
  static constexpr uint32_t kMaxMessageLen = 256;

  ErrorCode code = ErrorCode::kOk;
  char message[kMaxMessageLen] = {};

  bool ok() const { return code == ErrorCode::kOk; }
  explicit operator bool() const { return ok(); }

  void Set(ErrorCode c, const char* msg) {
    code = c;
    if (msg != nullptr) {
      std::strncpy(message, msg, kMaxMessageLen - 1);
      message[kMaxMessageLen - 1] = '\0';
    } else {
      message[0] = '\0';
    }
  }

  void SetFormat(ErrorCode c, const char* fmt, ...) {
    code = c;
    if (fmt != nullptr) {
      va_list ap;
      va_start(ap, fmt);
      std::vsnprintf(message, kMaxMessageLen, fmt, ap);
      va_end(ap);
    } else {
      message[0] = '\0';
    }
  }

  void Clear() {
    code = ErrorCode::kOk;
    message[0] = '\0';
  }

  static Error Ok() { return Error{}; }

  static Error Make(ErrorCode c, const char* msg = nullptr) {
    Error e;
    e.Set(c, msg);
    return e;
  }
};

}  // namespace dbpp
