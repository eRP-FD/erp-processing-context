/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "fhirtools/converter/FhirConverter.hxx"
#include "fhirtools/repository/FhirStructureDefinition.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/TestUtils.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <date/tz.h>
#include <fmt/ranges.h>
#include <gtest/gtest.h>

class RegressionTest : public ErpWorkflowTest
{
};

TEST_F(RegressionTest, Erp10674)
{
    std::string kbv_bundle_xml =
        ResourceManager::instance().getStringResource("test/validation/xml/kbv/bundle/Bundle_invalid_ERP-10674.xml");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    kbv_bundle_xml = String::replaceAll(kbv_bundle_xml, "160.000.008.870.312.04", task->prescriptionId().toString());
    kbv_bundle_xml = patchVersionsInBundle(kbv_bundle_xml);
    mActivateTaskRequestArgs.overrideExpectedKbvVersion = "XXX";
    std::string accessCode{task->accessCode()};
    ASSERT_NO_FATAL_FAILURE(
        taskActivateWithOutcomeValidation(task->prescriptionId(), accessCode,
                     toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDate("2022-07-29", model::Timestamp::UTCTimezone)),
                     HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(RegressionTest, Erp10669)
{
    using namespace std::literals::chrono_literals;
    using zoned_ms = date::zoned_time<std::chrono::milliseconds>;
    // today at 00:05 in german time zone
    auto zt = zoned_ms{model::Timestamp::GermanTimezone, model::Timestamp::now().localDay() + 5min};
    auto authoredOn = model::Timestamp(zt.get_sys_time());
    auto signingDay = model::Timestamp::now();

    ASSERT_EQ(signingDay.toGermanDate(), authoredOn.toGermanDate()); // sanity check to ensure this is the same day
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    std::string kbv_bundle_xml = kbvBundleMvoXml({.prescriptionId = task->prescriptionId(),
                                                  .authoredOn = signingDay,
                                                  .redeemPeriodStart = authoredOn.toGermanDate(),
                                                  .redeemPeriodEnd = {}});
    std::string accessCode{task->accessCode()};
    std::optional<model::Task> taskActivateResult;
    ASSERT_NO_FATAL_FAILURE(taskActivateResult = taskActivateWithOutcomeValidation(
                                task->prescriptionId(), accessCode, toCadesBesSignature(kbv_bundle_xml, signingDay)));
    ASSERT_TRUE(taskActivateResult);
    EXPECT_EQ(taskActivateResult->expiryDate().localDay(), signingDay.localDay() + date::days{365});
    EXPECT_EQ(taskActivateResult->acceptDate().localDay(), signingDay.localDay() + date::days{365});
    auto bundle = taskGetId(taskActivateResult->prescriptionId(), taskActivateResult->kvnr().value().id());
    ASSERT_TRUE(bundle);
    std::optional<model::Task> getTaskResult;
    getTaskFromBundle(getTaskResult, *bundle);
    ASSERT_TRUE(getTaskResult);
    EXPECT_EQ(getTaskResult->expiryDate().localDay(), signingDay.localDay() + date::days{365});
    EXPECT_EQ(getTaskResult->acceptDate().localDay(), signingDay.localDay() + date::days{365});
}

TEST_F(RegressionTest, Erp10835)
{
    auto task = taskCreate(model::PrescriptionType::direkteZuweisungPkv);
    ASSERT_TRUE(task.has_value());
    auto kvnr = generateNewRandomKVNR().id();
    ASSERT_NO_FATAL_FAILURE(
        taskActivateWithOutcomeValidation(task->prescriptionId(), task->accessCode(),
                     std::get<0>(makeQESBundle(kvnr, task->prescriptionId(), model::Timestamp::now()))));
    ASSERT_NO_FATAL_FAILURE(consentPost(model::ConsentType::CHARGCONS, kvnr, model::Timestamp::now()));
    const auto acceptBundle = taskAccept(task->prescriptionId(), std::string{task->accessCode()});
    ASSERT_TRUE(acceptBundle);
    const auto acceptedTasks = acceptBundle->getResourcesByType<model::Task>();
    ASSERT_EQ(acceptedTasks.size(), 1);
    ASSERT_NO_FATAL_FAILURE(
        taskClose(task->prescriptionId(), std::string{acceptedTasks[0].secret().value_or("")}, kvnr));
    ASSERT_NO_FATAL_FAILURE(chargeItemPost(task->prescriptionId(), kvnr, "3-SMC-B-Testkarte-883110000120312",
                                           std::string{acceptedTasks[0].secret().value_or("")}));
}

class RegressionTestErp8170 : public ErpWorkflowTest
{
protected:
    std::string medicationDispense(const std::string& kvnr, const std::string& prescriptionIdForMedicationDispense,
                                   const std::string&, const std::string&) override
    {
        std::string whenPrepared = "0001-01-01";
        auto xml = ResourceTemplates::medicationDispenseXml({
            .prescriptionId = prescriptionIdForMedicationDispense,
            .kvnr = kvnr,
            .whenPrepared = whenPrepared,
        });
        TVLOG(0) << xml;
        return xml;
    }

    std::string dispenseOrCloseTaskParameters(model::ProfileType profileType, const std::string& kvnr,
                                              const std::string& prescriptionIdForMedicationDispense,
                                              const std::string& whenHandedOver [[maybe_unused]],
                                              size_t numMedicationDispenses, const std::string&) override
    {
        return ErpWorkflowTest::dispenseOrCloseTaskParameters(profileType, kvnr, prescriptionIdForMedicationDispense,
                                                              "0001-01-01", numMedicationDispenses);
    }
};

TEST_F(RegressionTestErp8170, Erp8170)
{
    auto task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel);
    ASSERT_TRUE(task.has_value());
    auto kvnr = generateNewRandomKVNR().id();
    ASSERT_NO_FATAL_FAILURE(
        taskActivateWithOutcomeValidation(task->prescriptionId(), task->accessCode(),
                     std::get<0>(makeQESBundle(kvnr, task->prescriptionId(), model::Timestamp::now()))));
    const auto acceptBundle = taskAccept(task->prescriptionId(), std::string{task->accessCode()});
    ASSERT_TRUE(acceptBundle);
    const auto acceptedTasks = acceptBundle->getResourcesByType<model::Task>();
    ASSERT_EQ(acceptedTasks.size(), 1);
    ASSERT_NO_FATAL_FAILURE(taskClose(task->prescriptionId(), std::string{acceptedTasks[0].secret().value_or("")}, kvnr,
                                      HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(RegressionTest, Erp11142)
{
    const std::string KBV_PR_ERP_Bundle{model::resource::structure_definition::prescriptionItem};
    auto authoredOn = model::Timestamp::now();
    auto supportedVersions = Fhir::instance().structureRepository(authoredOn).supportedVersions({KBV_PR_ERP_Bundle});
    auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel);
    ASSERT_TRUE(task.has_value());
    auto accessCode = task->accessCode();

    const std::string kbv_bundle_orig_xml = ResourceTemplates::kbvBundleXml({
        .prescriptionId = task->prescriptionId(),
        .authoredOn = authoredOn,
        .kbvVersion = kbvVersion,
    });
    const auto& renderVersion = kbvVersion.renderVersion();
    auto kbv_bundle_xml = String::replaceAll(kbv_bundle_orig_xml,
                                             "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|" + renderVersion,
                                             "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle");
    mActivateTaskRequestArgs.overrideExpectedKbvVersion = "XXX";
    ASSERT_NO_FATAL_FAILURE(
        taskActivateWithOutcomeValidation(
            task->prescriptionId(), accessCode,
            toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDateTime("2022-09-14T00:05:57+02:00")),
            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "FHIR-Validation error",
            fmt::format(R"(;Bundle.meta.profile[0]: error: value must match fixed value: )"
                        R"("https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|{}" (but is )"
                        R"("https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle") (from profile: )"
                        R"(https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|{}))",
                        renderVersion, kbvVersion));//
    );

    // additional test with duplicate version, KBV_PR_ERP_Bundle|1.0.3|1.0.3
    kbv_bundle_xml =
        String::replaceAll(kbv_bundle_orig_xml, "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle",
                           "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|" + kbvVersion.renderVersion());
    mActivateTaskRequestArgs.overrideExpectedKbvVersion = kbvVersion.renderVersion();
    ASSERT_NO_FATAL_FAILURE(
        taskActivateWithOutcomeValidation(
            task->prescriptionId(), accessCode,
            toCadesBesSignature(kbv_bundle_xml, model::Timestamp::fromXsDateTime("2022-09-14T00:05:57+02:00")),
            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "parsing / validation error",
            fmt::format(
                "invalid profile https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|{0}|{0} must be one of: {1}",
                renderVersion, fmt::join(supportedVersions, ", ")));//
    );
}

TEST_F(RegressionTest, Erp10892)
{
    static const rapidjson::Pointer metaProfilePtr{"/meta/profile"};

    const auto now = model::Timestamp::now();
    const auto& converter = Fhir::instance().converter();
    std::string kbvProfile{model::resource::structure_definition::prescriptionItem};
    kbvProfile += '|' + to_string(ResourceTemplates::Versions::KBV_ERP_current(now));
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    auto kbvBundleJson = Fhir::instance().converter().xmlStringToJson(
        ResourceTemplates::kbvBundleXml({.prescriptionId = task->prescriptionId(), .authoredOn = now}));
    kbvBundleJson.addToArray(metaProfilePtr, kbvBundleJson.makeString(kbvProfile));
    const auto kbv_bundle_xml = converter.jsonToXmlString(kbvBundleJson, true);
    std::string accessCode{task->accessCode()};
    std::optional<model::Task> taskActivateResult;
    mActivateTaskRequestArgs.overrideExpectedKbvVersion = "XXX";
    ASSERT_NO_FATAL_FAILURE(taskActivateResult = taskActivateWithOutcomeValidation(
                                task->prescriptionId(), accessCode, toCadesBesSignature(kbv_bundle_xml, now),
                                HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(RegressionTest, Erp11050)
{
    std::string kbv_bundle_400_xml =
        ResourceManager::instance().getStringResource("test/issues/ERP-11050/activate_400.xml");
    std::string kbv_bundle_500_xml =
        ResourceManager::instance().getStringResource("test/issues/ERP-11050/activate_500.xml");
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    kbv_bundle_400_xml = String::replaceAll(kbv_bundle_400_xml, "160.000.000.040.283.70", task->prescriptionId().toString());
    kbv_bundle_500_xml = String::replaceAll(kbv_bundle_500_xml, "160.000.000.040.284.67", task->prescriptionId().toString());
    std::string accessCode{task->accessCode()};
    std::optional<model::Task> taskActivateResult;
    mActivateTaskRequestArgs.overrideExpectedKbvVersion = "XXX";
    ASSERT_NO_FATAL_FAILURE(
        taskActivateResult = taskActivateWithOutcomeValidation(
            task->prescriptionId(), accessCode,
            toCadesBesSignature(kbv_bundle_400_xml, model::Timestamp::fromXsDateTime("2022-07-29T00:05:57+02:00")),
            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
    ASSERT_NO_FATAL_FAILURE(
        taskActivateResult = taskActivateWithOutcomeValidation(
            task->prescriptionId(), accessCode,
            toCadesBesSignature(kbv_bundle_500_xml, model::Timestamp::fromXsDateTime("2022-07-29T00:05:57+02:00")),
            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid));
}

TEST_F(RegressionTest, Erp16393)
{
    const fhirtools::DefinitionKey kbvBundleKey{model::resource::structure_definition::prescriptionItem};
    const fhirtools::DefinitionKey normgroesseKey{"http://fhir.de/StructureDefinition/normgroesse"};
    const auto& fhirInstance = Fhir::instance();
    const auto& viewList = fhirInstance.structureRepository(model::Timestamp::now());
    const auto view = viewList.match(kbvBundleKey);
    ASSERT_NE(view, nullptr);
    const auto* kbvBundelDef = view->findStructure(kbvBundleKey);
    ASSERT_NE(kbvBundelDef, nullptr);
    const auto* normgroesseDef = view->findStructure(normgroesseKey);
    ASSERT_NE(normgroesseDef, nullptr);
    ResourceTemplates::Versions::KBV_ERP kbvVersion{kbvBundelDef->version()};
    const auto& kbvVerStr = to_string(kbvVersion);


    std::string expectedDiagnostics{
        // clang-format off
        "Bundle.entry[4].resource{Medication}.extension[3].valueCode: error: "
            "Value ABC not allowed for ValueSet https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_NORMGROESSE|1.00, allowed are "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]KA, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]KTP, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]N1, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]N2, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]N3, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]NB, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]Sonstiges "
                "(from profile: " + to_string(normgroesseDef->key()) + "); "
        "Bundle.entry[4].resource{Medication}.extension[3].valueCode: error: "
            "Value ABC not allowed for ValueSet https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_NORMGROESSE|1.00, allowed are "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]KA, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]KTP, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]N1, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]N2, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]N3, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]NB, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]Sonstiges "
                "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN:Normgroesse:valueCode|" + kbvVerStr + "); "
        "Bundle.entry[4].resource{Medication}.extension[3].valueCode: error: "
            "Value ABC not allowed for ValueSet https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_NORMGROESSE|1.00, allowed are "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]KA, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]KTP, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]N1, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]N2, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]N3, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]NB, "
            "[https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_NORMGROESSE]Sonstiges "
                "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN:Normgroesse|" + kbvVerStr + "); "};
    // clang-format on
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(model::PrescriptionType::apothekenpflichigeArzneimittel));
    ASSERT_TRUE(task.has_value());
    const auto authoredOn = model::Timestamp::now();
    auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
        .prescriptionId = task->prescriptionId(),
        .authoredOn = authoredOn,
        .kbvVersion = kbvVersion,
    });
    kbvBundleXml = String::replaceAll(kbvBundleXml, R"(<valueCode value="N1"/>)", R"(<valueCode value="ABC"/>)");
    std::string accessCode{task->accessCode()};
    mActivateTaskRequestArgs.withOverrideExpectedKbvVersion(kbvVersion.renderVersion());
    std::optional<model::Task> taskActivateResult;
    ASSERT_NO_FATAL_FAILURE(
        taskActivateResult = taskActivateWithOutcomeValidation(
            task->prescriptionId(), accessCode,
            toCadesBesSignature(kbvBundleXml, authoredOn),
            HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, "FHIR-Validation error",
            expectedDiagnostics));
}

TEST_F(RegressionTest, Erp20654InvalidTargetEscape)
{
    ClientResponse serverResponse;
    ClientResponse outerResponse;

    JWT jwt{jwtArzt()};

    RequestArguments args = RequestArguments{HttpMethod::POST, "/Task/$create/%xy", "body", "application/fhir+xml"}
                                .withJwt(jwt)
                                .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                                .withExpectedInnerStatus(HttpStatus::BadRequest);
    args.overrideExpectedInnerOperation = "UNKNOWN";
    args.overrideExpectedInnerRole = "XXX";
    args.overrideExpectedInnerClientId = "XXX";
    args.overrideExpectedLeips = "XXX";

    ASSERT_NO_FATAL_FAILURE(std::tie(outerResponse, serverResponse) = send(args));
}

struct Erp38393Param
{
    HttpMethod method = HttpMethod::POST;
    std::string path;
    std::optional<bde::UseCase> expectedUseCase;
    friend std::ostream& operator<<(std::ostream& os, const Erp38393Param& param)
    {
        os << param.path << " -> " << (param.expectedUseCase?to_string(*param.expectedUseCase):"ERP.VAU");
        return os;
    }
};
class Erp38393BdeUseCase401 : public ErpWorkflowTest, public testing::WithParamInterface<Erp38393Param>
{};
TEST_P(Erp38393BdeUseCase401, test)
{
    // iat claim removed to trigger invalid JWT check.
    auto jwt = JwtBuilder::testBuilder().getJWT(R"({
    "acr": "gematik-ehealth-loa-high",
    "aud": "https://gematik.erppre.de/",
    "exp": 2524608000,
    "display_name": "Vorname Nachname",
    "idNummer": "0123456789",
    "iss": "https://idp1.telematik.de/jwt",
    "jti": "<IDP>_01234567890123456789",
    "nbf": 1585336956,
    "nonce": "fuu bar baz",
    "organizationName": "Institutions- oder Organisations-Bezeichnung",
    "professionOID": "1.2.276.0.76.4.50",
    "sub": "RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk"
}
)");
    auto args = RequestArguments{GetParam().method, GetParam().path, "", "application/fhir+xml"}
    .withJwt(jwt)
    .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
    .withExpectedInnerStatus(HttpStatus::Unauthorized);
    args.expectedBdeUseCase = GetParam().expectedUseCase;
    args.overrideExpectedPrescriptionId = "XXX";
    args.overrideExpectedKbvVersion = "XXX";
    args.overrideExpectedWorkflowVersion = "XXX";
    args.overrideExpectedPatientenrechnungVersion = "XXX";
    args.overrideExpectedDavVersion = "XXX";
    auto [outerResponse, innerResponse] = send(args);
}
class Erp38393BdeUseCase403 : public ErpWorkflowTest, public testing::WithParamInterface<Erp38393Param>
{};
TEST_P(Erp38393BdeUseCase403, test)
{
    // ProfessionOid 1.2.276.0.76.4.292 triggers early 403.
    auto jwt = JwtBuilder::testBuilder().getJWT(R"({
    "acr": "gematik-ehealth-loa-high",
    "aud": "https://gematik.erppre.de/",
    "exp": 2524608000,
    "display_name": "Vorname Nachname",
    "iat": 1585336956,
    "idNummer": "0123456789",
    "iss": "https://idp1.telematik.de/jwt",
    "jti": "<IDP>_01234567890123456789",
    "nbf": 1585336956,
    "nonce": "fuu bar baz",
    "organizationName": "Institutions- oder Organisations-Bezeichnung",
    "professionOID": "1.2.276.0.76.4.292",
    "sub": "RabcUSuuWKKZEEHmrcNm_kUDOW13uaGU5Zk8OoBwiNk"
}
)");
    auto args = RequestArguments{GetParam().method, GetParam().path, "", "application/fhir+xml"}
    .withJwt(jwt)
    .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
    .withExpectedInnerStatus(HttpStatus::Forbidden);
    args.expectedBdeUseCase = GetParam().expectedUseCase;
    args.overrideExpectedPrescriptionId = "XXX";
    args.overrideExpectedKbvVersion = "XXX";
    args.overrideExpectedWorkflowVersion = "XXX";
    args.overrideExpectedPatientenrechnungVersion = "XXX";
    args.overrideExpectedDavVersion = "XXX";
    if (GetParam().path == "/Device" || GetParam().path == "/metadata")
    {
        args.expectedInnerStatus = HttpStatus::OK;
    }
    auto [outerResponse, innerResponse] = send(args);
}
std::vector<Erp38393Param> makeErp38393Params()
{
    const std::string idType160 = "/160.000.000.004.713.80";
    const std::string idType162 = "/162.000.033.491.280.69";
    const std::string idType166 = "/166.100.000.000.001.12";
    const std::string idType169 = "/169.018.562.305.023.72";
    const std::string idType200 = "/200.000.000.000.000.71";
    const std::string idType209 = "/209.000.000.032.994.37";
    return {
        {.path = "/Task/$create", .expectedUseCase = bde::CreateTask_UC_2_1},
        {.path = "/Task" + idType160 + "/$activate", .expectedUseCase = bde::ActivateTask_UC_2_3_160},
        {.path = "/Task" + idType162 + "/$activate", .expectedUseCase = bde::ActivateTask_UC_2_3_162},
        {.path = "/Task" + idType166 + "/$activate", .expectedUseCase = bde::ActivateTask_UC_2_3_166},
        {.path = "/Task" + idType169 + "/$activate", .expectedUseCase = bde::ActivateTask_UC_2_3_169},
        {.path = "/Task" + idType200 + "/$activate", .expectedUseCase = bde::ActivateTask_UC_2_3_200},
        {.path = "/Task" + idType209 + "/$activate", .expectedUseCase = bde::ActivateTask_UC_2_3_209},
        {.path = "/Task" + idType160 + "/$accept", .expectedUseCase = bde::AcceptTask_UC_4_1},
        {.path = "/Task" + idType160 + "/$reject", .expectedUseCase = bde::RejectTask_UC_4_2},
        {.path = "/Task" + idType160 + "/$close", .expectedUseCase = bde::CloseTask_UC_4_4},
        {.path = "/Task" + idType160 + "/$dispense", .expectedUseCase = bde::TaskDispense_UC_4_16},
        // POST /Task/{id}/$abort — use case depends on role (InnerRequestRole), not resolvable early
        {.path = "/Task" + idType209 + "/$abort", .expectedUseCase = std::nullopt},
        // GET /Task — NoUseCase (set directly in handler)
        {.method = HttpMethod::GET, .path = "/Task", .expectedUseCase = std::nullopt},
        // GET /Task/{id} — use case depends on role (InnerRequestRole), not resolvable early
        {.method = HttpMethod::GET, .path = "/Task" + idType160, .expectedUseCase = std::nullopt},
        {.method = HttpMethod::GET,
         .path = "/MedicationDispense",
         .expectedUseCase = bde::GetMedicationDispense_UC_3_9},
        // GET /Communication — use case depends on role (InnerRequestRole), not resolvable early
        {.method = HttpMethod::GET, .path = "/Communication", .expectedUseCase = std::nullopt},
        {.method = HttpMethod::GET, .path = "/AuditEvent", .expectedUseCase = bde::GetAuditEvent_UC_3_5},
        {.method = HttpMethod::GET, .path = "/Device", .expectedUseCase = bde::GetDevice_UC_1_1},
        {.method = HttpMethod::GET, .path = "/metadata", .expectedUseCase = bde::GetMetadata_UC_1_2},
        {.path = "/Subscription", .expectedUseCase = bde::PostSubscription_UC_4_14},
        {.method = HttpMethod::GET, .path = "/ChargeItem", .expectedUseCase = bde::GetChargeItems_UC_3_10},
        // GET /ChargeItem/{id} — use case depends on role (InnerRequestRole), not resolvable early
        {.method = HttpMethod::GET, .path = "/ChargeItem" + idType160, .expectedUseCase = std::nullopt},
        {.path = "/ChargeItem", .expectedUseCase = bde::PostChargeItem_UC_4_11},
        {.method = HttpMethod::DELETE,
         .path = "/ChargeItem" + idType160,
         .expectedUseCase = bde::DeleteChargeItem_UC_3_11},
        {.method = HttpMethod::PATCH,
         .path = "/ChargeItem" + idType160,
         .expectedUseCase = bde::PatchChargeItem_UC_3_12},
        {.method = HttpMethod::PUT, .path = "/ChargeItem" + idType160, .expectedUseCase = bde::PutChargeItem_UC_4_13},
        {.method = HttpMethod::GET, .path = "/Consent", .expectedUseCase = bde::GetConsent_UC_3_13},
        {.path = "/Consent", .expectedUseCase = bde::PostConsent_UC_3_14},
        {.method = HttpMethod::DELETE, .path = "/Consent", .expectedUseCase = bde::DeleteConsent_UC_3_15},
    };
}
INSTANTIATE_TEST_SUITE_P(test, Erp38393BdeUseCase401, testing::ValuesIn(makeErp38393Params()));
INSTANTIATE_TEST_SUITE_P(test, Erp38393BdeUseCase403, testing::ValuesIn(makeErp38393Params()));