/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/ErpProcessingContext.hxx"
#include "erp/server/handler/RequestHandlerInterface.hxx"
#include "erp/ErpRequirements.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

#include <gtest/gtest.h>

#ifdef _WINNT_
#ifdef DELETE
#undef DELETE
#endif
#endif

class ErpProcessingContextTest: public testing::Test
{
public:
    ErpProcessingContextTest()
        : mRequestHandlerManager(),
          mAllProfessionOIDs()
    {
        EnvironmentVariableGuard enablePkv{"ERP_FEATURE_PKV", "true"};

        ErpProcessingContext::addSecondaryEndpoints(mRequestHandlerManager);
        ErpProcessingContext::addPrimaryEndpoints(mRequestHandlerManager);
        std::string prefix = "1.2.276.0.76.4.";
        for (int i = 0; i != 1000; ++i)
        {
            mAllProfessionOIDs.insert(prefix + std::to_string(i));
        }

        // add some random values:
        mAllProfessionOIDs.insert("");
        mAllProfessionOIDs.insert("0");
        mAllProfessionOIDs.insert("0.0.0.0.0.0");
        mAllProfessionOIDs.insert("hallo welt");
        mAllProfessionOIDs.insert({});
    }

    void checkAllOids (const HttpMethod method, const std::string& target, const std::set<std::string>& allowedOIDs)
    {
        auto matchingHandler = mRequestHandlerManager.findMatchingHandler(method, target);
        ASSERT_TRUE(matchingHandler.handlerContext);
        ASSERT_TRUE(matchingHandler.handlerContext->handler);

        for (const auto& oid : mAllProfessionOIDs)
        {
            if (allowedOIDs.count(oid) > 0)
            {
                ASSERT_TRUE(matchingHandler.handlerContext->handler->allowedForProfessionOID(oid)) << "This OID is falsely not allowed: " << oid;
            }
            else
            {
                ASSERT_FALSE(matchingHandler.handlerContext->handler->allowedForProfessionOID(oid)) << "This OID is falsely allowed: " << oid;
            }
        }
    }

    RequestHandlerManager<PcServiceContext> mRequestHandlerManager;
    std::set<std::string> mAllProfessionOIDs;
};


TEST_F(ErpProcessingContextTest, GetAllTasks_ProfessionOIDs)
{
    A_21558.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::GET, "/Task", {
            "1.2.276.0.76.4.49", // oid_versicherter
    });
}

TEST_F(ErpProcessingContextTest, GetTask_ProfessionOIDs)
{
    A_19113_01.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::GET, "/Task/{id}", {
            "1.2.276.0.76.4.49", // oid_versicherter
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55" // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, PostTaskCreate_ProfessionOIDs)
{
    A_19018.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::POST, "/Task/$create", {
            "1.2.276.0.76.4.30", // oid_arzt
            "1.2.276.0.76.4.31", // oid_zahnarzt
            "1.2.276.0.76.4.50", // oid_praxis_arzt
            "1.2.276.0.76.4.51", // oid_zahnarztpraxis
            "1.2.276.0.76.4.52", // oid_praxis_psychotherapeut
            "1.2.276.0.76.4.53", // oid_krankenhaus
    });
}

TEST_F(ErpProcessingContextTest, PostTaskActivate_ProfessionOIDs)
{
    A_19022.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::POST, "/Task/{id}/$activate", {
            "1.2.276.0.76.4.30", // oid_arzt
            "1.2.276.0.76.4.31", // oid_zahnarzt
            "1.2.276.0.76.4.50", // oid_praxis_arzt
            "1.2.276.0.76.4.51", // oid_zahnarztpraxis
            "1.2.276.0.76.4.52", // oid_praxis_psychotherapeut
            "1.2.276.0.76.4.53", // oid_krankenhaus
    });
}

TEST_F(ErpProcessingContextTest, PostTaskAccept_ProfessionOIDs)
{
    A_19166.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::POST, "/Task/{id}/$accept", {
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55", // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, PostTaskReject_ProfessionOIDs)
{
    A_19170_01.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::POST, "/Task/{id}/$reject", {
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55", // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, PostTaskClose_ProfessionOIDs)
{
    A_19230.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::POST, "/Task/{id}/$close", {
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55", // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, PostTaskAbort_ProfessionOIDs)
{
    A_19026.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::POST, "/Task/{id}/$abort", {
            "1.2.276.0.76.4.49", // oid_versicherter
            "1.2.276.0.76.4.30", // oid_arzt
            "1.2.276.0.76.4.31", // oid_zahnarzt
            "1.2.276.0.76.4.50", // oid_praxis_arzt
            "1.2.276.0.76.4.51", // oid_zahnarztpraxis
            "1.2.276.0.76.4.52", // oid_praxis_psychotherapeut
            "1.2.276.0.76.4.53", // oid_krankenhaus
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55", // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, GetAllMedicationDispenses_ProfessionOIDs)
{
    A_19405.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::GET, "/MedicationDispense", {
            "1.2.276.0.76.4.49", // oid_versicherter
    });
}

TEST_F(ErpProcessingContextTest, GetMedicationDispense_ProfessionOIDs)
{
    A_19405.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::GET, "/MedicationDispense/{id}", {
            "1.2.276.0.76.4.49", // oid_versicherter
    });
}

TEST_F(ErpProcessingContextTest, GetAllCommunications_ProfessionOIDs)
{
    A_19446_01.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::GET, "/Communication", {
            "1.2.276.0.76.4.49", // oid_versicherter
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55", // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, GetCommunication_ProfessionOIDs)
{
    A_19446_01.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::GET, "/Communication/{id}", {
            "1.2.276.0.76.4.49", // oid_versicherter
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55", // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, PostCommunication_ProfessionOIDs)
{
    A_19446_01.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::POST, "/Communication", {
            "1.2.276.0.76.4.49", // oid_versicherter
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55", // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, DeleteCommunication_ProfessionOIDs)
{
    A_19446_01.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::DELETE, "/Communication/{id}", {
            "1.2.276.0.76.4.49", // oid_versicherter
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55", // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, GetAllAuditEvents_ProfessionOIDs)
{
    A_19395.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::GET, "/AuditEvent", {
            "1.2.276.0.76.4.49", // oid_versicherter
    });
}

TEST_F(ErpProcessingContextTest, GetAuditEvent_ProfessionOIDs)
{
    A_19395.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::GET, "/AuditEvent/{id}", {
            "1.2.276.0.76.4.49", // oid_versicherter
    });
}


TEST_F(ErpProcessingContextTest, DeleteChargeItem_ProfessionOIDs)
{
    A_22113.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::DELETE, "/ChargeItem/{id}", {
            "1.2.276.0.76.4.49", // oid_versicherter
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55", // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, GetAllChargeItems_ProfessionOIDs)
{
    A_22118.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::GET, "/ChargeItem", {
            "1.2.276.0.76.4.49", // oid_versicherter
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55", // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, GetChargeItem_ProfessionOIDs)
{
    A_22124.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::GET, "/ChargeItem/{id}", {
            "1.2.276.0.76.4.49", // oid_versicherter
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55", // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, PostChargeItem_ProfessionOIDs)
{
    A_22129.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::POST, "/ChargeItem", {
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55", // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, PutChargeItem_ProfessionOIDs)
{
    A_22144.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::PUT, "/ChargeItem/{id}", {
            "1.2.276.0.76.4.49", // oid_versicherter
            "1.2.276.0.76.4.54", // oid_oeffentliche_apotheke
            "1.2.276.0.76.4.55", // oid_krankenhausapotheke
    });
}

TEST_F(ErpProcessingContextTest, DeleteConsent_ProfessionOIDs)
{
    A_22155.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::DELETE, "/Consent/{id}", {
            "1.2.276.0.76.4.49", // oid_versicherter
    });
}

TEST_F(ErpProcessingContextTest, GetConsent_ProfessionOIDs)
{
    A_22159.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::GET, "/Consent", {
            "1.2.276.0.76.4.49", // oid_versicherter
    });
}

TEST_F(ErpProcessingContextTest, PostConsent_ProfessionOIDs)
{
    A_22161.test("Unit test of allowedForProfessionOID() function");
    checkAllOids(HttpMethod::POST, "/Consent", {
            "1.2.276.0.76.4.49", // oid_versicherter
    });
}


TEST_F(ErpProcessingContextTest, GetDevice_ProfessionOIDs)
{
    checkAllOids(HttpMethod::GET, "/Device",
            mAllProfessionOIDs // unconstrained
    );
}

TEST_F(ErpProcessingContextTest, GetMetadata_ProfessionOIDs)
{
    checkAllOids(HttpMethod::GET, "/metadata",
            mAllProfessionOIDs // unconstrained
    );
}

TEST_F(ErpProcessingContextTest, PostVAU_ProfessionOIDs)
{
    checkAllOids(HttpMethod::POST, "/VAU/{UP}",
            mAllProfessionOIDs // unconstrained
    );
}
