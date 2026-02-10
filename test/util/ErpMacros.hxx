/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPMACROS_HXX
#define ERP_PROCESSING_CONTEXT_ERPMACROS_HXX

#include "shared/util/Expect.hxx"
#include "shared/util/String.hxx"

#include <gtest/gtest.h>
#include <set>
#include <string_view>
#include <vector>

#define EXPECT_ERP_EXCEPTION(expression, httpStatus)                                               \
    {                                                                                              \
        try                                                                                        \
        {                                                                                          \
            static_cast<void>(expression);                                                         \
            FAIL() << "Expected ErpException did not occur";                                       \
        }                                                                                          \
        catch (const ErpException& ex)                                                             \
        {                                                                                          \
            EXPECT_EQ(ex.status(), httpStatus);                                                    \
        }                                                                                          \
        catch (...)                                                                                \
        {                                                                                          \
            FAIL() << "Expected ErpException, but wrong exception type was thrown";                \
        }                                                                                          \
    }

#define EXPECT_ERP_EXCEPTION_WITH_MESSAGE(expression, httpStatus, message)                         \
    {                                                                                              \
        try                                                                                        \
        {                                                                                          \
            static_cast<void>(expression);                                                         \
            FAIL() << "Expected ErpException did not occur";                                       \
        }                                                                                          \
        catch (const ErpException& ex)                                                             \
        {                                                                                          \
            EXPECT_EQ(ex.status(), httpStatus);                                                    \
            EXPECT_STREQ(ex.what(), (message)) << ex.what();                          \
        }                                                                                          \
        catch (...)                                                                                \
        {                                                                                          \
            FAIL() << "Expected ErpException, but wrong exception type was thrown";                \
        }                                                                                          \
    }

#define EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(expression, httpStatus, message, diag)        \
    {                                                                                              \
        try                                                                                        \
        {                                                                                          \
            static_cast<void>(expression);                                                         \
            FAIL() << "Expected ErpException did not occur";                                       \
        }                                                                                          \
        catch (const ErpException& ex)                                                             \
        {                                                                                          \
            EXPECT_EQ(ex.status(), httpStatus);                                                    \
            EXPECT_EQ(ex.what(), std::string_view(message)) << ex.what();                          \
            EXPECT_EQ(ex.diagnostics(), std::string(diag)) << ex.diagnostics().value_or("<unset>"); \
        }                                                                                          \
        catch (...)                                                                                \
        {                                                                                          \
            FAIL() << "Expected ErpException, but wrong exception type was thrown";                \
        }                                                                                          \
    }

inline std::set<std::string> stripProfiles(std::vector<std::string>&& diagnostics)
{
    std::set<std::string> ret;
    for (auto&& diagnostic : std::move(diagnostics))
    {
        ret.insert(diagnostic.substr(0, diagnostic.find_first_of("(from profile:")));
    }
    return ret;
}

inline void expectErpExceptionWithFHIRValidationError(const ErpException& ex, const std::string& message,
                                                      const std::string& diag)
{
    EXPECT_EQ(ex.status(), HttpStatus::BadRequest);
    EXPECT_EQ(ex.what(), std::string_view{(message)}) << ex.what();
    ASSERT_TRUE(ex.diagnostics().has_value());
    std::set<std::string> diagnosticsMessagesSet = stripProfiles(String::split(ex.diagnostics().value(), "; "));
    std::set<std::string> fullDiagnosticsMessagesSet = stripProfiles(String::split((diag), "; "));
    EXPECT_EQ(diagnosticsMessagesSet, fullDiagnosticsMessagesSet) << std::endl << ex.diagnostics().value();
}

#define EXPECT_ERP_EXCEPTION_WITH_MESSAGE_AND_FHIR_VALIDATION_ERROR(expression, message, diag)        \
    {                                                                                              \
        try                                                                                        \
        {                                                                                          \
            static_cast<void>(expression);                                                         \
            FAIL() << "Expected ErpException did not occur";                                       \
        }                                                                                          \
        catch (const ErpException& ex)                                                             \
        {                                                                                          \
            expectErpExceptionWithFHIRValidationError(ex, message, diag);                           \
        }                                                                                          \
        catch (...)                                                                                \
        {                                                                                          \
            FAIL() << "Expected ErpException, but wrong exception type was thrown";                \
        }                                                                                          \
    }

#define EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(expression, diag) \
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE_AND_FHIR_VALIDATION_ERROR(expression, "FHIR-Validation error", diag)

#define ASSERT_NO_ERP_EXCEPTION(expression)                                                                            \
    {                                                                                                                  \
        try                                                                                                            \
        {                                                                                                              \
            static_cast<void>(expression);                                                                             \
        }                                                                                                              \
        catch (const ErpException& ex)                                                                                 \
        {                                                                                                              \
            FAIL() << "ErpException what: " << ex.what() << " diagnostics: " << ex.diagnostics().value_or("none");     \
            throw;                                                                                                     \
        }                                                                                                              \
        catch (...)                                                                                                    \
        {                                                                                                              \
            ASSERT_NO_FATAL_FAILURE(throw);                                                                            \
        }                                                                                                              \
    }

namespace erp::test
{
inline auto softExpectTrue(const bool result, const bool allowFailure, std::string_view msg,
                           const std::source_location loc)
{
    if (! result)
    {
        if (! allowFailure)
        {
            EXPECT_TRUE(result) << msg << " at " << loc.file_name() << ":" << loc.line();
        }
        else
        {
            LOG(WARNING) << "failed, retrying: " << msg << " at " << loc.file_name() << ":" << loc.line();
        }
        return false;
    }
    return true;
};
}
#define SOFT_EXPECT_TRUE(expression, allowFailure, hadError)                                                           \
    hadError =                                                                                                         \
        ! erp::test::softExpectTrue(expression, allowFailure, #expression, std::source_location::current()) || hadError

#endif//ERP_PROCESSING_CONTEXT_ERPMACROS_HXX
