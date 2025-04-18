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

#define EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(expression, diag)        \
    {                                                                                              \
        try                                                                                        \
        {                                                                                          \
            static_cast<void>(expression);                                                         \
            FAIL() << "Expected ErpException did not occur";                                       \
        }                                                                                          \
        catch (const ErpException& ex)                                                             \
        {                                                                                          \
            EXPECT_EQ(ex.status(), HttpStatus::BadRequest);                                        \
            EXPECT_EQ(ex.what(), std::string_view{"FHIR-Validation error"}) << ex.what();          \
            ASSERT_TRUE(ex.diagnostics().has_value());                                             \
            std::vector<std::string> diagnosticsMessages = String::split(ex.diagnostics().value(), "; "); \
            std::vector<std::string> fullDiagnosticsMessages = String::split(diag, "; ");          \
            std::set<std::string> diagnosticsMessagesSet(diagnosticsMessages.begin(), diagnosticsMessages.end()); \
            std::set<std::string> fullDiagnosticsMessagesSet(fullDiagnosticsMessages.begin(), fullDiagnosticsMessages.end()); \
            EXPECT_EQ(diagnosticsMessagesSet, fullDiagnosticsMessagesSet);                         \
        }                                                                                          \
        catch (...)                                                                                \
        {                                                                                          \
            FAIL() << "Expected ErpException, but wrong exception type was thrown";                \
        }                                                                                          \
    }

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


#endif//ERP_PROCESSING_CONTEXT_ERPMACROS_HXX
