/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
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
