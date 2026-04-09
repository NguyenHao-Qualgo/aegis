#pragma once

#include <stdexcept>
#include <string>
#include <variant>
#include <optional>
#include <system_error>

namespace rauc {

/// Base exception for all RAUC errors
class RaucError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class ConfigError    : public RaucError { using RaucError::RaucError; };
class BundleError    : public RaucError { using RaucError::RaucError; };
class SignatureError : public RaucError { using RaucError::RaucError; };
class InstallError   : public RaucError { using RaucError::RaucError; };
class SlotError      : public RaucError { using RaucError::RaucError; };
class BootError      : public RaucError { using RaucError::RaucError; };
class CryptError     : public RaucError { using RaucError::RaucError; };
class MountError     : public RaucError { using RaucError::RaucError; };
class ChecksumError  : public RaucError { using RaucError::RaucError; };

/// Lightweight result type: holds either a value or an error string.
template <typename T>
class Result {
public:
    static Result ok(T value) { return Result{std::move(value), {}}; }
    static Result err(std::string msg) { return Result{{}, std::move(msg)}; }

    explicit operator bool() const { return error_.empty(); }
    const T& value() const { return value_; }
    T& value() { return value_; }
    const std::string& error() const { return error_; }

private:
    Result(T val, std::string err) : value_(std::move(val)), error_(std::move(err)) {}
    T value_;
    std::string error_;
};

/// Specialization for void results
template <>
class Result<void> {
public:
    static Result ok() { return Result{""}; }
    static Result err(std::string msg) { return Result{std::move(msg)}; }

    explicit operator bool() const { return error_.empty(); }
    const std::string& error() const { return error_; }

private:
    explicit Result(std::string err) : error_(std::move(err)) {}
    std::string error_;
};

} // namespace rauc
