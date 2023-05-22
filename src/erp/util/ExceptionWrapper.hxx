/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_EXCEPTIONWRAPPER_HXX
#define ERP_PROCESSING_CONTEXT_EXCEPTIONWRAPPER_HXX

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <utility>


class FileNameAndLineNumber
{
public:
    const char* fileName;
    const size_t line;

    FileNameAndLineNumber(const char* fileName, const size_t line);
    bool operator == (const FileNameAndLineNumber& other) const;
    bool operator != (const FileNameAndLineNumber& other) const;
};

struct Location
{
    const FileNameAndLineNumber location;
    const FileNameAndLineNumber rootLocation;
};

std::ostream& operator << (std::ostream& out, const FileNameAndLineNumber& location);
std::string to_string(const FileNameAndLineNumber& location);

class ExceptionWrapperBase
{
public:
    explicit ExceptionWrapperBase(const FileNameAndLineNumber& initRootLocation, const FileNameAndLineNumber initLocation)
        : location{initLocation, initRootLocation}
    {}
    virtual ~ExceptionWrapperBase() = default;

    template<typename AnyExceptionT>
    static std::optional<Location> getLocation (const AnyExceptionT& ex)
    {
        const auto* wrapper = dynamic_cast<const ExceptionWrapperBase*>(&ex);
        if (wrapper != nullptr)
            return wrapper->location;
        else
            return std::nullopt;
    }

    const Location location;

protected:
    static FileNameAndLineNumber getRootLocation(const FileNameAndLineNumber& location);
};

template <typename ExceptionT>
class ExceptionWrapper
    : public ExceptionT
    , public ExceptionWrapperBase
{
public:
    template<typename... Arguments>
    static ExceptionWrapper<ExceptionT> create(FileNameAndLineNumber fileNameAndLineNumber, Arguments&& ... arguments)
    {
        return ExceptionWrapper<ExceptionT>(getRootLocation(fileNameAndLineNumber), fileNameAndLineNumber,
                                            std::forward<Arguments>(arguments)...);
    }


private:
    template <typename... ExceptionArgs>
    explicit ExceptionWrapper(const FileNameAndLineNumber& initRootLocation, const FileNameAndLineNumber& initLocation,
                              ExceptionArgs&&... args)
        : ExceptionT(std::forward<ExceptionArgs>(args)...)
        , ExceptionWrapperBase(initRootLocation, initLocation)
    {}

};

#endif // ERP_PROCESSING_CONTEXT_EXCEPTIONWRAPPER_HXX
