#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <variant>

namespace aegis {

/// Base exception for all Aegis errors
class AegisError : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class ConfigError : public AegisError {
    using AegisError::AegisError;
};
class BundleError : public AegisError {
    using AegisError::AegisError;
};
class SignatureError : public AegisError {
    using AegisError::AegisError;
};
class InstallError : public AegisError {
    using AegisError::AegisError;
};
class SlotError : public AegisError {
    using AegisError::AegisError;
};
class BootError : public AegisError {
    using AegisError::AegisError;
};
class CryptError : public AegisError {
    using AegisError::AegisError;
};
class MountError : public AegisError {
    using AegisError::AegisError;
};
class ChecksumError : public AegisError {
    using AegisError::AegisError;
};

/// Lightweight result type: holds either a value or an error string.
template <typename T> class Result {
  public:
    static Result ok(T value) {
        return Result{std::move(value), {}};
    }
    static Result err(std::string msg) {
        return Result{{}, std::move(msg)};
    }

    explicit operator bool() const {
        return error_.empty();
    }
    const T& value() const {
        return value_;
    }
    T& value() {
        return value_;
    }
    const std::string& error() const {
        return error_;
    }

  private:
    Result(T val, std::string err) : value_(std::move(val)), error_(std::move(err)) {}
    T value_;
    std::string error_;
};

/// Specialization for void results
template <> class Result<void> {
  public:
    static Result ok() {
        return Result{""};
    }
    static Result err(std::string msg) {
        return Result{std::move(msg)};
    }

    explicit operator bool() const {
        return error_.empty();
    }
    const std::string& error() const {
        return error_;
    }

  private:
    explicit Result(std::string err) : error_(std::move(err)) {}
    std::string error_;
};

} // namespace aegis
