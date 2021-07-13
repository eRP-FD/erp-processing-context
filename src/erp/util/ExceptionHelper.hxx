#ifndef ERP_PROCESSING_CONTEXT_EXCEPTIONHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_EXCEPTIONHANDLER_HXX

#include "erp/util/ExceptionWrapper.hxx"

#include <functional>


class ExceptionHelper
{
public:
    /**
     * This method is meant as a generic way to extract information about an in-flight exception.
     * The goal is to make catch statements simpler and avoid listing each exception type explicitly.
     */
    [[noreturn]]
    static void extractInformationAndRethrow (std::function<void(std::string&& details, std::string&& location)>&& consumer);
};


#endif
