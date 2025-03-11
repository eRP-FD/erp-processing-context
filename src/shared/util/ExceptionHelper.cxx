/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/util/ExceptionHelper.hxx"
#include "shared/network/message/HttpStatus.hxx"
#include "shared/hsm/HsmException.hxx"
#include "shared/util/Demangle.hxx"
#include "shared/util/ErpException.hxx"
#include "shared/util/JwtException.hxx"

#include <sw/redis++/redis++.h>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>
#include <sstream>

#if WITH_HSM_TPM_PRODUCTION > 0
#include "shared/hsm/production/HsmProductionClient.hxx"
#endif

namespace
{
    template<class ExceptionType>
    std::string getLocationString (const ExceptionType& exception)
    {
        auto optionalLocation = ExceptionWrapper<ExceptionType>::getLocation(exception);
        std::ostringstream location;
        if (optionalLocation.has_value())
            location << optionalLocation->location;
        else
            location << "unknown location";
        return location.str();
    }
}

void ExceptionHelper::extractInformationAndRethrow (
    std::function<void(std::string&& details, std::string&& location)>&& consumer)
{
    extractInformationAndRethrow(std::move(consumer), std::current_exception());
}


void ExceptionHelper::extractInformationAndRethrow (
    std::function<void(std::string&& details, std::string&& location)>&& consumer,
    std::exception_ptr exception)
{
    try
    {
        std::rethrow_exception(std::move(exception));
    }
    catch (const ErpException& e)
    {
        std::ostringstream detail;
        detail << "ErpException " << e.status();
        if (e.status() == HttpStatus::BadRequest)
            detail << ": " << e.what();

        std::move(consumer)(detail.str(), getLocationString(e));
        throw;
    }
    catch (const JwtInvalidRfcFormatException& e)
    {
        std::move(consumer)("JwtInvalidRfcFormatException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const JwtExpiredException& e)
    {
        std::move(consumer)("JwtExpiredException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const JwtInvalidFormatException& e)
    {
        std::move(consumer)("JwtInvalidFormatException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const JwtInvalidSignatureException& e)
    {
        std::move(consumer)("JwtInvalidSignatureException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const JwtRequiredClaimException& e)
    {
        std::move(consumer)("JwtRequiredClaimException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const JwtInvalidAudClaimException& e)
    {
        std::move(consumer)("JwtInvalidAudClaimException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const JwtException& e)
    {
        std::move(consumer)("JwtException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const HsmException& e)
    {
        std::move(consumer)("HsmException(" + std::string(e.what())
#if WITH_HSM_TPM_PRODUCTION > 0
                     + "," + HsmProductionClient::hsmErrorDetails(e.errorCode)
#endif
                     + ")", getLocationString(e));
        throw;
    }
    catch (const sw::redis::Error& e)
    {
        std::move(consumer)("sw::redis::Error(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const std::runtime_error& e)
    {
        const std::string typeinfo = util::demangle(typeid(e).name());
        std::move(consumer)("std::runtime_error(" + typeinfo + ")(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const std::logic_error& e)
    {
        const std::string typeinfo = util::demangle(typeid(e).name());
        std::move(consumer)("std::logic_error(" + typeinfo + ")(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const std::exception& e)
    {
        const std::string typeinfo = util::demangle(typeid(e).name());
        std::move(consumer)("std::exception(" + typeinfo + ")(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const boost::exception& e)
    {
        std::move(consumer)("boost::exception(" + boost::diagnostic_information(e) + ")", getLocationString(e));
        throw;
    }
    catch(...)
    {
        std::move(consumer)("unknown exception", "unknown location");
        throw;
    }
}


void ExceptionHelper::extractInformation (
    std::function<void(std::string&& details, std::string&& location)>&& consumer,
    std::exception_ptr exception)
{
    try
    {
        extractInformationAndRethrow(std::move(consumer), std::move(exception));
    }
    catch(...) //NOLINT(bugprone-empty-catch)
    {
        // Caller does not want the exception to be thrown beyond this function, so ignore it
    }
}
