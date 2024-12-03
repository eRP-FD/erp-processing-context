/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_UTIL_ASSERT_HXX
#define EPA_LIBRARY_UTIL_ASSERT_HXX

#include "library/log/LogTrace.hxx"
#include "library/log/Logging.hxx"
#include "library/metadata/MetadataValidationError.hxx"

#include <cstddef>
#include <stdexcept>
#include <string>

namespace epa
{

class FileNameAndLineNumber
{
public:
    const char* fileName;
    const size_t line;

    FileNameAndLineNumber(const char* fileName, const size_t line);

    std::string toString() const;
};


// Only for internal use by the macros defined below.
namespace assertion::local
{
    /**
     * This function only exists to avoid having to include OpenSslHelper.hxx in every compilation
     * unit that uses assertions.
     */
    std::string getAllOpenSslErrors();
}


/**
 * Interface that has to be implemented by classes like `Assertion` which support
 * setting an exception type.
 */
struct IExceptionTypeHandler
{
    virtual ~IExceptionTypeHandler() noexcept(false) = default;
    virtual void setThrower(std::function<void(const std::string&)>&& thrower) = 0;
};


/**
 * Encapsulate the exception type of an assertion and register it
 * at the current `Log::Implementation` instance.
 */
class AssertionThrower
{
public:
    using Thrower = std::function<void(const std::string& what)>;
    explicit AssertionThrower(Thrower&& thrower);
    void deploy(IExceptionTypeHandler& handler) const;

private:
    mutable Thrower mThrower;
};
std::ostream& operator<<(std::ostream& os, const AssertionThrower& thrower);


/**
 * An assertion message detail is appended to the log message.
 */
class Detail
{
public:
    const std::string text;

    explicit Detail(const std::string& text);
    explicit Detail(std::string&& text);
};
struct IDetailStreamProvider
{
    virtual ~IDetailStreamProvider() noexcept(false) = default;
    virtual std::ostream& getDetailStream() = 0;
};
std::ostream& operator<<(std::ostream& os, const Detail& detail);


/**
 * Usually a failed assertion is treated as error: it writes a log message with severity ERROR,
 * shows all pending trace log messages and throws an exception. In some cases a failed assertion
 * only signifies that one of several optional execution paths has failed but the others are
 * still valid, i.e. for a form of backtracking. For these cases, this class and more specifically,
 * its `NotAnErrorGuard` allow to alter the behavior of `Assert()` temporarily. With at least
 * one active `NotAnErrorGuard` a failed `Assert()` writes a log message on `INFO` level, does
 * not show pending trace log messages but still throws the exception.
 *
 * You can use macro `FailedAssertionIsNotAnError()` instead of instantiating the guard class
 * explicitly.
 */
class AssertionIsAnError : private boost::noncopyable
{
public:
    static bool value();

    class NotAnErrorGuard : private boost::noncopyable
    {
    public:
        NotAnErrorGuard();
        ~NotAnErrorGuard();

    private:
        // Save the current value of mAssertionIsAnError so that NotAnErrorGuard can be nested.
        // This requires that destructing multiple NotAnErrorGuard instance must happen in reverse
        // order of constructing (=> maybe turn AssertionIsAnError::mValue into a counter and remove
        // mSavedValue?)
        const bool mSavedValue;
    };

private:
    static thread_local bool mValue;
};


/**
 * The `Assertion` class implements a couple of interfaces so that it can take part in the
 * stream implementation that is provided by the `Log` class.
 */
class Assertion : public Log::IKeyValueHandler,
                  public IExceptionTypeHandler,
                  public Log::LogStream::UserData,
                  public IDetailStreamProvider
{
public:
    /**
     * Constructor for assertions with an expression, i.e. `Assert(expression)`.
     * The constructor only excepts arguments that have to be supplied via a macro. Everything else
     * is optional and is provided via the `<<` stream operator.
     */
    explicit Assertion(
        const char* expression,
        const Location& location,
        LogContext exceptionContext);

    /**
     * Note that the destructor may throw an exception, hence its `noexcept(false)` declaration.
     */
    //NOLINTNEXTLINE(bugprone-exception-escape)
    ~Assertion() noexcept(false) override;

    Log::LogStream& stream();

    /**
     * Accept a `Thrower` lambda which effectively redefines which type of exception to throw in
     * case of an unfulfilled assertion.
     */
    void setThrower(AssertionThrower::Thrower&& thrower) override;

    /**
     * Add a key/value pair to the JSON output. If the output format is plain text, then
     * the `keyValue` is ignored.
     */
    void addKeyValue(const Log::KeyValue& keyValue) override;

    /**
     * Forward the given `writer` to the wrapped `Log` object.
     */
    void addJsonSerializer(const Log::JsonSerializer& serializer) override;

    std::ostream& getDetailStream() override;

protected:
    Assertion(
        const Location& location,
        const std::string& initialMessage,
        LogContext exceptionContext);

    /**
     * Only to be called from the destructor(s). Cleans up member variables and in case the
     * assertion is not fulfilled, writes a log message and throws an exception.
     */
    void finish();

private:
    std::unique_ptr<Log> mLog;
    AssertionThrower::Thrower mThrower;

    /**
     * Defines the log context of the text thrown in exception. Some assertions throw exceptions
     * with `LogContext::Response` context, others with `LogContext::Log`.
     */
    LogContext mExceptionContext;

    /**
     * For compatibility with previous behavior, do not include the expression text in the
     * exception "what" text. Therefore, remember where the expression text ends so that it can be
     * removed for the exception "what" text but not for the log message.
     */
    std::string::size_type mExpressionEnd;

    /**
     * A secondary stream whose content will be appended when the log message is created.
     * I.e. `Assert(...) << "a" << logging::detail("b") << "c";`
     * will produce the output "acb".
     */
    std::stringstream mDetailStream;
};


/**
 * This class exists only so that its destructor can be declared `[[noreturn]]`. Doing that for
 * the destructor of `Assertion` did not lead to any observable problems, but technically it is
 * undefined behavior if a method that is declared `[[noreturn]]` would return.
 */
class FailureAssertion : public Assertion
{
public:
    /**
     * Constructor for `Failure()` assertions, i.e. assertions without expression.
     * The constructor only excepts arguments that have to be supplied via a macro. Everything else
     * is optional and is provided via the `<<` stream operator.
     */
    explicit FailureAssertion(const Location& location);
    /**
     * The destructor is declared [[noreturn]] so that a `Failure()` macro call does not have to
     * be followed by a `return` statement in methods that return a value.
     */
    //NOLINTNEXTLINE(bugprone-exception-escape)
    [[noreturn]] ~FailureAssertion() noexcept(false) override;
};


namespace assertion
{
    /**
     * Stream manipulator that changes the type of the exception that is thrown by an assertion.
     * It accepts additional arguments that are passed on to the exception's constructor.
     * The first and implicit argument will be the message text, but if that doesn't mach any
     * constructor, it is tried to use the message text as the last argument.
     */
    // NOLINTBEGIN(cppcoreguidelines-missing-std-forward)
    template<typename ExceptionType, class... Arguments>
    AssertionThrower exceptionType(Arguments&&... arguments)
    {
        if constexpr (std::is_constructible_v<ExceptionType, const std::string&, Arguments&&...>)
        {
            // Prepend the `message` text as first argument to the argument list, create an
            // exception from the arguments and throw it.
            return AssertionThrower(
                [... args = std::forward<Arguments>(arguments)](const std::string& message) {
                    throw ExceptionType(message, args...);
                });
        }
        else
        {
            // Add the `message` text as last argument to the argument list, create an
            // exception from the arguments and throw it.
            return AssertionThrower(
                [... args = std::forward<Arguments>(arguments)](const std::string& message) {
                    throw ExceptionType(args..., message);
                });
        }
    }
    // NOLINTEND(cppcoreguidelines-missing-std-forward)

    Detail detail(const std::string& text);
    Detail detail(std::string&& text);
}

/**
 * Assert that a given `expression` evaluates to `true`. If the assertion fails
 * 1. a message is displayed that shows the expression and the code location where the
 *    assertion was evaluated. An additional description can provide more details.
 * 2. an exception is thrown.
 *
 * The message is created with the `<<` stream operator. Example:
 *
 * ```c++
 * Assert(predicate()) << "details why predicate() evaluates to false";
 * ```
 *
 * When the assertion is fulfilled, i.e. the expression to `true` then the `Assertion` statement is
 * a noop, apart from evaluating the expression.
 *
 * The stream that is returned by `Assert()` understands
 * - text classification like `logging::sensitive("text")`
 * - `logging::keyValue(key,value)` pairs which are added to a JSON log message, but are ignored if
 *   JSON support is configured off
 * - `assertion::exceptionType<Type>(constructor arguments)` to change the exception type from
 *   `std::runtime_error` to `Type`. The first argument given to any exception constructor is the
 *   message text. If additional `constructor arguments` are provided, they are passed on to the
 *   exception constructor as additional arguments.
 */

//NOLINTBEGIN(bugprone-macro-parentheses)
/**
 * Implementation detail of the macro:
 * Removing the `static_cast<void>(0),` from the macro, that was borrowed from GLOG.
 * It interferes with ASSERT_(NO)_THROW and does not seem to serve a purpose.
 */
#define Assert(expression)                                                                         \
    (expression)                                                                                   \
        ? (void) 0                                                                                 \
        : google::logging::internal::LogMessageVoidify()                                           \
              & ::epa::Assertion(#expression, ::epa::here(), ::epa::LogContext::Log).stream()

/**
 * Same as previous macro, but the exception text is generated using LogContext::Response context.
 */
#define AssertForResponse(expression)                                                              \
    (expression) ? (void) 0                                                                        \
                 : google::logging::internal::LogMessageVoidify()                                  \
                       & ::epa::Assertion(#expression, here(), LogContext::Response).stream()

//NOLINTEND(bugprone-macro-parentheses)

// The name `Fail` causes a compile error, hence the longer `Failure`.
#define Failure() FailureAssertion(here()).stream()

/**
 * Convenience specializations.
 */

/**
 * Variant of the `Assert` macro that collects OpenSSL errors and appends them to the message.
 * At the moment, the openssl details are displayed before any additional message because
 * there is not yet possible to retain the details until the log message is complete.
 * Should that be added?
 */
#define AssertOpenSsl(expression)                                                                  \
    Assert(expression) << assertion::detail(", OpenSsl details: '")                                \
                       << assertion::detail(::epa::assertion::local::getAllOpenSslErrors())        \
                       << assertion::detail("'")

/**
 * AssertValidation throws MetadataValidationError exception that transports text intended to be
 * used in the response.
 */
#define AssertValidation(expression)                                                               \
    AssertForResponse(expression) << ::epa::assertion::exceptionType<MetadataValidationError>()

} // namespace epa

#endif
