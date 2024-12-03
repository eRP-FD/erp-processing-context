/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#ifndef EPA_LIBRARY_LOG_CLASSIFIEDTEXT_HXX
#define EPA_LIBRARY_LOG_CLASSIFIEDTEXT_HXX

#include "library/log/LogContext.hxx"

#include <boost/core/noncopyable.hpp>
#include <string>

namespace epa
{

/**
 * Small class that handles text replacement of log message parts that are not allowed
 * to be printed in a given environment. I.e. no personal information in log files.
 */
class ClassifiedText : private boost::noncopyable
{
public:
    enum class Visibility
    {
        ShowInLog,
        ShowInResponse,
        ShowInDevelopmentBuild,
        ShowAlways
    };

    /**
     * Factory method that accepts different types of values which are transformed into text before the
     * ClassifiedText object is returned.
     * Currently supported:
     * - integral values
     * - string-like types, i.e. anything that is accepted by a std::string constructor.
     */
    template<typename T>
    static ClassifiedText create(const T& value, std::string&& replacement, Visibility visibility);

    ClassifiedText(std::string&& text, std::string&& replacement, Visibility visibility);
    ClassifiedText(const ClassifiedText& other);
    ~ClassifiedText();

    const std::string& toString(LogContext context) const;

private:
    const std::string mText;
    const std::string mReplacement;
    const Visibility mVisibility;
};


std::ostream& operator<<(std::ostream& os, const ClassifiedText& classifiedText);


template<typename T>
ClassifiedText ClassifiedText::create(
    const T& value,
    std::string&& replacement,
    Visibility visibility)
{
    if constexpr (std::is_integral_v<std::decay_t<T>>)
        return ClassifiedText(std::to_string(value), std::move(replacement), visibility);
    else
        return ClassifiedText(std::string(value), std::move(replacement), visibility);
}

} // namespace epa

#endif
