/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/util/Assert.hxx"

#include "shared/crypto/OpenSslHelper.hxx"


namespace epa
{
namespace assertion::local
{

    std::string getAllOpenSslErrors()
    {
        std::string errors;
        while (unsigned long error = ERR_get_error())
        {
            if (! errors.empty())
            {
                errors += "; ";
            }

            std::array<char, 128> buffer; // NOLINT(*-member-init)
            ERR_error_string_n(error, buffer.data(), buffer.size());
            errors += std::string_view(buffer.data());
        }
        return errors;
    }
}

FileNameAndLineNumber::FileNameAndLineNumber(const char* fileName, const size_t line)
  : fileName(fileName),
    line(line)
{
}


std::string FileNameAndLineNumber::toString() const
{
    std::ostringstream oss;
    oss << fileName << ":" << line;
    return oss.str();
}


std::ostream& operator<<(std::ostream& os, const AssertionThrower& thrower)
{
    auto* handler =
        dynamic_cast<Assertion::IExceptionTypeHandler*>(Log::LogStream::getUserData(os));
    if (handler != nullptr)
        thrower.deploy(*handler);
    return os;
}


Assertion::Assertion(const char* expression, const Location& location, LogContext exceptionContext)
  : Assertion(
        location,
        logging::development(
            "assertion '" + std::string(expression) + "' not fulfilled: ",
            "assertion not fulfilled: ")
            .toString(LogContext::Log),
        exceptionContext)
{
}


Assertion::Assertion(
    const Location& location,
    const std::string& initialMessage,
    LogContext exceptionContext)
  : mLog(std::make_unique<Log>(
        AssertionIsAnError::value() ? Log::Severity::ERROR : Log::Severity::INFO,
        Location(location),
        Log::Mode::IMMEDIATE)),
    mThrower(),
    mExceptionContext(exceptionContext),
    mExpressionEnd(0),
    mDetailStream()
{
    // Allow the stream manipulators to find this object.
    mLog->stream().setUserData(*this);

    if (! mLog->isJson())
        stream() << "ERROR at " << location.toShortString() << ": ";
    stream() << initialMessage;
    const auto message = mLog->getMessage(mExceptionContext);
    mExpressionEnd = message.size();
}


//NOLINTNEXTLINE(bugprone-exception-escape)
Assertion::~Assertion() noexcept(false)
{
    finish();
}


void Assertion::finish()
{
    if (mLog == nullptr)
    {
        // finish() may be called twice. Ignore the second call.
        return;
    }

    // Show all pending trace messages.
    if (AssertionIsAnError::value())
        SHOW_LOG_TRACE();

    // Prepare to exit the destructor with an exception by cleaning members up as good as possible.
    auto thrower = std::move(mThrower);
    mLog->stream() << mDetailStream.str();
    const std::string message = mLog->getMessage(mExceptionContext).substr(mExpressionEnd);
    // mExpressionEnd is not a complex type. No cleaning up necessary.

    // Write the log message.
    mLog->outputLog(false);

    // Destroy mLog to make finish idempotent.
    mLog.reset();

    if (thrower)
        thrower(message);

    // Fallback to the default exception type.
    throw std::runtime_error(message);
}


Log::LogStream& Assertion::stream()
{
    return mLog->stream();
}


void Assertion::setThrower(AssertionThrower::Thrower&& thrower)
{
    mThrower = std::move(thrower);
}


void Assertion::addKeyValue(const Log::KeyValue& keyValue)
{
    mLog->getImplementation().addKeyValue(keyValue);
}


void Assertion::addJsonSerializer(const Log::JsonSerializer& serializer)
{
    mLog->getImplementation().addJsonSerializer(serializer);
}


std::ostream& Assertion::getDetailStream()
{
    return mDetailStream;
}


thread_local bool AssertionIsAnError::mValue = true;

bool AssertionIsAnError::value()
{
    return mValue;
}


AssertionIsAnError::NotAnErrorGuard::NotAnErrorGuard()
  : mSavedValue(mValue)
{
    mValue = false;
}


AssertionIsAnError::NotAnErrorGuard::~NotAnErrorGuard()
{
    mValue = mSavedValue;
}


FailureAssertion::FailureAssertion(const Location& location)
  : Assertion(location, "", LogContext::Log)
{
}


//NOLINTNEXTLINE(bugprone-exception-escape)
FailureAssertion::~FailureAssertion() noexcept(false)
{
    finish();

    // This explicit throw is not necessary and should never be executed. It exists only
    // to avoid a compiler warning about this destructor being declared [[noreturn]] but still
    // returning.
    throw std::runtime_error("");
}


AssertionThrower::AssertionThrower(std::function<void(const std::string&)>&& thrower)
  : mThrower(std::move(thrower))
{
}


void AssertionThrower::deploy(IExceptionTypeHandler& handler) const
{
    handler.setThrower(std::move(mThrower));
}


Detail::Detail(const std::string& text)
  : text(text)
{
}


Detail::Detail(std::string&& text)
  : text(std::move(text))
{
}


std::ostream& operator<<(std::ostream& os, const Detail& detail)
{
    auto* provider = dynamic_cast<IDetailStreamProvider*>(Log::LogStream::getUserData(os));
    if (provider != nullptr)
        provider->getDetailStream() << detail.text;
    return os;
}


namespace assertion
{
    Detail detail(const std::string& text)
    {
        return Detail(text);
    }

    Detail detail(std::string&& text)
    {
        return Detail(std::move(text));
    }
}

} // namespace epa
