/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_ERPMACROS_HXX
#define ERP_PROCESSING_CONTEXT_ERPMACROS_HXX

#include <string_view>

#include <gtest/gtest.h>

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
            EXPECT_EQ(ex.what(), std::string_view(message)) << ex.what();                          \
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


#endif//ERP_PROCESSING_CONTEXT_ERPMACROS_HXX
