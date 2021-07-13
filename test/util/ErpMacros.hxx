#ifndef ERP_PROCESSING_CONTEXT_ERPMACROS_HXX
#define ERP_PROCESSING_CONTEXT_ERPMACROS_HXX

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

#endif//ERP_PROCESSING_CONTEXT_ERPMACROS_HXX
