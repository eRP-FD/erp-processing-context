/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_SERVICE_ENDPOINTHANDLERTEST_ENDPOINTHANDLERTESTFIXTURE_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_SERVICE_ENDPOINTHANDLERTEST_ENDPOINTHANDLERTESTFIXTURE_HXX

#include "erp/model/Task.hxx"
#include "erp/pc/PcServiceContext.hxx"
#include "shared/util/String.hxx"

#include <gtest/gtest.h>
#include <regex>
#include <variant>

class DatabaseFrontend;
class JwtBuilder;
class MockDatabase;


class EndpointHandlerTest : public testing::Test
{
public:
    EndpointHandlerTest();
    ~EndpointHandlerTest() override;

    model::Task addTaskToDatabase(
        SessionContext& sessionContext,
        model::Task::Status taskStatus,
        const std::string& accessCode,
        const std::string& insurant)
    {
        return addTaskToDatabase(sessionContext, taskStatus, accessCode, insurant, model::Timestamp{.0});
    }

    model::Task addTaskToDatabase(
        SessionContext& sessionContext,
        model::Task::Status taskStatus,
        const std::string& accessCode,
        const std::string& insurant,
        const model::Timestamp lastUpdate);

    std::optional<model::UnspecifiedResource> getMedicationDispenses(const std::string& kvnr,
                                                                    const model::PrescriptionId& prescriptionId);

    void checkGetAllAuditEvents(const std::string& kvnr, const std::string& expectedResultFilename);

    std::string replacePrescriptionId(const std::string& templateStr, const std::string& prescriptionId)
    {
        return String::replaceAll(templateStr, "##PRESCRIPTION_ID##", prescriptionId);
    }
    std::string replaceKvnr(const std::string& templateStr, const std::string& kvnr)
    {
        return String::replaceAll(templateStr, "##KVNR##", kvnr);
    }

    // GEMREQ-start callHandlerWithResponseStatusCheck
    template<class HandlerType>
    static void callHandlerWithResponseStatusCheck(PcSessionContext& sessionContext, HandlerType& handler,
                                            const HttpStatus expectedStatus,
                                            const std::optional<std::string_view> expectedExcWhat = std::nullopt)
    {
        auto status = HttpStatus::Unknown;
        try
        {
            handler.preHandleRequestHook(sessionContext);
            handler.handleRequest(sessionContext);
            status = sessionContext.response.getHeader().status();
        }
        catch (const ErpException& exc)
        {
            status = exc.status();
            if (expectedExcWhat)
            {
                ASSERT_EQ(exc.what(), *expectedExcWhat) << exc.what();
            }
        }
        catch (const std::exception& exc)
        {
            FAIL() << "Unexpected exception " << exc.what();
        }
        ASSERT_EQ(status, expectedStatus);
    }
    // GEMREQ-end callHandlerWithResponseStatusCheck



protected:
    std::unique_ptr<MockDatabase> mockDatabase;
    std::string dataPath;
    PcServiceContext mServiceContext;
    std::unique_ptr<JwtBuilder> mJwtBuilder;
    std::unique_ptr<DatabaseFrontend> createDatabase(HsmPool& hsmPool, KeyDerivation& keyDerivation);
};



#endif//ERP_PROCESSING_CONTEXT_TEST_ERP_SERVICE_ENDPOINTHANDLERTEST_ENDPOINTHANDLERTESTFIXTURE_HXX
