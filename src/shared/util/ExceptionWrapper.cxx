/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/ExceptionWrapper.hxx"

#include <exception>
#include <ostream>
#include <sstream>
#include <string_view>

FileNameAndLineNumber::FileNameAndLineNumber(const char* initFileName, const size_t initLine)
    : fileName(initFileName)
    , line(initLine)
{
}

bool FileNameAndLineNumber::operator!=(const FileNameAndLineNumber& other) const
{
    return not ((*this) == other);
}

bool FileNameAndLineNumber::operator==(const FileNameAndLineNumber& other) const
{
    return (line == other.line) && (std::string_view{fileName} == other.fileName);
}

std::ostream & operator <<(std::ostream& out, const FileNameAndLineNumber& location)
{
    out << location.fileName << ':' << location.line;
    return out;
}

std::string to_string(const FileNameAndLineNumber& location)
{
    std::ostringstream strm;
    strm << location;
    return strm.str();
}

FileNameAndLineNumber ExceptionWrapperBase::getRootLocation(const FileNameAndLineNumber& location)
{
    const auto& currentEx = std::current_exception();
    if (currentEx)
    {
        try
        {
            std::rethrow_exception(currentEx);
        }
        catch (const ExceptionWrapperBase& currentWrapper)
        {
            return currentWrapper.location.rootLocation;
        }
        catch(...)
        {}
    }
    return location;
}
