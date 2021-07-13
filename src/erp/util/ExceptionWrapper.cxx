#include "erp/util/ExceptionWrapper.hxx"

#include <exception>
#include <ostream>
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

