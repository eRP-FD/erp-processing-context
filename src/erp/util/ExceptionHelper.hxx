/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_EXCEPTIONHANDLER_HXX
#define ERP_PROCESSING_CONTEXT_EXCEPTIONHANDLER_HXX

#include "erp/util/ExceptionWrapper.hxx"

#include <functional>


/**
 * This class is meant as a generic way to extract information from an in-flight exception.
 * The goal is to make catch statements simpler and avoid listing each exception type explicitly.
 */
class ExceptionHelper
{
public:
    /**
     * Use this variant from inside a catch(SomeExceptionType&) or catch(...) block.
     */
    [[noreturn]]
    static void extractInformationAndRethrow (
        std::function<void(std::string&& details, std::string&& location)>&& consumer);

    /**
     * Use this variant if you have to call st::current_exception() anyway and already have the exception_ptr at hand.
     */
    [[noreturn]]
    static void extractInformationAndRethrow (
        std::function<void(std::string&& details, std::string&& location)>&& consumer,
        std::exception_ptr exception);

    static void extractInformation (
        std::function<void(std::string&& details, std::string&& location)>&& consumer,
        std::exception_ptr exception);
};


#endif
