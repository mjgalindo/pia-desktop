// Copyright (c) 2022 Private Internet Access, Inc.
//
// This file is part of the Private Internet Access Desktop Client.
//
// The Private Internet Access Desktop Client is free software: you can
// redistribute it and/or modify it under the terms of the GNU General Public
// License as published by the Free Software Foundation, either version 3 of
// the License, or (at your option) any later version.
//
// The Private Internet Access Desktop Client is distributed in the hope that
// it will be useful, but WITHOUT ANY WARRANTY; without even the implied
// warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with the Private Internet Access Desktop Client.  If not, see
// <https://www.gnu.org/licenses/>.

#include "common.h"
#line HEADER_FILE("builtin/error.h")

#ifndef BUILTIN_ERROR_H
#define BUILTIN_ERROR_H
#pragma once

#include "logging.h"

#include <QCoreApplication>
#include <QJsonObject>
#include <QStringList>

#include <exception>


class QDebug;
typedef CodeLocation ErrorLocation;

/**
 * @brief The Error class is the base class for all errors that may need
 * to be reported to the user.
 */
class COMMON_EXPORT Error : public std::exception
{
    Q_GADGET
    Q_DECLARE_TR_FUNCTIONS(Error)

public:
    enum Code : int
    {
        Success = 0,                      // No error

        Unknown = 1,                      // %1 = description (optional)
        System = 2,                       // %1 = system error code, %2 = description, %3 = operation (optional),
        Check = System,

        InvalidEnumValue,

        JsonFieldError,                   // %1 = field name, %2 = field type (empty for nonexistent fields), %3 = value (optional)
        JsonCastError,

        // Standard JSON RPC codes
        JsonRPCParseError = -32700,
        JsonRPCInvalidRequest = -32600,
        JsonRPCMethodNotFound = -32601,
        JsonRPCInvalidParams = -32602,
        JsonRPCInternalError = -32603,

        // User-defined JSON RPC codes (-32000 - -32099)
        JsonRPCConnectionLost = -32000,

        // CLI-specific exit codes
        CliInvalidArgs = -100,
        CliTimeout = -101,

        // Errors generated by IPC
        IPCNotConnected = 100,

        DaemonConnectionError = 500,

        OpenVPNError = 1000,
        OpenVPNProcessFailedToStart,
        OpenVPNProcessCrashed,
        OpenVPNManagementAcceptError,
        OpenVPNManagementListenError,
        OpenVPNManagementWriteError,
        OpenVPNTLSHandshakeError,
        OpenVPNConfigFileWriteError,
        OpenVPNAuthenticationError,
        OpenVPNDNSConfigError,
        OpenVPNProxyResolveError,
        OpenVPNProxyAuthenticationError,
        OpenVPNProxyError,
        OpenVPNHelperListenError,

        FirewallError = 1100,
        FirewallInitializationError,
        FirewallRuleFailed,

        ApiNetworkError = 1200,
        ApiRateLimitedError,
        ApiBadResponseError,
        ApiUnauthorizedError,
        ApiInterfaceError,  // Need to use a specific interface for the API request, but the interface isn't ready
        ApiPaymentRequiredError,

        VersionUnparseableError = 1300,

        TaskRejected = 1400,
        TaskStillPending,
        TaskDestroyedWhilePending,
        TaskRecipientDestroyed,
        TaskTimedOut,

        // Errors returned by Daemon RPC calls
        DaemonRPCDiagnosticsFailed = 1500,
        DaemonRPCDiagnosticsNotEnabled,
        DaemonRPCDaemonInactive,    // RPC rejected because no active client is connected
        DaemonRPCNotLoggedIn,   // RPC rejected because the user has not logged in
        DaemonRPCUnknownSetting,    // RPC rejected due to unknown setting property
        DaemonRPCDedicatedIpTokenExpired, // RPC rejected due to adding a DIP token that's expired
        DaemonRPCDedicatedIpTokenInvalid, // RPC rejected due to adding a DIP token that's invalid

        // Network adapter errors (can be thrown by Daemon implementations)
        NetworkAdapterNotFound = 1600,

        // Wireguard connectivity errors
        WireguardAddKeyFailed = 1700,
        WireguardCreateDeviceFailed,
        WireguardConfigDeviceFailed,
        WireguardDeviceLost,
        WireguardHandshakeTimeout,
        WireguardProcessFailed,
        WireguardNotResponding,
        WireguardPingTimeout,

        // Connectivity errors for multiple VPN methods
        VPNConfigInvalid = 1800,

        LocalSocketNotFound = 1900, // The local socket definitely doesn't exist at all.
        LocalSocketCannotConnect,   // The local socket might exist, but we couldn't connect.

        // Library entry point loaded at runtime is not available
        LibraryUnavailable = 2000,

        // Starting a service on Windows failed with ERROR_INCOMPATIBLE_SERVICE_SID_TYPE
        // (see win_servicestate.cpp / win_dnscachecontrol.cpp)
        WinServiceIncompatibleSidType = 2100,
    };
    Q_ENUM(Code)

#if defined(Q_OS_WIN)
    typedef unsigned long SystemCode;
#else
    typedef int SystemCode;
#endif

public:
    Error()
        : _location(nullptr, 0, *QLoggingCategory::defaultCategory()), _code(Success), _systemCode(0) {}
    Error(ErrorLocation location, Code code, QStringList params)
        : _location(std::move(location)), _code(code), _systemCode(0), _params(std::move(params)) {}
    template<typename... Args>
    Error(ErrorLocation location, Code code, Args&&... args)
        : _location(std::move(location)), _code(code), _systemCode(0), _params{ std::forward<Args>(args)... } {}
    Error(const QJsonObject& errorObject);

    Q_INVOKABLE Code code() const { return _code; }
    SystemCode systemCode() const { return _systemCode; }
    Q_INVOKABLE QString errorString() const;
    QString errorDescription() const;
    QJsonObject toJsonObject() const;
    const CodeLocation& location() const { return _location; }
    Q_INVOKABLE QString file() const { return {_location.file}; }
    Q_INVOKABLE int line() const { return _location.line; }

    operator bool() const { return _code != Success; }
    bool operator!() const { return _code == Success; }

protected:
    Error(ErrorLocation &&location, Code code, SystemCode systemCode, QStringList params)
        : _location(location), _code(code), _systemCode(systemCode), _params(std::move(params)) {}

public:
    template<typename... Args> void Q_NORETURN fatal(const char* msg, Args&&... args) const { QMessageLogger(_location.file, _location.line, nullptr, _location.category->categoryName()).fatal(msg, std::forward<Args>(args)...); }

    #define IMPLEMENT_SEVERITY(severity) \
    template<typename... Args> void severity(const char* msg, Args&&... args) const { QMessageLogger(_location.file, _location.line, nullptr, _location.category->categoryName()).severity(*_location.category, msg, std::forward<Args>(args)...); } \
    QDebug severity() const { return QMessageLogger(_location.file, _location.line, nullptr, _location.category->categoryName()).severity(*_location.category); }
    #define IMPLEMENT_SEVERITY_NOOP(severity) \
    template<typename... Args> void severity(const char* msg, Args&.. args) const {} \
    QNoDebug severity() const { return QNoDebug(); }

    IMPLEMENT_SEVERITY(critical)
    #if defined(QT_NO_WARNING_OUTPUT)
    IMPLEMENT_SEVERITY_NOOP(warning)
    #else
    IMPLEMENT_SEVERITY(warning)
    #endif
    #if defined(QT_NO_INFO_OUTPUT)
    IMPLEMENT_SEVERITY_NOOP(info)
    #else
    IMPLEMENT_SEVERITY(info)
    #endif
    #if defined(QT_NO_DEBUG_OUTPUT)
    IMPLEMENT_SEVERITY_NOOP(debug)
    #else
    IMPLEMENT_SEVERITY(debug)
    #endif

    #undef IMPLEMENT_SEVERITY
    #undef IMPLEMENT_SEVERITY_NOOP

protected:
    ErrorLocation _location;
    Code _code;
    SystemCode _systemCode;
    QStringList _params;
    QByteArray _storedFile;
};
Q_DECLARE_METATYPE(Error)

QDebug COMMON_EXPORT operator<<(QDebug d, const Error& e);

#define IMPLEMENT_ERROR_SUBCLASS_WITH_BASE(name, base, code) \
    class COMMON_EXPORT name : public base { \
    public: \
        name(ErrorLocation&& location, QStringList params) : base(std::move(location), code, std::move(params)) {} \
        template<typename... Args> name(ErrorLocation&& location, Args&&... args) : name(std::move(location), QStringList { args... }) {} \
    };
#define IMPLEMENT_ERROR_SUBCLASS(name, code) \
    IMPLEMENT_ERROR_SUBCLASS_WITH_BASE(name, Error, code)


// Subclasses with convenience constructors. IMPORTANT: must not add any members
// or override any virtual functions; they need to be possible to pass around as
// a plain Error instance.

IMPLEMENT_ERROR_SUBCLASS(UnknownError, Error::Unknown)

extern "C" {
#ifdef Q_OS_WIN
extern Q_DECL_IMPORT unsigned long ATTR_stdcall GetLastError();
#define SYSTEM_LAST_ERROR (::GetLastError())
#else
extern int errno;
#define SYSTEM_LAST_ERROR (errno)
#endif
}

class COMMON_EXPORT SystemError : public Error
{
public:
    SystemError(ErrorLocation&& location, SystemCode errorCode, _D(const char* operation)_R(const std::nullptr_t&) = nullptr);
    SystemError(ErrorLocation&& location, _D(const char* operation)_R(const std::nullptr_t&) = nullptr)
        : SystemError(std::move(location), SYSTEM_LAST_ERROR _D(, operation)) {}
private:
    static QString getSystemErrorString(SystemCode errorCode);
};

typedef SystemError CheckError;

// Throw a check failure exception with a string describing the operation.
// The string is ignored in release builds. The second optional argument
// is a system error code; if omitted, CHECK_THROW will get the code from
// the operating system.
#define CHECK_THROW(operation, ...)  throw CheckError(HERE,##__VA_ARGS__,_D(operation)_R(nullptr))

// Check if a value (typically a return value) matches a certain predicate,
// and throws an exception if it does. Useful for making flat RAII-based
// call chains instead of nested if statements and cleanups.
#define CHECK_IF(predicate,expr)     ([](auto&& value){ auto error = SYSTEM_LAST_ERROR; if (predicate)         CHECK_THROW(#expr, error); return std::forward<decltype(value)>(value); }(expr))
// Same as CHECK_IF except throws an exception if the value does NOT match
// the given predicate.
#define CHECK_IF_NOT(predicate,expr) ([](auto&& value){ auto error = SYSTEM_LAST_ERROR; if (predicate) {} else CHECK_THROW(#expr, error); return std::forward<decltype(value)>(value); }(expr))

// Check the result of a function that directly returns an error code.
#define CHECK_ERROR_IF(predicate,expr) (([](auto&& error){ if (error && (predicate)) CHECK_THROW(#expr, error); return std::forward<decltype(error)>(error); })(expr))
#define CHECK_ERROR(expr)            CHECK_ERROR_IF(true,expr)

#define CHECK_IF_TRUE(expr)          CHECK_IF(value, expr)
#define CHECK_IF_ZERO(expr)          CHECK_IF(value == 0, expr)
#define CHECK_IF_POSITIVE(expr)      CHECK_IF(value > 0, expr)
#define CHECK_IF_NEGATIVE(expr)      CHECK_IF(value < 0, expr)
#define CHECK_IF_NULL(expr)          CHECK_IF(value == nullptr, expr)

#define CHECK_IF_FALSE(expr)         CHECK_IF_NOT(value, expr)
#define CHECK_IF_NOT_ZERO(expr)      CHECK_IF_NOT(value == 0, expr)
#define CHECK_IF_NOT_POSITIVE(expr)  CHECK_IF_NOT(value > 0, expr)
#define CHECK_IF_NOT_NEGATIVE(expr)  CHECK_IF_NOT(value < 0, expr)
#define CHECK_IF_NOT_NULL(expr)      CHECK_IF_NOT(value == nullptr, expr)

// Check for the -1 error return value commonly used by POSIX functions.
#define CHECK_IF_MINUS_ONE(expr)     CHECK_IF(value == -1, expr)
// Check for the INVALID_HANDLE_VALUE error return value on Windows.
#define CHECK_IF_INVALID(expr)       CHECK_IF(value == INVALID_HANDLE_VALUE, expr)

#ifdef Q_OS_WIN
#define CHECK_ALLOC(expr)            CHECK_IF((value == nullptr ? (error = ERROR_NOT_ENOUGH_MEMORY), true : false), expr)
#else
#define CHECK_ALLOC(expr)            CHECK_IF((value == nullptr ? (error = ENOMEM), true : false), expr)
#endif


IMPLEMENT_ERROR_SUBCLASS(JsonFieldError, Error::JsonFieldError)

class COMMON_EXPORT JsonRPCError : public Error { public: using Error::Error; };
IMPLEMENT_ERROR_SUBCLASS_WITH_BASE(JsonRPCParseError, JsonRPCError, Error::JsonRPCParseError)
IMPLEMENT_ERROR_SUBCLASS_WITH_BASE(JsonRPCInvalidRequestError, JsonRPCError, Error::JsonRPCInvalidRequest)
IMPLEMENT_ERROR_SUBCLASS_WITH_BASE(JsonRPCMethodNotFoundError, JsonRPCError, Error::JsonRPCMethodNotFound)
IMPLEMENT_ERROR_SUBCLASS_WITH_BASE(JsonRPCInvalidParamsError, JsonRPCError, Error::JsonRPCInvalidParams)
IMPLEMENT_ERROR_SUBCLASS_WITH_BASE(JsonRPCInternalError, JsonRPCError, Error::JsonRPCInternalError)


// Global function to report an Error from a location that cannot throw
// an exception (e.g. in a slot that might be invoked directly by Qt).
//
extern void COMMON_EXPORT reportError(Error error);

// Macro to run an expression or statement or block inside a try..catch
// block and report any errors using a report function (should take a
// single parameter of type Error).
//
#define GUARD_WITH(report, ...) \
    do { \
        try \
        { \
            __VA_ARGS__; \
        } \
        catch (const Error& e) \
        { \
            qDebug() << e; \
            report(e); \
        } \
        catch (const std::exception& e) \
        { \
            qWarning() << "Caught exception:" << e.what(); \
            report(UnknownError(HERE, e.what())); \
        } \
        catch (...) \
        { \
            qWarning() << "Caught unknown exception"; \
            report(UnknownError(HERE)); \
        } \
    } while(0)

// Macro to run an expression or statement or block inside a try..catch
// block and report any errors using the global reportError function.
//
#define GUARD(...) GUARD_WITH(reportError,##__VA_ARGS__)

// Function to run a function/functor inside a try..catch block and report
// any errors using a report function (should take a single parameter of
// type Error). The return value of the function/functor is ignored, and
// instead this function returns true if the function/functor completed
// without throwing any exceptions, or false otherwise.
//
template<typename Func, typename Report, typename... Args>
static inline bool guardWith(Func&& func, Report&& report, Args&&... args)
{
    GUARD_WITH(report, { func(std::forward<Args>(args)...); return true; });
    return false;
}

// Function to run a member function inside a try...catch block and report
// any errors using a report function (should take a single parameter of
// type Error). The return value of the function/functor is ignored, and
// instead this function returns true if the function/functor completed
// without throwing any exceptions, or false otherwise.
//
template<class Class, typename Ptr, typename Report, typename Return, typename... Args>
static inline bool guardWith(Return (Class::*func)(Args...) const, Ptr&& context, Report&& report, Args&&... args)
{
    GUARD_WITH(report, { ((context)->*(func))(std::forward<Args>(args)...); return true; });
    return false;
}

// Function to run a function/functor inside a try..catch block and report
// any errors using the global reportError function.
//
template<typename Func, typename... Args>
static inline bool guard(Func&& func, Args&&... args)
{
    GUARD({ func(std::forward<Args>(args)...); return true; });
    return false;
}

// Function to run a member function inside a try...catch block and report
// any errors using the global reportError function.
//
template<class Class, typename Ptr, typename Return, typename... Args>
static inline bool guard(Return (Class::*func)(Args...) const, Ptr&& context, Args&&... args)
{
    GUARD({ ((context)->*(func))(std::forward<Args>(args)...); return true; });
    return false;
}

// Hash Error::Code as int
namespace std
{
    template<>
    struct hash<Error::Code> : public hash<int> {};
}

// Trace an errno value - writes numeric value and name from qt_error_string()
class COMMON_EXPORT ErrnoTracer : public DebugTraceable<ErrnoTracer>
{
public:
    ErrnoTracer(int code) : _code{code} {}
    ErrnoTracer() : ErrnoTracer{errno} {}

public:
    void trace(QDebug &dbg) const
    {
        dbg << QStringLiteral("(code: %1) %2").arg(_code)
            .arg(qPrintable(qt_error_string(errno)));
    }

private:
    int _code;
};

#endif // BUILTIN_ERROR_H
