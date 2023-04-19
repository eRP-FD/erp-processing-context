/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/util/ExceptionHelper.hxx"

#include "erp/hsm/HsmException.hxx"
#include "erp/util/ErpException.hxx"
#include "erp/util/JwtException.hxx"

#include <boost/exception/exception.hpp>
#include <gtest/gtest.h>
#include <atomic>


class ExceptionHelperTest : public testing::Test
{
};

namespace
{
    template<typename ExceptionType, typename ... ConstructorArguments>
    //NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void throwAndCatchException (
        const std::string& expectedDetails,
        ConstructorArguments&& ... constructorArguments)
    {
        std::string details;
        std::string location;
        ASSERT_THROW(
            try
            {
                throw ExceptionType(std::forward<ConstructorArguments>(constructorArguments)...); //NOLINT[hicpp-exception-baseclass]
            }
            catch(...)
            {
                ExceptionHelper::extractInformationAndRethrow(
                    [&]
                    (const std::string& details_, const std::string& location_)
                    {
                        // Store the extracted information for a later test.
                        details = details_;
                        location = location_;
                    });
            },
            ExceptionType);

        ASSERT_EQ(details, expectedDetails);
        ASSERT_FALSE(location.empty());
    }
}


TEST_F(ExceptionHelperTest, ErpException)
{
    throwAndCatchException<ErpException>(
        "ErpException BadRequest: details",
        "details",
        HttpStatus::BadRequest);
}


TEST_F(ExceptionHelperTest, JwtInvalidRfcFormatException)
{
    throwAndCatchException<JwtInvalidRfcFormatException>(
        "JwtInvalidRfcFormatException(details)",
        "details");
}


TEST_F(ExceptionHelperTest, JwtExpiredException)
{
    throwAndCatchException<JwtExpiredException>(
        "JwtExpiredException(details)",
        "details");
}


TEST_F(ExceptionHelperTest, JwtInvalidFormatException)
{
    throwAndCatchException<JwtInvalidFormatException>(
        "JwtInvalidFormatException(details)",
        "details");
}


TEST_F(ExceptionHelperTest, JwtInvalidSignatureException)
{
    throwAndCatchException<JwtInvalidSignatureException>(
        "JwtInvalidSignatureException(details)",
        "details");
}


TEST_F(ExceptionHelperTest, JwtRequiredClaimException)
{
    throwAndCatchException<JwtRequiredClaimException>(
        "JwtRequiredClaimException(details)",
        "details");
}


TEST_F(ExceptionHelperTest, JwtInvalidAudClaimException)
{
    throwAndCatchException<JwtInvalidAudClaimException>(
        "JwtInvalidAudClaimException(details)",
        "details");
}


TEST_F(ExceptionHelperTest, JwtException)
{
    throwAndCatchException<JwtException>(
        "JwtException(details)",
        "details");
}


TEST_F(ExceptionHelperTest, HsmException)
{
#if WITH_HSM_TPM_PRODUCTION != 1
    GTEST_SKIP_("skipped due to WITH_HSM_TPM_PRODUCTION != 1");
#endif
    throwAndCatchException<HsmException>(
        "HsmException(details,42420001 ERP_ERR_NO_CONNECTION without index)",
        "details",
        0x42420001u);
}


TEST_F(ExceptionHelperTest, std_runtime_error)
{
    throwAndCatchException<std::runtime_error>(
        "std::runtime_error(St13runtime_error)(detail)",
        "detail");
}


TEST_F(ExceptionHelperTest, std_logic_error)
{
    throwAndCatchException<std::logic_error>(
        "std::logic_error(St11logic_error)(detail)",
        "detail");
}


TEST_F(ExceptionHelperTest, std_exception)
{
    throwAndCatchException<std::exception>(
        "std::exception(St9exception)(std::exception)");
}


class NotDerivedFromException
{
};

TEST_F(ExceptionHelperTest, other)
{
    throwAndCatchException<NotDerivedFromException>(
        "unknown exception");
}
