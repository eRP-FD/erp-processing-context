/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/log/ClassifiedText.hxx"
#include "library/log/Logging.hxx"

#include "shared/util/String.hxx"

namespace epa
{

namespace
{
    /**
     * This small function makes the decision whether a given text is displayed as-is in a log
     * message or is substituted with with a replacement text.
     */
    bool canShowText(const ClassifiedText::Visibility visibility, const LogContext context)
    {
#ifdef ENABLE_DEBUG_LOG
        (void) visibility;
        (void) context;
        // No restrictions in debug builds.
        return true;
#else
        return (visibility == ClassifiedText::Visibility::ShowInLog && context == LogContext::Log)
               || (visibility == ClassifiedText::Visibility::ShowInResponse
                   && context == LogContext::Response)
               || (visibility == ClassifiedText::Visibility::ShowAlways);
#endif
    }
}


ClassifiedText::ClassifiedText(
    std::string&& text,
    std::string&& replacement,
    const Visibility visibility)
  : mText(std::move(text)),
    mReplacement(std::move(replacement)),
    mVisibility(visibility)
{
    String::safeClear(text);
    String::safeClear(replacement);
}


ClassifiedText::ClassifiedText(const ClassifiedText& other)
  : boost::noncopyable(),
    mText(other.mText),
    mReplacement(other.mReplacement),
    mVisibility(other.mVisibility)
{
}


ClassifiedText::~ClassifiedText()
{
    //NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    String::safeClear(const_cast<std::string&>(mText));
    // safeClear is not necessary for mReplacement, which by definition must not expose
    // personal, sensitive or confidential data.
}


const std::string& ClassifiedText::toString(LogContext context) const
{
    if (canShowText(mVisibility, context))
        return mText;
    else
        return mReplacement;
}


std::ostream& operator<<(std::ostream& os, const ClassifiedText& classifiedText)
{
    // This is the main reason for the existence of class `ClassifiedText` rather than
    // using for example a function `std::string logging::personal(std::string)` that decides
    // on the spot whether the print the original text or use the replacement:
    // This decision is based on information that is only available when you have access to
    // the user data of the `LogStream` that is used as `std::ostream` in `operator<<`.
    auto* logStream = dynamic_cast<Log::LogStream*>(&os);
    if (logStream != nullptr)
        logStream->addClassifiedText(classifiedText);
    return os;
}

} // namespace epa
