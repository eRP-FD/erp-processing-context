/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/ExceptionHelper.hxx"
#include "erp/common/HttpStatus.hxx"
#include "erp/hsm/HsmException.hxx"
#include "erp/util/ErpException.hxx"
#include "erp/util/JwtException.hxx"

#include <sw/redis++/redis++.h>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/exception/exception.hpp>
#include <sstream>

#if WITH_HSM_TPM_PRODUCTION > 0
#include "erp/hsm/production/HsmProductionClient.hxx"
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

        consumer(detail.str(), getLocationString(e));
        throw;
    }
    catch (const JwtInvalidRfcFormatException& e)
    {
        consumer("JwtInvalidRfcFormatException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const JwtExpiredException& e)
    {
        consumer("JwtExpiredException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const JwtInvalidFormatException& e)
    {
        consumer("JwtInvalidFormatException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const JwtInvalidSignatureException& e)
    {
        consumer("JwtInvalidSignatureException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const JwtRequiredClaimException& e)
    {
        consumer("JwtRequiredClaimException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const JwtInvalidAudClaimException& e)
    {
        consumer("JwtInvalidAudClaimException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const JwtException& e)
    {
        consumer("JwtException(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const HsmException& e)
    {
        consumer("HsmException(" + std::string(e.what())
#if WITH_HSM_TPM_PRODUCTION > 0
                     + "," + HsmProductionClient::hsmErrorDetails(e.errorCode)
#endif
                     + ")", getLocationString(e));
        throw;
    }
    catch (const sw::redis::Error& e)
    {
        consumer("sw::redis::Error(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const std::runtime_error& e)
    {
        consumer("std::runtime_error(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const std::logic_error& e)
    {
        consumer("std::logic_error(" + std::string(e.what()) + ")", getLocationString(e));
        throw;
    }
    catch (const std::exception& e)
    {
        consumer("std::exception()", getLocationString(e));
        throw;
    }
    catch (const boost::exception& e)
    {
        consumer("boost::exception(" + boost::diagnostic_information(e) + ")", getLocationString(e));
        throw;
    }
    catch(...)
    {
        consumer("unknown exception", "unknown location");
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
    catch(...)
    {
        // Caller does not want the exception to be thrown beyond this function, so ignore it
    }
}
