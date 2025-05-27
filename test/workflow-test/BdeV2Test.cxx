/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "test/util/ResourceTemplates.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

class BdeV2Test : public ErpWorkflowTest
{
};

TEST_F(BdeV2Test, InvalidLanr)
{
    auto task = taskCreate();
    ASSERT_TRUE(task);
    const auto signingTime = model::Timestamp::now();
    model::Lanr defectLanr{"844444400", model::Lanr::Type::lanr};
    ASSERT_FALSE(defectLanr.validChecksum());
    auto kbvBundle =
        kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = signingTime, .lanr = defectLanr});
    mActivateTaskRequestArgs.expectedLANR = defectLanr.id();
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), task->accessCode(),
                                         toCadesBesSignature(kbvBundle, signingTime), HttpStatus::OkWarnInvalidAnr));
}

TEST_F(BdeV2Test, InvalidZanr)
{
    auto task = taskCreate();
    ASSERT_TRUE(task);
    const auto signingTime = model::Timestamp::now();
    model::Lanr defectLanr{"123456789", model::Lanr::Type::zanr};
    ASSERT_FALSE(defectLanr.validChecksum());
    auto kbvBundle =
        kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = signingTime, .lanr = defectLanr});
    mActivateTaskRequestArgs.expectedZANR = defectLanr.id();
    ASSERT_NO_FATAL_FAILURE(taskActivate(task->prescriptionId(), task->accessCode(),
                                         toCadesBesSignature(kbvBundle, signingTime), HttpStatus::OkWarnInvalidAnr));
}

TEST_F(BdeV2Test, MalitiousProfileVersion)
{
    using namespace std::string_view_literals;
    using model::Task;
    using model::Timestamp;
    std::string create = "<Parameters xmlns=\"http://hl7.org/fhir\">\n"
                         "  <meta>\n"
                         "    <profile "
                         "value=\"https://gematik.de/fhir/erp/StructureDefinition/"
                         "GEM_ERP_PR_PAR_CreateOperation_Input|1.4&quot;&lt;script&gt;\"/>\n"
                         "  </meta>\n"
                         "  <parameter>\n"
                         "    <name value=\"workflowType\"/>\n"
                         "    <valueCoding>\n"
                         "      <system value=\"" +
                         std::string{model::resource::code_system::flowType} +
                         "\"/>\n"
                         "      <code value=\"160\"/>\n"
                         "    </valueCoding>\n"
                         "  </parameter>\n"
                         "</Parameters>\n";

    ClientResponse serverResponse;
    ClientResponse outerResponse;

    JWT jwt{jwtArzt()};
    ASSERT_NO_FATAL_FAILURE(std::tie(outerResponse, serverResponse) =
                                send(RequestArguments{HttpMethod::POST, "/Task/$create", create, "application/fhir+xml"}
                                         .withJwt(jwt)
                                         .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                                         .withExpectedInnerStatus(HttpStatus::BadRequest)
                                         .withOverrideExpectedWorkflowVersion("XXX")));
}
