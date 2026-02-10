/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/database/DatabaseFrontend.hxx"
#include "erp/service/task/ActivateTaskHandler.hxx"
#include "fhirtools/converter/FhirConverter.hxx"
#include "fhirtools/repository/FhirValueSet.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/model/Resource.hxx"
#include "test/erp/service/EndpointHandlerTest/EndpointHandlerTestFixture.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"

#include <boost/algorithm/string/replace.hpp>
#include <date/tz.h>

class ActivateTaskTest : public EndpointHandlerTest
{
public:
    std::string requestBody(const std::string_view& kbvBundleXml, std::optional<model::Timestamp> signingTime)
    {
        auto cadesBesSignatureFile = CryptoHelper::toCadesBesSignature(std::string(kbvBundleXml), signingTime);
        return R"--(
<Parameters xmlns="http://hl7.org/fhir">
    <parameter>
        <name value="ePrescription" />
        <resource>
            <Binary>
                <contentType value="application/pkcs7-mime" />
                <data value=")--" +
                                 cadesBesSignatureFile + R"--("/>
            </Binary>
        </resource>
    </parameter>
</Parameters>)--";
    }

    ServerRequest serverRequest(const std::string_view& taskJson, const std::string_view& kbvBundleXml,
                                std::optional<model::Timestamp> signingTime)
    {
        const auto& origTask = model::Task::fromJsonNoValidation(taskJson);

        ActivateTaskHandler handler({});
        Header requestHeader{HttpMethod::POST,
                             "/Task/" + origTask.prescriptionId().toString() + "/$activate",
                             0,
                             {{Header::ContentType, ContentMimeType::fhirXmlUtf8}},
                             HttpStatus::Unknown};
        requestHeader.addHeaderField("X-AccessCode", std::string(origTask.accessCode()));
        ServerRequest serverRequest{std::move(requestHeader)};
        serverRequest.setBody(requestBody(kbvBundleXml, signingTime));
        serverRequest.setPathParameters({"id"}, {origTask.prescriptionId().toString()});
        return serverRequest;
    }
    struct ActivateTaskArgs {
        HttpStatus expectedStatus = HttpStatus::OK;
        std::optional<model::Timestamp> signingTime = std::nullopt;
        std::optional<model::Timestamp> expectedExpiry = std::nullopt;
        std::optional<model::Timestamp> expectedAcceptDate = std::nullopt;
        bool insertTask = false;
        std::optional<std::reference_wrapper<std::exception_ptr>> outExceptionPtr = std::nullopt;
        std::optional<std::reference_wrapper<Header>> outHeader = std::nullopt;
        std::optional<std::string_view> flowTypeDisplayName = std::nullopt;
        bool expectedEuRedeemable = false;
    };

    void checkActivateTask(PcServiceContext& serviceContext, const std::string_view& taskJson,
                           const std::string_view& kbvBundleXml, const std::string_view& expectedKvnr,
                           ActivateTaskArgs args)
    {
        mockDatabase.reset();
        const auto& origTask = model::Task::fromJsonNoValidation(taskJson);

        ActivateTaskHandler handler({});
        ServerRequest request{serverRequest(taskJson, kbvBundleXml, args.signingTime)};
        request.setAccessToken(mJwtBuilder->makeJwtArzt());
        ServerResponse serverResponse;
        AccessLog accessLog;
        SessionContext sessionContext{serviceContext, request, serverResponse, accessLog};
        if (args.insertTask)
        {
            createDatabase(mServiceContext.getHsmPool(), mServiceContext.getKeyDerivation());
            ASSERT_NE(mockDatabase, nullptr);
            mockDatabase->insertTask(origTask);
        }

        ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
        if (args.expectedStatus != HttpStatus::OK && args.expectedStatus != HttpStatus::Accepted &&
            args.expectedStatus != HttpStatus::OkWarnInvalidAnr)
        {
            auto callHandler = [&]{
                try {
                    handler.handleRequest(sessionContext);
                } catch (...) {
                    if (args.outExceptionPtr)
                    {
                        args.outExceptionPtr->get() = std::current_exception();
                    }
                    throw;
                }
            };
            EXPECT_ERP_EXCEPTION(callHandler(), args.expectedStatus);
            return;
        }

        ASSERT_NO_THROW(handler.handleRequest(sessionContext));
        ASSERT_EQ(serverResponse.getHeader().status(), args.expectedStatus);
        if (args.outHeader)
        {
            args.outHeader->get() = serverResponse.getHeader();
        }

        std::optional<model::Task> task;
        ASSERT_NO_THROW(task = model::Task::fromXml(serverResponse.getBody(), *StaticData::getXmlValidator()))
            << serverResponse.getBody();
        ASSERT_TRUE(task.has_value());
        EXPECT_EQ(task->prescriptionId(), origTask.prescriptionId());
        EXPECT_EQ(task->status(), model::Task::Status::ready);
        EXPECT_EQ(task->kvnr().value(), expectedKvnr);
        EXPECT_NO_THROW((void) task->expiryDate());
        EXPECT_FALSE(task->lastModifiedDate().toXsDateTime().empty());
        EXPECT_FALSE(task->authoredOn().toXsDateTime().empty());
        EXPECT_FALSE(task->accessCode().empty());
        EXPECT_FALSE(task->expiryDate().toXsDateTime().empty());
        if (args.expectedExpiry.has_value())
        {
            EXPECT_EQ(*args.expectedExpiry, task->expiryDate());
        }
        EXPECT_FALSE(task->acceptDate().toXsDateTime().empty());
        if (args.expectedAcceptDate.has_value())
        {
            EXPECT_EQ(*args.expectedAcceptDate, task->acceptDate());
        }
        EXPECT_TRUE(task->healthCarePrescriptionUuid().has_value());
        EXPECT_TRUE(task->patientConfirmationUuid().has_value());
        EXPECT_FALSE(task->receiptUuid().has_value());
        EXPECT_EQ(args.expectedEuRedeemable, task->isEuRedeemableByProperties());

        if (args.flowTypeDisplayName.has_value())
        {
            auto ext = task->getExtension(model::resource::structure_definition::prescriptionType);
            ASSERT_TRUE(ext.has_value());
            auto flowTypeDisplay = ext->valueCodingDisplay();
            ASSERT_TRUE(flowTypeDisplay.has_value());
            EXPECT_EQ(flowTypeDisplay, args.flowTypeDisplayName.value());
        }

        using namespace model::resource;
        using fn = std::optional<std::string_view> (*)(const rapidjson::Value& object, const rapidjson::Pointer& key);
        static constexpr auto getOptionalStringValue =
        static_cast<fn>(&model::NumberAsStringParserDocument::getOptionalStringValue);
        auto taskDoc = model::NumberAsStringParserDocument::fromJson(task->serializeToJsonString());
        static const rapidjson::Pointer performerTypeCodingPtr{
            ElementName::path(ElementName{"performerType"}, 0, elements::coding, 0)};
        auto* performerTypeCoding = performerTypeCodingPtr.Get(taskDoc);
        ASSERT_NE(performerTypeCoding, nullptr);
        static const rapidjson::Pointer systemPtr{ElementName::path(elements::system)};
        static const rapidjson::Pointer codePtr{ElementName::path(elements::code)};
        static const rapidjson::Pointer displayPtr{ElementName::path(elements::display)};
        auto performerTypeCodingSystem = getOptionalStringValue(*performerTypeCoding, systemPtr);
        auto performerTypeCodingCode = getOptionalStringValue(*performerTypeCoding, codePtr);
        auto performerTypeCodingDisplay = getOptionalStringValue(*performerTypeCoding, displayPtr);
        ASSERT_TRUE(performerTypeCodingSystem.has_value());
        ASSERT_EQ(std::string{*performerTypeCodingSystem},
                  "https://gematik.de/fhir/erp/CodeSystem/GEM_ERP_CS_OrganizationType");
        ASSERT_TRUE(performerTypeCodingCode.has_value());
        ASSERT_TRUE(performerTypeCodingDisplay.has_value());
        if (isDiga(task->prescriptionId().type()))
        {
            ASSERT_EQ(std::string{*performerTypeCodingCode}, "urn:oid:1.2.276.0.76.4.59");
            ASSERT_EQ(std::string{*performerTypeCodingDisplay}, "Kostenträger");
        }
        else
        {
            ASSERT_EQ(std::string{*performerTypeCodingCode}, "urn:oid:1.2.276.0.76.4.54");
            ASSERT_EQ(std::string{*performerTypeCodingDisplay}, "Öffentliche Apotheke");
        }
    }

    bool enableDarreichungsformTest() const
    {
        const fhirtools::DefinitionKey darreichungsformKey{
            "https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM", std::nullopt};
        auto allViews = Fhir::instance().allViews();
        std::set<fhirtools::DefinitionKey> keys;
        for (const auto& view: allViews.all())
        {
            if (auto&& found = view->findValueSet(darreichungsformKey))
            {
                keys.emplace(found->key());
            }
        }
        Expect(keys.size() > 0, "must have at least one Darreichungsform version.");
        return keys.size() != 1;
    }

};

class ActivateTaskTestPkv : public ActivateTaskTest
{
protected:

    std::string_view pkvInsuranceType(const ResourceTemplates::Versions::KBV_ERP& ver)
    {
        if (ver >= ResourceTemplates::Versions::KBV_ERP_1_3_3)
        {
            return "KVZ10";
        }
        return "PKV";
    }
    std::string_view gkvInsuranceType(const ResourceTemplates::Versions::KBV_ERP& ver)
    {
        if (ver >= ResourceTemplates::Versions::KBV_ERP_1_3_3)
        {
            return "KVZ10";
        }
        return "GKV";
    }
    std::string_view pkvKvid10Ns(const ResourceTemplates::Versions::KBV_ERP& ver)
    {
        if (ver >= ResourceTemplates::Versions::KBV_ERP_1_3_3)
        {
            return model::resource::naming_system::gkvKvid10;
        }
        return model::resource::naming_system::pkvKvid10;
    }
};

TEST_F(ActivateTaskTest, ActivateTask)
{
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({.prescriptionId = taskId, .authoredOn = signingTime});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, taskJson, kbvBundleXml, "X234567891", {.signingTime = signingTime}));
}

TEST_F(ActivateTaskTest, ActivateTask166)
{
    testutils::ShiftFhirResourceViewsGuard kbv14Guard{
        "KBV_1_4", date::floor<date::days>(model::Timestamp::now().toChronoTimePoint())};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::tRezept, 4800);
    ResourceTemplates::KbvBundleOptions options{.prescriptionId = taskId, .authoredOn = signingTime};
    options.medicationOptions.medicationCategory = "02";
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml(options);
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = taskId});
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, taskJson, kbvBundleXml, "X234567891",
                                              {.signingTime = signingTime}));
}

TEST_F(ActivateTaskTest, ActivateTask166Kbv13)
{
    auto signingTime = model::Timestamp::now();
    const auto taskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::tRezept, 4800);
    const auto fakeId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    ResourceTemplates::KbvBundleOptions options{.prescriptionId = fakeId, .authoredOn = signingTime};
    options.kbvVersion = ResourceTemplates::Versions::KBV_ERP_1_3_3;
    options.medicationOptions.version = ResourceTemplates::Versions::KBV_ERP_1_3_3;
    options.medicationOptions.medicationCategory = "02";
    auto kbvBundleXml = ResourceTemplates::kbvBundleXml(options);
    boost::replace_all(kbvBundleXml, fakeId.toString(), taskId.toString());
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = taskId});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, "X234567891",
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(std::rethrow_exception(exception), HttpStatus::BadRequest,
                                      "T-Rezept Workflow 166 not allowed before KBV-1.4");
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkv)
{
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50010);
    const char* const pkvKvnr = "X500000017";
    const auto authoredOn = model::Timestamp::now();

    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId, .timestamp = authoredOn});
    const auto bundle =
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId, .authoredOn = authoredOn, .kvnr = pkvKvnr});
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr, {.signingTime = authoredOn}));
}

TEST_F(ActivateTaskTestPkv, ActivateTask166Pkv)
{
    testutils::ShiftFhirResourceViewsGuard kbv14Guard{
        "KBV_1_4", date::floor<date::days>(model::Timestamp::now().toChronoTimePoint())};
    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::tRezept, 4801);
    const auto authoredOn = model::Timestamp::now();

    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId, .timestamp = authoredOn});
    ResourceTemplates::KbvBundleOptions options{.prescriptionId = pkvTaskId, .authoredOn = authoredOn, .tRezeptIsPkv = true};
    options.medicationOptions.medicationCategory = "02";
    const auto bundle = ResourceTemplates::kbvBundleXml(options);
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, "X234567891",
                                              {.signingTime = authoredOn}));
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkv209)
{
    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50011);
    const char* const pkvKvnr = "X500000029";
    const auto authoredOn = model::Timestamp::now();

    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId, .timestamp = authoredOn});
    const auto bundle =
        ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId, .authoredOn = authoredOn, .kvnr = pkvKvnr});
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr, {.signingTime = authoredOn}));
}

TEST_F(ActivateTaskTest, ActivateTaskDiga)
{
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6001);
    const auto kbvBundleXml = ResourceTemplates::evdgaBundleXml(
        {.prescriptionId = taskId.toString(), .kvnr = "X234567891", .authoredOn = signingTime.toGermanDate()});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, taskJson, kbvBundleXml, "X234567891", {.signingTime = signingTime}));
}

TEST_F(ActivateTaskTest, ActivateTaskEuRedeemable)
{
    A_27768.test("Bestimmung der Einlösbarkeit im EU-Ausland");
    const EnvironmentVariableGuard featureEuGuard{ConfigurationKey::FEATURE_EU, "true"};
    testutils::ShiftFhirResourceViewsGuard erp15Guard{
        "GEM_WF_1_5_0", floor<std::chrono::days>(model::Timestamp::now().toChronoTimePoint())};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({.prescriptionId = taskId, .authoredOn = signingTime});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, taskJson, kbvBundleXml, "X234567891",
                                              {.signingTime = signingTime, .expectedEuRedeemable = true}));
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkvInvalidCoverage200)
{
    A_22347_01.test("invalid coverage WF 200");

    const auto pkvTaskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50010);
    const char* const pkvKvnr = "X500000017";
    const auto authoredOn = model::Timestamp::now();
    const auto& kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId, .timestamp = authoredOn});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId,
                                                         .authoredOn = authoredOn,
                                                         .kvnr = pkvKvnr,
                                                         .kbvVersion = kbvVersion,
                                                         .coverageInsuranceType = "GKV",
                                                         .forceInsuranceType = gkvInsuranceType(kbvVersion),
                                                         .forceKvid10Ns = model::resource::naming_system::gkvKvid10});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = authoredOn,
                                               .insertTask = false,
                                               .outExceptionPtr = exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(std::string(ex.what()), "Coverage \"PKV\" not set for flowtype 200/209")
            << ex.diagnostics().value_or("");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkvInvalidCoverage209)
{
    A_22347_01.test("invalid coverage WF 209");

    const auto pkvTaskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50011);
    const char* const pkvKvnr = "X500000029";
    const auto authoredOn = model::Timestamp::now();
    const auto& kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = pkvTaskId, .timestamp = authoredOn});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = pkvTaskId,
                                                         .authoredOn = authoredOn,
                                                         .kvnr = pkvKvnr,
                                                         .kbvVersion = kbvVersion,
                                                         .coverageInsuranceType = "GKV",
                                                         .forceInsuranceType = gkvInsuranceType(kbvVersion),
                                                         .forceKvid10Ns = model::resource::naming_system::gkvKvid10});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = authoredOn,
                                               .insertTask = false,
                                               .outExceptionPtr = exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(std::string(ex.what()), "Coverage \"PKV\" not set for flowtype 200/209")
            << ex.diagnostics().value_or("");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkvInvalidCoverage160)
{
    A_23443_01.test("invalid coverage WF 160");
    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 50010);
    const char* const pkvKvnr = "X500000017";
    const auto authoredOn = model::Timestamp::now();
    const auto& kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = authoredOn});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId,
                                                         .authoredOn = authoredOn,
                                                         .kvnr = pkvKvnr,
                                                         .kbvVersion = kbvVersion,
                                                         .coverageInsuranceType = "PKV",
                                                         .forceInsuranceType = pkvInsuranceType(kbvVersion),
                                                         .forceKvid10Ns = pkvKvid10Ns(kbvVersion)});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = authoredOn,
                                               .insertTask = true,
                                               .outExceptionPtr = exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(std::string(ex.what()), "Coverage \"PKV\" not allowed for flowtype 160/162/169")
            << ex.diagnostics().value_or("");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkvInvalidCoverage169)
{
    A_23443_01.test("invalid coverage WF 169");

    const auto prescriptionId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisung, 50010);
    const char* const pkvKvnr = "X500000017";
    const auto authoredOn = model::Timestamp::now();
    const auto& kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = authoredOn});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId,
                                                         .authoredOn = authoredOn,
                                                         .kvnr = pkvKvnr,
                                                         .kbvVersion = kbvVersion,
                                                         .coverageInsuranceType = "PKV",
                                                         .forceInsuranceType = pkvInsuranceType(kbvVersion),
                                                         .forceKvid10Ns = pkvKvid10Ns(kbvVersion)});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = authoredOn,
                                               .insertTask = true,
                                               .outExceptionPtr = exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(std::string(ex.what()), "Coverage \"PKV\" not allowed for flowtype 160/162/169")
            << ex.diagnostics().value_or("");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTestPkv, ActivateTaskPkvInvalidCoverage162)
{
    A_23443_01.test("invalid coverage WF 162");

    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 50010);
    const char* const pkvKvnr = "X500000017";
    const auto authoredOn = model::Timestamp::now();
    const auto& kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    const auto task = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = authoredOn});

    const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId,
                                                         .authoredOn = authoredOn,
                                                         .kvnr = pkvKvnr,
                                                         .kbvVersion = kbvVersion,
                                                         .coverageInsuranceType = "PKV",
                                                         .forceInsuranceType = pkvInsuranceType(kbvVersion),
                                                         .forceKvid10Ns = pkvKvid10Ns(kbvVersion)});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, pkvKvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = authoredOn,
                                               .insertTask = true,
                                               .outExceptionPtr = exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        // Its not getting to the point where the coverage is checked, it fails way earlier...
        EXPECT_EQ(std::string(ex.what()),
                  "Für diesen Workflowtypen sind nur Verordnungen für Digitale Gesundheitsanwendungen zulässig")
            << ex.diagnostics().value_or("");
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTest, ActivateTaskInvalidPatientCodingCode)
{
    auto currentKbv = ResourceTemplates::Versions::KBV_ERP_current();
    A_23936.test("Test invalid Patient.Coding.Code");
    const auto* kvnr = "123456";
    const auto prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    auto signingTime = model::Timestamp::now();
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = prescriptionId,
         .authoredOn = signingTime,
         .kvnr = kvnr,
         .kbvVersion = currentKbv,
         .coverageInsuranceSystem = "http://fhir.de/CodeSystem/versicherungsart-de-basis",
         .coverageInsuranceType = "SEL",
         .forceInsuranceType = "kvk",
         .forceKvid10Ns = "http://fhir.de/sid/gkv/kvk-versichertennummer",
         .patientIdentifierTypeSystem = "https://fhir.kbv.de/CodeSystem/KBV_CS_Base_identifier_type"});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = prescriptionId});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, taskJson, kbvBundleXml, kvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = signingTime,
                                               .expectedExpiry = std::nullopt,
                                               .outExceptionPtr = exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        if (currentKbv < ResourceTemplates::Versions::KBV_ERP_1_3_3)
        {
            EXPECT_EQ(std::string(ex.what()),
                    "Als Identifier für den Patienten muss eine Versichertennummer angegeben werden.")
                << ex.diagnostics().value_or("");
        }
        else
        {
            auto ver = to_string(currentKbv);
            expectErpExceptionWithFHIRValidationError(
                ex, "FHIR-Validation error",
                "Bundle: error: -erp-angabeKVKVersichertennummerVerbot: In der Ressource vom Typ Patient ist "
                "eine KVK-Versichertennummer vorhanden, diese darf nicht angegeben werden. (from profile: "
                      "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|" + ver + "); ");
        }
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}


TEST_F(ActivateTaskTest, ActivateTaskInvalidPatientIdentifierSystem)
{
    auto currentKbv = ResourceTemplates::Versions::KBV_ERP_current();
    A_23936.test("Test invalid Patient.Coding.Code");
    const auto* kvnr = "X123456789";
    const auto prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    auto signingTime = model::Timestamp::now();
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
        .prescriptionId = prescriptionId,
        .authoredOn = signingTime,
        .kvnr = kvnr,
        .kbvVersion = currentKbv,
        .forceKvid10Ns = model::resource::naming_system::pkvKvid10,
    });
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = prescriptionId});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, taskJson, kbvBundleXml, kvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = signingTime,
                                               .expectedExpiry = std::nullopt,
                                               .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    auto forVer = ResourceTemplates::Versions::latest("https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Patient");
    ASSERT_TRUE(forVer.version.has_value());
    // clang-format off
    std::string diagnostics =
        "Bundle.entry[1].resource{Patient}.identifier[0].system: "
            "error: value must match fixed value: \"http://fhir.de/sid/gkv/kvid-10\" (but is \"http://fhir.de/sid/pkv/kvid-10\") "
                "(from profile: http://fhir.de/StructureDefinition/identifier-kvid-10|1.5.2); "
        "Bundle.entry[1].resource{Patient}.identifier[0].system: "
            "error: value must match fixed value: \"http://fhir.de/sid/gkv/kvid-10\" (but is \"http://fhir.de/sid/pkv/kvid-10\") "
                "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Patient:versichertenId|" + to_string(*forVer.version) + "); ";
    if (currentKbv <= ResourceTemplates::Versions::KBV_ERP_1_1_0)
    {
        diagnostics =
            "Bundle.entry[1].resource{Patient}.identifier[0]: "
                "error: element doesn't match any slice in closed slicing "
                    "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Patient|1.1.0); ";
    }
    // clang-format on
    EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(std::rethrow_exception(exception), diagnostics);
}


TEST_F(ActivateTaskTest, ActivateTaskBrokenSignature)
{
    const auto taskId = model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto taskJson = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext,
                                              taskJson,
                                              "",// empty signature
                                              "X234567891", {HttpStatus::BadRequest}));
}

TEST_F(ActivateTaskTest, AuthoredOnSignatureDateEquality)
{
    A_22487.test("Task aktivieren - Prüfregel Ausstellungsdatum");
    const auto* kvnr = "X234567891";
    const auto prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    auto task =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId});
    struct TestData
    {
        model::Timestamp signingTime;
        model::Timestamp authoredDate;
        HttpStatus expectedStatus;
    };
    const auto signingTime1 = model::Timestamp::fromXsDateTime("2022-06-25T22:33:00+00:00");
    const auto authoredTime1 = model::Timestamp::fromXsDate("2022-06-26", model::Timestamp::UTCTimezone);
    const auto signingTime2 = model::Timestamp::fromXsDateTime("2022-06-25T00:33:00+00:00");
    const auto authoredTime2 = model::Timestamp::fromXsDate("2022-06-25", model::Timestamp::UTCTimezone);
    const auto signingTime3 = model::Timestamp::fromXsDateTime("2022-06-26T12:33:00+00:00");
    const auto authoredTime3 = model::Timestamp::fromXsDate("2022-06-26", model::Timestamp::UTCTimezone);
    const auto signingTime4 = model::Timestamp::fromXsDateTime("2022-01-25T23:33:00+00:00");
    const auto authoredTime4 = model::Timestamp::fromXsDate("2022-01-26", model::Timestamp::UTCTimezone);
    const auto signingTime5 = model::Timestamp::fromXsDateTime("2022-01-25T22:33:00+00:00");
    const auto authoredTime5 = model::Timestamp::fromXsDate("2022-01-25", model::Timestamp::UTCTimezone);
    std::vector<TestData> testSet {
        {signingTime1, authoredTime1, HttpStatus::OK},
        {signingTime2, authoredTime1, HttpStatus::BadRequest},
        {signingTime3, authoredTime1, HttpStatus::OK},

        {signingTime1, authoredTime2, HttpStatus::BadRequest},
        {signingTime2, authoredTime2, HttpStatus::OK},
        {signingTime3, authoredTime2, HttpStatus::BadRequest},

        {signingTime1, authoredTime3, HttpStatus::OK},
        {signingTime2, authoredTime3, HttpStatus::BadRequest},
        {signingTime3, authoredTime3, HttpStatus::OK},

        {signingTime4, authoredTime4, HttpStatus::OK},
        {signingTime5, authoredTime4, HttpStatus::BadRequest},

        {signingTime5, authoredTime5, HttpStatus::OK},
        {signingTime4, authoredTime5, HttpStatus::BadRequest},
    };

    for (const auto& testData : testSet)
    {
        const auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(testData.authoredDate);
        const auto bundle = ResourceTemplates::kbvBundleXml({.prescriptionId = prescriptionId,
                                                             .authoredOn = testData.authoredDate,
                                                             .kvnr = kvnr,
                                                             .kbvVersion = kbvVersion});
        ASSERT_NO_FATAL_FAILURE(
            checkActivateTask(mServiceContext, task, bundle, kvnr, {testData.expectedStatus, testData.signingTime}));
    }
}


TEST_F(ActivateTaskTest, ThreeMonthExpiry)
{
    const auto* kvnr = "X234567891";
    const auto prescriptionId = model::PrescriptionId::fromString("160.000.000.004.713.80");
    const auto authoredOn = model::Timestamp::fromXsDateTime("2022-08-31T18:40:00+00:00");
    const auto task = ResourceTemplates::taskJson({
        .taskType = ResourceTemplates::TaskType::Draft,
        .prescriptionId = prescriptionId,
        .timestamp = authoredOn
    });
    const auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(authoredOn);
    auto bundle = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = prescriptionId, .authoredOn = authoredOn, .kvnr = kvnr, .kbvVersion = kbvVersion});

    auto expectedExpiryTime = model::Timestamp::fromGermanDate("2022-11-30");
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, task, bundle, kvnr,
                                              {.signingTime = authoredOn, .expectedExpiry = expectedExpiryTime}));
}


TEST_F(ActivateTaskTest, Erp10633UnslicedExtension)
{
    A_22927_01.test("Task aktivieren - Ausschluss unspezifizierter Extensions");
    testutils::ShiftFhirResourceViewsGuard shiftGuard{testutils::ShiftFhirResourceViewsGuard::asConfigured};
    EnvironmentVariableGuard onUnknownExtensionGuard{
            "ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "reject"};
    // not yet active:
    EnvironmentVariableGuard onUnknownExtensionGuard2{
        "ERP_FHIR_VALIDATION_REJECT_UNSLICED_EXTENSIONS_FROM", "2022-07-26"};
    const auto timestamp = model::Timestamp::fromXsDate("2022-07-25", model::Timestamp::UTCTimezone);
    const auto* kvnr = "Y229270213";
    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 429);
    const auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(timestamp);
    auto bundle = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = prescriptionId, .authoredOn = timestamp, .kvnr = kvnr, .kbvVersion = kbvVersion});
    // intent is only used for medication request and immediately after status, where we want to insert the extension
    auto extraStatusPos = bundle.find(R"(<intent value="order")");
    ASSERT_NE(extraStatusPos, std::string::npos);
    extraStatusPos = bundle.rfind("/>", extraStatusPos);
    ASSERT_NE(extraStatusPos, std::string::npos);
    bundle.replace(extraStatusPos, 2,
                  R"(><extension url="subStatus"><valueBoolean value="true"/></extension></status>)");
    auto taskJson = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = timestamp, .kvnr = kvnr});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, taskJson, bundle, kvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = timestamp,
                                               .insertTask = true,
                                               .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    std::string diagnostics =
        "Bundle.entry[6].resource{MedicationRequest}.status.extension[0]: error: element doesn't belong to any slice. "
        "(from profile: http://hl7.org/fhir/StructureDefinition/Extension|4.0.1); "
        "Bundle.entry[6].resource{MedicationRequest}.status.extension[0]: error: element doesn't belong to any slice. "
        "(from profile: http://hl7.org/fhir/StructureDefinition/code|4.0.1); ";
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE_AND_FHIR_VALIDATION_ERROR(
        std::rethrow_exception(exception), "Unintendierte Verwendung von Extensions an unspezifizierter Stelle",
        diagnostics);
}

TEST_F(ActivateTaskTest, UnslicedExtension)
{
    A_22927_02.test("FHIR-Ressource validieren - Ausschluss unspezifizierter Extensions");
    testutils::ShiftFhirResourceViewsGuard shiftGuard{testutils::ShiftFhirResourceViewsGuard::asConfigured};
    EnvironmentVariableGuard onUnknownExtensionGuard{
            "ERP_SERVICE_TASK_ACTIVATE_KBV_VALIDATION_ON_UNKNOWN_EXTENSION", "ignore"};
    // now active:
    EnvironmentVariableGuard onUnknownExtensionGuard2{
        "ERP_FHIR_VALIDATION_REJECT_UNSLICED_EXTENSIONS_FROM", "2022-07-25"};
    const auto timestamp = model::Timestamp::fromXsDate("2022-07-25", model::Timestamp::UTCTimezone);
    const auto* kvnr = "Y229270213";
    const auto prescriptionId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 429);
    const auto kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(timestamp);
    auto bundle = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = prescriptionId, .authoredOn = timestamp, .kvnr = kvnr, .kbvVersion = kbvVersion});
    // intent is only used for medication request and immediately after status, where we want to insert the extension
    auto extraStatusPos = bundle.find(R"(<intent value="order")");
    ASSERT_NE(extraStatusPos, std::string::npos);
    extraStatusPos = bundle.rfind("/>", extraStatusPos);
    ASSERT_NE(extraStatusPos, std::string::npos);
    bundle.replace(extraStatusPos, 2,
                  R"(><extension url="subStatus"><valueBoolean value="true"/></extension></status>)");
    auto taskJson = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = timestamp, .kvnr = kvnr});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, taskJson, bundle, kvnr,
                                              {.expectedStatus = HttpStatus::BadRequest,
                                               .signingTime = timestamp,
                                               .insertTask = true,
                                               .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    std::string diagnostics =
        "Bundle.entry[6].resource{MedicationRequest}.status.extension[0]: error: element doesn't belong to any slice. "
        "(from profile: http://hl7.org/fhir/StructureDefinition/Extension|4.0.1); "
        "Bundle.entry[6].resource{MedicationRequest}.status.extension[0]: error: element doesn't belong to any slice. "
        "(from profile: http://hl7.org/fhir/StructureDefinition/code|4.0.1); ";
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE_AND_FHIR_VALIDATION_ERROR(
        std::rethrow_exception(exception), "Unintendierte Verwendung von Extensions an unspezifizierter Stelle",
        diagnostics);
}


TEST_F(ActivateTaskTest, authoredOnReference)
{
    if (!enableDarreichungsformTest())
    {
        // consider adapting this test when two darreichungsform versions are present again!
        GTEST_SKIP();
    }

    auto now = std::chrono::system_clock::now();
    auto today{date::floor<date::days>(now)};
    auto yesterday{today - date::days{1}};

    testutils::ShiftFhirResourceViewsGuard viewGuard{"KBV_2023-07-01", fhirtools::Date{today, fhirtools::Date::Precision::day}};

    using namespace std::chrono_literals;
    const auto* kvnr = "X234567891";
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto taskJson = ResourceTemplates::taskJson({.gematikVersion = ResourceTemplates::Versions::GEM_ERP_1_4,
                                                       .taskType = ResourceTemplates::TaskType::Ready,
                                                       .prescriptionId = taskId});

    // authoredOn is in DARREICHUNGSFORM 1.14 period (yesterday)
    {
        const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
            .prescriptionId = taskId,
            .authoredOn = model::Timestamp{yesterday},
            .medicationOptions =
                {
                    .version = ResourceTemplates::Versions::KBV_ERP_current(),
                    .darreichungsform = "LYE",
                },
        });
        ASSERT_NO_FATAL_FAILURE(
            checkActivateTask(mServiceContext, taskJson, kbvBundleXml, kvnr,
                              {.expectedStatus = HttpStatus::BadRequest, .signingTime = model::Timestamp{yesterday}}));
    }

    // authoredOn is in DARREICHUNGSFORM 1.15 period (today)
    {
        const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
            .prescriptionId = taskId,
            .authoredOn = model::Timestamp{today},
            .medicationOptions =
                {
                    .version = ResourceTemplates::Versions::KBV_ERP_current(),
                    .darreichungsform = "LYE",
                },
        });
        ASSERT_NO_FATAL_FAILURE(
            checkActivateTask(mServiceContext, taskJson, kbvBundleXml, kvnr, {.signingTime = model::Timestamp{today}}));
    }
}

TEST_F(ActivateTaskTest, ERP_12708_kbv_bundle_RepeatedErrorMessages)
{
    static const std::string expectedDiagnostic{
        R"(Bundle.entry[1].resource{Patient}.identifier[0]: error: )"
        R"(element doesn't match any slice in closed slicing)"
        R"( (from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Patient|1.1.0); )"};
    ResourceManager& resourceManager = ResourceManager::instance();
    const auto& kbvBundle =
        resourceManager.getStringResource("test/EndpointHandlerTest/ERP-12708-kbv_bundle-RepeatedErrorMessages.xml");
    std::string_view kvnr{"X123456788"};
    auto task = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .kvnr = kvnr});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, kbvBundle, kvnr,
                          {.expectedStatus = HttpStatus::BadRequest, .outExceptionPtr = exception}));
    try {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        ASSERT_TRUE(ex.diagnostics().has_value());
        EXPECT_EQ(ex.diagnostics().value(), expectedDiagnostic);
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTest, ERP12860_WrongProfile)
{
    static const std::string prescriptionItem{model::resource::structure_definition::prescriptionItem};
    std::string_view kvnr{"X123456788"};
    auto kbvBundle = ResourceTemplates::kbvBundleXml({.authoredOn = model::Timestamp::now(), .kvnr = kvnr});
    kbvBundle = String::replaceAll(kbvBundle, prescriptionItem, "https://sample.de/StructureDefinition/KBV_PR_ERP_Bundle");
    auto task = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .kvnr = kvnr});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, kbvBundle, kvnr,
                          {.expectedStatus = HttpStatus::BadRequest, .outExceptionPtr = exception}));
    try
    {
        ASSERT_TRUE(exception);
        std::rethrow_exception(exception);
    }
    catch (const ErpException& ex)
    {
        ASSERT_TRUE(ex.diagnostics().has_value());
        auto viewList = Fhir::instance().structureRepository(model::Timestamp::now());
        std::ostringstream supportedProfiles;
        for (std::string_view sep; const auto& key: viewList.supportedVersions({prescriptionItem}))
        {
            supportedProfiles << sep << key;
            sep = ", ";
        }
        // see ERP-13187:
        EXPECT_EQ(ex.diagnostics().value(), "invalid profile https://sample.de/StructureDefinition/KBV_PR_ERP_Bundle|" +
                                                to_string(value(viewList.latestRenderVersion(prescriptionItem).version)) +
                                                " must be one of: " + std::move(supportedProfiles).str());
    }
    catch (...)
    {
        ADD_FAILURE() << "Unexpected Exception";
    }
}

TEST_F(ActivateTaskTest, ERP15117_begrenzungDateEnd)
{
    std::string_view kvnr{"X123456788"};
    const auto KBV_ERP_current = ResourceTemplates::Versions::KBV_ERP_current();
    auto task = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .kvnr = kvnr});
    auto kbvBundle = ResourceTemplates::kbvBundleMvoXml({.redeemPeriodEnd = "2021-01-02T23:59:59.999+01:00"});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, kbvBundle, kvnr,
                          {.expectedStatus = HttpStatus::BadRequest, .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    if (KBV_ERP_current < ResourceTemplates::Versions::KBV_ERP_1_3_3)
    {
        EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
            std::rethrow_exception(exception),
            "Bundle.entry[1].resource{MedicationRequest}.extension[3].extension[2].valuePeriod: error: "
            "-erp-begrenzungDateEnd: Begrenzung der Datumsangabe auf 10 Zeichen JJJJ-MM-TT (from profile: "
            "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription:Zeitraum:valuePeriod|1.1.0); "
            "Bundle.entry[1].resource{MedicationRequest}.extension[3].extension[2].valuePeriod: error: "
            "-erp-begrenzungDateEnd: Begrenzung der Datumsangabe auf 10 Zeichen JJJJ-MM-TT (from profile: "
            "https://fhir.kbv.de/StructureDefinition/"
            "KBV_PR_ERP_Prescription:Mehrfachverordnung:Zeitraum:valuePeriod|1.1.0); "
            "Bundle.entry[1].resource{MedicationRequest}.extension[3].extension[2].valuePeriod: error: per-1: If present, "
            "start SHALL have a lower value than end (from profile: "
            "http://hl7.org/fhir/StructureDefinition/Period|4.0.1); ");
    }
    else
    {
        auto ver = to_string(KBV_ERP_current);
        EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
            std::rethrow_exception(exception),
            // clang-format off
            "Bundle.entry[1].resource{MedicationRequest}.extension[2]: error: "
                "-erp-multiplePrescriptionEinloesefristEnde: Das Ende der Einlösefrist darf nicht vor dem Beginn der Einlösefrist liegen. "
                    "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription|" + ver + "); "
            "Bundle.entry[1].resource{MedicationRequest}.extension[2]: error: "
                "-erp-multiplePrescriptionEinloesefristEnde: Das Ende der Einlösefrist darf nicht vor dem Beginn der Einlösefrist liegen. "
                    "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Prescription:Mehrfachverordnung|" + ver + "); "
            "Bundle.entry[1].resource{MedicationRequest}.extension[2].extension[2].valuePeriod: error: "
                "-erp-begrenzungDateEnd: Begrenzung der Datumsangabe auf 10 Zeichen JJJJ-MM-TT "
                    "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription:Zeitraum:valuePeriod|" + ver + "); "
            "Bundle.entry[1].resource{MedicationRequest}.extension[2].extension[2].valuePeriod: error: "
                "-erp-begrenzungDateEnd: Begrenzung der Datumsangabe auf 10 Zeichen JJJJ-MM-TT "
                    "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Prescription:Mehrfachverordnung:Zeitraum:valuePeriod|" + ver + "); "
            "Bundle.entry[1].resource{MedicationRequest}.extension[2].extension[2].valuePeriod: error: "
                "per-1: If present, start SHALL have a lower value than end "
                    "(from profile: http://hl7.org/fhir/StructureDefinition/Period|4.0.1); "
            // clang-format on
        );
    }
}

TEST_F(ActivateTaskTest, ERP15117_begrenzungDateStart)
{
    std::string_view kvnr{"X123456788"};
    const auto KBV_ERP_current = ResourceTemplates::Versions::KBV_ERP_current();
    auto task = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .kvnr = kvnr});
    auto kbvBundle = ResourceTemplates::kbvBundleMvoXml({.redeemPeriodStart = "2021-01-02T23:59:59.999+01:00"});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, kbvBundle, kvnr,
                          {.expectedStatus = HttpStatus::BadRequest, .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    if (KBV_ERP_current < ResourceTemplates::Versions::KBV_ERP_1_3_3)
    {
        EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
            std::rethrow_exception(exception),
            "Bundle.entry[1].resource{MedicationRequest}.extension[3].extension[2].valuePeriod: error: "
            "-erp-begrenzungDateStart: Begrenzung der Datumsangabe auf 10 Zeichen JJJJ-MM-TT (from profile: "
            "https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription:Zeitraum:valuePeriod|1.1.0); "
            "Bundle.entry[1].resource{MedicationRequest}.extension[3].extension[2].valuePeriod: error: "
            "-erp-begrenzungDateStart: Begrenzung der Datumsangabe auf 10 Zeichen JJJJ-MM-TT (from profile: "
            "https://fhir.kbv.de/StructureDefinition/"
            "KBV_PR_ERP_Prescription:Mehrfachverordnung:Zeitraum:valuePeriod|1.1.0); ");
    }
    else
    {
        auto ver = to_string(KBV_ERP_current);
        EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
            std::rethrow_exception(exception),
            // clang-format off
                "Bundle: error: "
                    "-erp-multiplePrescriptionEinloesefristBeginn: Der Beginn der Einlösefrist darf nicht vor dem Ausstellungsdatum liegen. "
                        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|" + ver + "); "
                "Bundle.entry[1].resource{MedicationRequest}.extension[2].extension[2].valuePeriod: error: "
                    "-erp-begrenzungDateStart: Begrenzung der Datumsangabe auf 10 Zeichen JJJJ-MM-TT "
                        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription:Zeitraum:valuePeriod|" + ver + "); "
                "Bundle.entry[1].resource{MedicationRequest}.extension[2].extension[2].valuePeriod: error: "
                    "-erp-begrenzungDateStart: Begrenzung der Datumsangabe auf 10 Zeichen JJJJ-MM-TT "
                        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Prescription:Mehrfachverordnung:Zeitraum:valuePeriod|" + ver + "); "
            // clang-format on
        );
    }
}

TEST_F(ActivateTaskTest, ERP20812_acceptUpperCase_ID)
{
    std::string_view kvnr{"X234567891"};
    auto task = ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .kvnr = kvnr});
    auto mvoStart = model::Timestamp::now().toGermanDate();
    //auto signingTime = model::Timestamp::now();
    auto kbvBundle = ResourceTemplates::kbvBundleMvoXml(
        {.redeemPeriodStart = mvoStart, .redeemPeriodEnd = mvoStart, .mvoId = "urn:uuid:3C624210-0211-42F0-AFF2-E2814980E154"});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, task, kbvBundle, kvnr,
                          {.expectedStatus = HttpStatus::OK, .outExceptionPtr = exception}));
    ASSERT_FALSE(exception);
}

TEST_F(ActivateTaskTest, failInvalidKvnr)
{
    A_23890.test("Test invalid KVNR");
    const model::Kvnr invalidKvnr{"B123457890"};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml =
        ResourceTemplates::kbvBundleXml({.prescriptionId = taskId, .authoredOn = signingTime, .kvnr = invalidKvnr.id()});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, invalidKvnr.id(),
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        std::rethrow_exception(exception), HttpStatus::BadRequest,
        "Ungültige Versichertennummer (KVNR): Die übergebene Versichertennummer des Patienten entspricht nicht den "
        "Prüfziffer-Validierungsregeln.");
}

TEST_F(ActivateTaskTest, failInvalidIK)
{
    A_23888.test("Test invalid IK");
    const model::Kvnr kvnr{"B123457897"};
    const model::Iknr iknr{"109911214"};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml(
        {.prescriptionId = taskId, .authoredOn = signingTime, .kvnr = kvnr.id(), .iknr = iknr});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, kvnr.id(),
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        std::rethrow_exception(exception), HttpStatus::BadRequest,
        "Ungültiges Institutionskennzeichen (IKNR): Das übergebene Institutionskennzeichen im Versicherungsstatus "
        "entspricht nicht den Prüfziffer-Validierungsregeln.");
}

TEST_F(ActivateTaskTest, validWithoutIK)
{
    A_23888.test("Test valid case without IK");
    const model::Kvnr kvnr{"B123457897"};
    const model::Iknr iknr{""};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
        .prescriptionId = taskId, .authoredOn = signingTime, .kvnr = kvnr.id(),
        .coverageInsuranceType = "SEL", .iknr = iknr });
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, kvnr.id(), {.signingTime = signingTime}));
}

TEST_F(ActivateTaskTest, failInvalidAlternativeIK)
{
    static constexpr char medicationRequestExtension[] = R"(
<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Accident">
    <extension url="Unfallkennzeichen">
        <valueCoding>
            <system value="https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"/>
            <code value="4"/>
        </valueCoding>
    </extension>
</extension>
)";
    A_24030.test("Invalid IKNR in coverage extension");
    const model::Kvnr kvnr{"B123457897"};
    const model::Iknr iknr{"109911214"};
    const auto extensionCoverage =
        R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK"><valueIdentifier><system value=")" +
        std::string{iknr.namingSystem()} + R"(" /><value value=")" + iknr.id() +
        R"(" /></valueIdentifier></extension>)";
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
        .prescriptionId = taskId,
        .authoredOn = signingTime,
        .kvnr = kvnr.id(),
        .coverageInsuranceSystem = "https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Payor_Type_KBV",
        .coverageInsuranceType = "UK",
        .coveragePayorExtension = extensionCoverage,
        .medicationRequestExtension = medicationRequestExtension,
        .statusCoPayment = "1",
    });
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, kvnr.id(),
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    TVLOG(1) << kbvBundleXml;
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        std::rethrow_exception(exception), HttpStatus::BadRequest,
        "Ungültiges Institutionskennzeichen (IKNR): Das übergebene Institutionskennzeichen des Kostenträgers "
        "entspricht nicht den Prüfziffer-Validierungsregeln.");
}

TEST_F(ActivateTaskTest, failInvalidANR)
{
    A_24032.test("Test invalid ANR - fail");
    const model::Kvnr kvnr{"B123457897"};
    EnvironmentVariableGuard failLanrConfig{ConfigurationKey::SERVICE_TASK_ACTIVATE_ANR_VALIDATION_MODE, "error"};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    for (const auto type : {model::Lanr::Type::lanr, model::Lanr::Type::zanr})
    {
        const model::Lanr lanr{"444444300", type};
        const auto kbvBundleXml = ResourceTemplates::kbvBundleXml(
            {.prescriptionId = taskId, .authoredOn = signingTime, .kvnr = kvnr.id(), .lanr = lanr});
        const auto taskJson =
            ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

        std::exception_ptr exception;
        ASSERT_NO_FATAL_FAILURE(checkActivateTask(
            mServiceContext, taskJson, kbvBundleXml, kvnr.id(),
            {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
        EXPECT_ERP_EXCEPTION_WITH_MESSAGE(std::rethrow_exception(exception), HttpStatus::BadRequest,
                                          "Ungültige Arztnummer (LANR oder ZANR): Die übergebene Arztnummer entspricht "
                                          "nicht den Prüfziffer-Validierungsregeln.");
    }
}

TEST_F(ActivateTaskTest, warnInvalidANR)
{
    A_24033.test("Test invalid ANR - warning");
    const model::Kvnr kvnr{"B123457897"};
    EnvironmentVariableGuard failLanrConfig{ConfigurationKey::SERVICE_TASK_ACTIVATE_ANR_VALIDATION_MODE, "warning"};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    for (const auto type : {model::Lanr::Type::lanr, model::Lanr::Type::zanr})
    {
        const model::Lanr lanr{"444444300", type};

        const auto kbvBundleXml = ResourceTemplates::kbvBundleXml(
            {.prescriptionId = taskId, .authoredOn = signingTime, .kvnr = kvnr.id(), .lanr = lanr});
        const auto taskJson =
            ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
        Header responseHeader;
        ASSERT_NO_FATAL_FAILURE(checkActivateTask(
            mServiceContext, taskJson, kbvBundleXml, kvnr.id(),
            {.expectedStatus = HttpStatus::OkWarnInvalidAnr, .signingTime = signingTime, .outHeader = responseHeader}));
        ASSERT_TRUE(responseHeader.header(Header::Warning).has_value());
        ASSERT_EQ(responseHeader.header(Header::Warning).value(),
                  "252 erp-server \"Ungültige Arztnummer (LANR oder ZANR): Die übergebene Arztnummer entspricht nicht "
                  "den Prüfziffer-Validierungsregeln.\"");
    }
}

TEST_F(ActivateTaskTest, failInvalidPZNChecksum)
{
    A_23892.test("pzn checksum must be correct");
    const model::Kvnr kvnr{"B123457897"};
    const model::Pzn pzn{"27580891"};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
        .prescriptionId = taskId,
        .authoredOn = signingTime,
        .kvnr = kvnr.id(),
        .medicationOptions{
            .version = ResourceTemplates::Versions::KBV_ERP_current(),
            .pzn = pzn,
        },
    });
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, kvnr.id(),
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(std::rethrow_exception(exception), HttpStatus::BadRequest,
                                      "Ungültige PZN: Die übergebene Pharmazentralnummer entspricht nicht den "
                                      "vorgeschriebenen Prüfziffer-Validierungsregeln.");
}

TEST_F(ActivateTaskTest, failInvalidPZNFormat)
{
    const auto kbvVersion =  ResourceTemplates::Versions::KBV_ERP_current();
    const model::Kvnr kvnr{"B123457897"};
    const model::Pzn shortPZN{"2750891"};
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto kbvBundleXml = ResourceTemplates::kbvBundleXml({
        .prescriptionId = taskId,
        .authoredOn = signingTime,
        .kvnr = kvnr.id(),
        .medicationOptions{
            .version = kbvVersion,
            .pzn = shortPZN,
        },
    });
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, kvnr.id(),
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    std::string suffix =(kbvVersion >= ResourceTemplates::Versions::KBV_ERP_1_3_3)?"pzn|" + to_string(kbvVersion):"pznCode|1.1.0";
    EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
        std::rethrow_exception(exception),
        "Bundle.entry[4].resource{Medication}.code.coding[0]: "
        "error: -erp-begrenzungPznCode: Der PZN-Code muss aus genau 8 Zeichen bestehen. "
        "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN:" + suffix + "); ");
}

TEST_F(ActivateTaskTest, ERP17605CrashMissingIngredientArray)
{
    auto version = ResourceTemplates::Versions::KBV_ERP_current();
    auto renderVersion = version.renderVersion();
    auto kbvBundleXml = ResourceManager::instance().getStringResource(
        "test/issues/ERP-17605/Bundle_invalid_MedicationCompounding_missing_ingredient_array_" + renderVersion +".xml");
    const auto taskId =
        model::PrescriptionId::fromString("160.100.000.000.051.83");
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = taskId});

    auto now = model::Timestamp::now();
    kbvBundleXml = String::replaceAll(kbvBundleXml, "2023-12-20", now.toGermanDate());

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, "H030170228",
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = model::Timestamp::fromGermanDate("2022-05-20"),
        .insertTask=true, .outExceptionPtr = exception}));
    EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
        std::rethrow_exception(exception),
        "Bundle.entry[2].resource{Medication}.ingredient[*]: error: missing mandatory element (from profile: "
        "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_Compounding|" + to_string(version) + "); ");
}

class DigaNoAlternativeIdAllowed : public ActivateTaskTest,
                                   public testing::WithParamInterface<std::pair<std::string, std::string>>
{
};
TEST_P(DigaNoAlternativeIdAllowed, test)
{
    A_26372.test("DigaNoAlternativeIdAllowed");
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6001);
    auto evdgaBundleXml = ResourceTemplates::evdgaBundleXml({.prescriptionId = taskId.toString(),
                                                             .kvnr = "X234567891",
                                                             .coverageSystem = GetParam().first,
                                                             .coverageCode = GetParam().second,
                                                             .authoredOn = signingTime.toGermanDate()});

    const auto extensionCoverage =
        R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Alternative_IK"><valueIdentifier><system value=")" +
        std::string{model::resource::naming_system::argeIknr} +
        R"(" /><value value="121191241" /></valueIdentifier></extension>)";

    // inject extension before payor.identifier.system:
    const auto injectBefore = R"(<system value="http://fhir.de/sid/arge-ik/iknr" />)";
    evdgaBundleXml = String::replaceAll(evdgaBundleXml, injectBefore, extensionCoverage + injectBefore);
    ASSERT_NE(evdgaBundleXml.find("KBV_EX_FOR_Alternative_IK"), std::string::npos) << "extension injection failed!";

    std::string accidentExt =
        R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Accident"><extension url="Unfallkennzeichen"><valueCoding><system value="https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Ursache_Type"/><code value="4"/></valueCoding></extension></extension>)";
    const auto injectBefore2 = R"(<extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_EVDGA_SER">)";
    evdgaBundleXml = String::replaceAll(evdgaBundleXml, injectBefore2, accidentExt + injectBefore2);

    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, evdgaBundleXml, "X234567891",
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    A_26372.test("no Coverage.payor.identifier.extension:alternativeID allowed");
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        std::rethrow_exception(exception), HttpStatus::BadRequest,
        "Workflow nicht für Verordnungen nutzbar, die zu Lasten von Unfallkassen oder Berufsgenossenschaften gehen");
}
INSTANTIATE_TEST_SUITE_P(
    x, DigaNoAlternativeIdAllowed,
    testing::Values(std::make_pair("https://fhir.kbv.de/CodeSystem/KBV_CS_FOR_Payor_Type_KBV", "UK"),
                     std::make_pair("http://fhir.de/CodeSystem/versicherungsart-de-basis", "BG")));

TEST_F(ActivateTaskTest, DigaE16D)
{
    A_25991.test("e16D");
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6001);
    auto evdgaBundleXml = ResourceTemplates::evdgaBundleXml(
        {.prescriptionId = taskId.toString(), .kvnr = "X234567891", .authoredOn = signingTime.toGermanDate()});
    evdgaBundleXml = String::replaceAll(evdgaBundleXml, "e16D", "e16A");
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = taskId});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, evdgaBundleXml, "X234567891",
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    const auto& evdgaVersion = to_string(ResourceTemplates::Versions::KBV_EVDGA_current(signingTime));
    EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
        std::rethrow_exception(exception),
        R"(; Bundle.entry[0].resource{Composition}.type.coding[0].code: error: )"
        R"(value must match fixed value: "e16D" (but is "e16A") )"
        R"((from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_EVDGA_Composition|)" +
            evdgaVersion + ")");
}

TEST_F(ActivateTaskTest, DigaDeviceRequest)
{
    A_25991.test("DeviceRequest-Ressource");
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6001);
    auto evdgaBundleXml = ResourceTemplates::kbvBundleXml({.prescriptionId = taskId, .kvnr = "X234567891"});
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = taskId});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, evdgaBundleXml, "X234567891",
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        std::rethrow_exception(exception), HttpStatus::BadRequest,
        R"(Für diesen Workflowtypen sind nur Verordnungen für Digitale Gesundheitsanwendungen zulässig)");
}

TEST_F(ActivateTaskTest, DigaPZN)
{
    A_25992.test("PZN-checksum in KBV_PR_EVDGA_HealthAppRequest.codeCodeableConcept.coding.code");
    auto signingTime = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6001);
    auto evdgaBundleXml = ResourceTemplates::evdgaBundleXml(
        {.prescriptionId = taskId.toString(), .kvnr = "X234567891", .authoredOn = signingTime.toGermanDate()});
    evdgaBundleXml = String::replaceAll(evdgaBundleXml, "19205615", "19205616");
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = taskId});
    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, evdgaBundleXml, "X234567891",
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = signingTime, .outExceptionPtr = exception}));
    ASSERT_TRUE(exception);
    EXPECT_ERP_EXCEPTION_WITH_MESSAGE(
        std::rethrow_exception(exception), HttpStatus::BadRequest,
        R"(Ungültige PZN: Die übergebene Pharmazentralnummer entspricht nicht den vorgeschriebenen Prüfziffer-Validierungsregeln.)");
}


struct ActivateTaskTestDarreichungsformValidityParam
{
    std::string darreichungsfrom;
    std::string becomesValidInView;
    std::string previousVersion;
};

class ActivateTaskTestDarreichungsformValidity
    : public ActivateTaskTest,
      public ::testing::WithParamInterface<ActivateTaskTestDarreichungsformValidityParam>
{
};

TEST_P(ActivateTaskTestDarreichungsformValidity, DarreichungsformNotYetValid)
{
    if (!enableDarreichungsformTest())
    {
        // consider adapting this test when two darreichungsform versions are present again!
        GTEST_SKIP();
    }
    using namespace fhirtools::version_literal;
    const auto now = model::Timestamp::now();
    const auto today = date::floor<date::days>(now.toChronoTimePoint());
    const auto tomorrow = today + date::days{1};
    testutils::ShiftFhirResourceViewsGuard shiftView{GetParam().becomesValidInView, tomorrow};
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    auto kbvBundleXml = ResourceTemplates::kbvBundleXml(ResourceTemplates::KbvBundleOptions{
        .prescriptionId = taskId,
        .medicationOptions =
            {
                .version = ResourceTemplates::Versions::KBV_ERP_current(),
                .darreichungsform = GetParam().darreichungsfrom,
            },
    });

    std::exception_ptr exception;
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(
        mServiceContext, taskJson, kbvBundleXml, "X234567891",
        {.expectedStatus = HttpStatus::BadRequest, .signingTime = now, .outExceptionPtr = exception}));
    const gsl::not_null repoView = Fhir::instance().structureRepository(now).match(
        "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle", ResourceTemplates::Versions::KBV_ERP_current());
    const auto* valueSet = repoView->findValueSet(
        fhirtools::DefinitionKey{"https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM", std::nullopt});
    ASSERT_NE(valueSet, nullptr);
    ASSERT_EQ(valueSet->getVersion(), fhirtools::FhirVersion{GetParam().previousVersion}) << repoView->id();
    auto codes = fhirtools::FhirValueSetCodes::create(std::to_address(repoView), valueSet);

    ASSERT_FALSE(codes->containsCode(GetParam().darreichungsfrom,
                                        "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM")) << repoView->id();
    // Bonbons and bubble gum should be there, right?
    ASSERT_TRUE(codes->containsCode("BON", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
    ASSERT_TRUE(codes->containsCode("KGU", "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM"));
    EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(
        std::rethrow_exception(exception),
        "Bundle.entry[4].resource{Medication}.form.coding[0]: error: Code " + GetParam().darreichungsfrom +
            " is not part of CodeSystem "
            "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM (from profile: "
            "http://hl7.org/fhir/StructureDefinition/Coding|4.0.1); "
            "Bundle.entry[4].resource{Medication}.form.coding[0]: "
            "error: Code " +
            GetParam().darreichungsfrom +
            " is not part of CodeSystem "
            "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM "
            "(from profile: http://hl7.org/fhir/StructureDefinition/CodeableConcept|4.0.1); "
            "Bundle.entry[4].resource{Medication}.form.coding[0]: error: Code " +
            GetParam().darreichungsfrom +
            " is not part of CodeSystem "
            "https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM (from profile: "
            "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN|1.1.0); "
            "Bundle.entry[4].resource{Medication}.form.coding[0]: "
            "error: Code " +
            GetParam().darreichungsfrom +
            " with system https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DARREICHUNGSFORM not allowed "
            "for "
            "ValueSet https://fhir.kbv.de/ValueSet/KBV_VS_SFHIR_KBV_DARREICHUNGSFORM|" + GetParam().previousVersion + ", allowed are " +
            codes->codesToString() +
            " (from profile: "
            "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Medication_PZN:kbvDarreichungsform|1.1.0); ");
}

TEST_P(ActivateTaskTestDarreichungsformValidity, DarreichungsformValid)
{
    auto today = date::floor<date::days>(std::chrono::system_clock::now());
    testutils::ShiftFhirResourceViewsGuard shiftView{GetParam().becomesValidInView, today};

    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    auto kbvBundleXml = ResourceTemplates::kbvBundleXml(ResourceTemplates::KbvBundleOptions{
        .prescriptionId = taskId,
        .medicationOptions =
            {
                .version = ResourceTemplates::Versions::KBV_ERP_current(),
                .darreichungsform = GetParam().darreichungsfrom,
            },
    });

    ASSERT_NO_FATAL_FAILURE(
        checkActivateTask(mServiceContext, taskJson, kbvBundleXml, "X234567891",
                          {.expectedStatus = HttpStatus::OK, .signingTime = model::Timestamp::now()}));
}

INSTANTIATE_TEST_SUITE_P(darreichnungsformen, ActivateTaskTestDarreichungsformValidity,
                         ::testing::ValuesIn(std::list<ActivateTaskTestDarreichungsformValidityParam>{
                             {.darreichungsfrom = "LYE", .becomesValidInView = "KBV_2023-07-01", .previousVersion = "1.14"},
                             {.darreichungsfrom = "PUE", .becomesValidInView = "KBV_2023-07-01", .previousVersion = "1.14"},
                         }),
                         [](const auto& p) {
                             return p.param.darreichungsfrom;
                         });

namespace
{
struct Parameters {
    std::string_view testName;
    std::string_view signingTime;
    std::string_view acceptDate;
    std::string_view expiryDate;
    std::string_view acceptDatePkv;
    std::string_view acceptDate166;
};


using ParameterTuple = std::tuple<Parameters, model::PrescriptionId>;

std::ostream& operator<<(std::ostream& out, const Parameters& params)
{
    out << params.testName;
    return out;
}
}// namespace

class ProzessParameterFlowtype : public ActivateTaskTest, public testing::WithParamInterface<ParameterTuple>
{
public:
    void SetUp() override
    {
        if (isTRezept(std::get<model::PrescriptionId>(GetParam()).type()) &&
            ResourceTemplates::Versions::KBV_ERP_current() < ResourceTemplates::Versions::KBV_ERP_1_4_0)
        {
            GTEST_SKIP_("Test requires KBV 1.4.0 or higher");
        }
    }
    static std::string name(const testing::TestParamInfo<ParameterTuple>& info)
    {
        return boost::replace_all_copy(std::string{std::get<Parameters>(info.param).testName} + "_" +
               std::get<model::PrescriptionId>(info.param).toString(), ".", "_");
    }
};


TEST_P(ProzessParameterFlowtype, samples)
{
    const auto* kvnr = "X234567891";
    testutils::ShiftFhirResourceViewsGuard shiftView(
        "KBV_1_4", date::year_month_day{date::year(2020), date::month(02), date::day(01)});

    A_27844.test("Prozessparameter - Flowtype 160");
    A_27845.test("Prozessparameter - Flowtype 162");
    A_27846.test("Prozessparameter - Flowtype 166");
    A_27847.test("Prozessparameter - Flowtype 169");
    A_27848.test("Prozessparameter - Flowtype 200");
    A_27849.test("Prozessparameter - Flowtype 209");
    //using namespace model::resource;
    const auto& [params, prescriptionId] = GetParam();
    auto signingTime = model::Timestamp::fromGermanDate(std::string{params.signingTime});
    auto acceptDate = model::Timestamp::fromGermanDate(std::string{params.acceptDate});
    auto expiryDate = model::Timestamp::fromGermanDate(std::string{params.expiryDate});

    ResourceTemplates::KbvBundleOptions options{
        .prescriptionId = prescriptionId,
        .authoredOn = signingTime,
        .kvnr = kvnr,
        .kbvVersion = ResourceTemplates::Versions::KBV_ERP_current(signingTime)};
    switch (prescriptionId.type())
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
        case model::PrescriptionType::direkteZuweisung:
            break;
        case model::PrescriptionType::tRezept:
            options.medicationOptions.medicationCategory = "02";
            acceptDate = model::Timestamp::fromGermanDate(std::string{params.acceptDate166});
            expiryDate = acceptDate;
            break;
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
        case model::PrescriptionType::direkteZuweisungPkv:
            acceptDate = model::Timestamp::fromGermanDate(std::string{params.acceptDatePkv});
            break;
    }

    // Create Task
    const auto taskJson = ResourceTemplates::taskJson(
        {.taskType = ResourceTemplates::TaskType::Draft, .prescriptionId = prescriptionId, .timestamp = signingTime});
    std::string kbvBundleXml = ResourceTemplates::kbvBundleXml(options);
    std::string_view flowTypeDisplayName;
    switch (prescriptionId.type())
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
            flowTypeDisplayName = "Flowtype für Apothekenpflichtige Arzneimittel";
            break;
        case model::PrescriptionType::digitaleGesundheitsanwendungen:
            flowTypeDisplayName = "Flowtype für Digitale Gesundheitsanwendungen";
            kbvBundleXml = ResourceTemplates::evdgaBundleXml({
                .version = ResourceTemplates::Versions::KBV_EVDGA_current(signingTime),
                .prescriptionId = prescriptionId.toString(),
                .timestamp = signingTime.toXsDateTime(),
                .kvnr = kvnr,
                .authoredOn = signingTime.toGermanDate(),
            });
            break;
        case model::PrescriptionType::direkteZuweisung:
            flowTypeDisplayName = "Flowtype zur Workflow-Steuerung durch Leistungserbringer";
            break;
        case model::PrescriptionType::direkteZuweisungPkv:
            flowTypeDisplayName = "Flowtype zur Workflow-Steuerung durch Leistungserbringer (PKV)";
            break;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            flowTypeDisplayName = "Flowtype für Apothekenpflichtige Arzneimittel (PKV)";
            break;
        case model::PrescriptionType::tRezept:
            flowTypeDisplayName = "Flowtype für Arzneimittel nach § 3a AMVV";
            break;
    }
    ASSERT_NO_FATAL_FAILURE(checkActivateTask(mServiceContext, taskJson, kbvBundleXml, kvnr,
                                              {.signingTime = signingTime,
                                               .expectedExpiry = expiryDate,
                                               .expectedAcceptDate = acceptDate,
                                               .flowTypeDisplayName = flowTypeDisplayName}));
}


std::list<Parameters> samples = {
    {"normal"  , "2021-02-10", "2021-03-10", "2021-05-10", "2021-05-10", "2021-02-16"},
    {"leapYear", "2020-02-10", "2020-03-09", "2020-05-10", "2020-05-10", "2020-02-16"},
    {"leapDay" , "2020-02-29", "2020-03-28", "2020-05-29", "2020-05-29", "2020-03-06"}
};

INSTANTIATE_TEST_SUITE_P(
    samples, ProzessParameterFlowtype,
    testing::Combine(
        testing::ValuesIn(samples),
        testing::Values(
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713),
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::digitaleGesundheitsanwendungen, 6001),
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::tRezept, 4800),
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisung, 4718),
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, 50010),
            model::PrescriptionId::fromDatabaseId(model::PrescriptionType::direkteZuweisungPkv, 50011))),
            &ProzessParameterFlowtype::name);

struct ActivateTaskTestPatchParam {
    std::string testling;
    std::string expectedError;
};
std::ostream& operator<<(std::ostream& os, const ActivateTaskTestPatchParam& p)
{
    os << p.testling;
    return os;
}
class ActivateTaskTestPatchTest : public ActivateTaskTest,
                                  public ::testing::WithParamInterface<ActivateTaskTestPatchParam>
{
};

TEST_P(ActivateTaskTestPatchTest, ValidateAfterPatch)
{
    testutils::ShiftFhirResourceViewsGuard guard{
        "KBV_2023-07-01", std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now())};
    auto kbvBundleXml = ResourceManager::instance().getStringResource(
        "test/validation/xml/v_2023_07_01/kbv/bundle_1_1_2/" + GetParam().testling);

    const auto tomorrow = (model::Timestamp::now() + date::days{1}).toXsDate(model::Timestamp::GermanTimezone);
    kbvBundleXml = String::replaceAll(kbvBundleXml, "<authoredOn value=\"2022-05-20\" />",
                                      "<authoredOn value=\"" + tomorrow + "\" />");
    auto signingTime = model::Timestamp::fromGermanDate(tomorrow);
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Draft,
                                     .prescriptionId = model::PrescriptionId::fromString("160.100.000.000.004.30"),
                                     .timestamp = signingTime});
    std::exception_ptr exceptionPtr;
    checkActivateTask(mServiceContext, taskJson, kbvBundleXml, "K220635158",
                      {.expectedStatus = HttpStatus::BadRequest, .insertTask = true, .outExceptionPtr = exceptionPtr});

    EXPECT_ERP_EXCEPTION_WITH_FHIR_VALIDATION_ERROR(std::rethrow_exception(exceptionPtr), GetParam().expectedError);
}

INSTANTIATE_TEST_SUITE_P(
    x, ActivateTaskTestPatchTest,
    ::testing::Values(
        ActivateTaskTestPatchParam{
            .testling = "Bundle_invalid_Beispiel_6_URN_-erp-angabeIdentifikatorVerantwortlichePerson_Negativ.xml",
            .expectedError =
                "Bundle: error: -erp-angabeIdentifikatorVerantwortlichePerson: In der Ressource vom Typ Practitioner "
                "ist "
                "der Identifikator der verantwortlichen Person nicht vorhanden, dieser ist aber eine Pflichtangabe bei "
                "den Kostentraegern der Typen \"GKV\", \"BG\", \"SKT\" oder \"UK\", wenn es sich um einen Arzt, "
                "Zahnarzt "
                "oder Arzt als Vertreter handelt und keine ASV-Fachgruppennummer angegeben ist. (from profile: "
                "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0); "},
        ActivateTaskTestPatchParam{
            .testling =
                "Bundle_invalid_Beispiel_60_URN_-erp-angabeFachgruppennummerAsvAusstellendePersonVerbot_Negativ.xml",
            .expectedError =
                "Bundle: error: -erp-angabeFachgruppennummerAsvAusstellendePersonVerbot: In einer Ressource vom Typ "
                "Practitioner ist eine ASV-Fachgruppennummer der ausstellenden Person vorhanden, diese darf aber nur "
                "angegeben werden, wenn die Rechtsgrundlage den Wert \"01\" oder \"11\" besitzt und wenn es sich um "
                "einen Arzt oder Arzt als Vertreter handelt, f\xC3\xBCr den kein Identifikator angegeben ist. (from "
                "profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0); "},
        ActivateTaskTestPatchParam{
            .testling = "Bundle_invalid_Beispiel_6_URN_-erp-angabeVerantwortlichePersonVerbot-2_Negativ.xml",
            .expectedError = "Bundle: error: -erp-angabeVerantwortlichePersonVerbot-2: Eine Ressource vom Typ "
                             "Practitioner wird als verantwortliche Person angegeben, diese darf aber nur angegeben "
                             "werden, wenn es sich nicht um eine Hebamme oder einen Arzt in Weiterbildung handelt. "
                             "(from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0); "},
        ActivateTaskTestPatchParam{
            .testling =
                "Bundle_invalid_Beispiel_60_URN_-erp-angabeFachgruppennummerAsvVerantwortlichePersonVerbot_Negativ.xml",
            .expectedError =
                "Bundle: error: -erp-angabeFachgruppennummerAsvVerantwortlichePersonVerbot: In einer Ressource vom Typ "
                "Practitioner ist eine ASV-Fachgruppennummer der verantwortlichen Person vorhanden, diese darf aber "
                "nur angegeben werden, wenn die Rechtsgrundlage den Wert \"01\" oder \"11\" besitzt und wenn es sich "
                "um einen Arzt oder Arzt als Vertreter handelt, f\xC3\xBCr den kein Identifikator angegeben ist. (from "
                "profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0); "},
        ActivateTaskTestPatchParam{
            .testling = "Bundle_invalid_Beispiel_6_URN_-erp-angabeVerantwortlichePersonVerbot-1_Negativ_1.xml",
            .expectedError =
                "Bundle: error: -erp-angabeVerantwortlichePersonVerbot-1: Eine Ressource vom Typ Practitioner wird als "
                "verantwortliche Person angegeben, diese darf aber nur angegeben werden, wenn es sich bei der "
                "ausstellenden Person um einen Arzt in Weiterbildung oder Arzt als Vertreter handelt. (from profile: "
                "https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0); "},
        ActivateTaskTestPatchParam{
            .testling = "Bundle_invalid_Beispiel_1_URN_-erp-angabeIdentifikatorAusstellendePerson_Negativ.xml",
            .expectedError =
                "Bundle: error: -erp-angabeIdentifikatorAusstellendePerson: In der Ressource vom Typ Practitioner ist "
                "der Identifikator der ausstellenden oder verschreibenden Person nicht vorhanden, dieser ist aber eine "
                "Pflichtangabe bei den Kostentraegern der Typen \"GKV\", \"BG\", \"SKT\", \"UK\" oder \"PKV\", wenn es "
                "sich um einen Arzt, Zahnarzt oder Arzt als Vertreter handelt und keine ASV-Fachgruppennummer "
                "angegeben ist. (from profile: https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.1.0); "}));


struct ActivateTaskUuidValidationTestParameters {
    std::string urnUuidCheckFrom;
    std::string fullUrl;
    bool expectSuccess;
};

void PrintTo(const ActivateTaskUuidValidationTestParameters& parameters, std::ostream* os)
{
    (*os) << '{' << R"("urnUuidCheckFrom": ")" << parameters.urnUuidCheckFrom << R"(" )"
          << R"("fullUrl": ")" << parameters.fullUrl << R"(" )"
          << R"("expectSuccess": )" << std::boolalpha << parameters.expectSuccess << '}';
}


class ActivateTaskUuidValidationTest : public ActivateTaskTest,
                                       public testing::WithParamInterface<ActivateTaskUuidValidationTestParameters>
{
public:
    static std::list<ActivateTaskUuidValidationTestParameters> parameters();
};

TEST_P(ActivateTaskUuidValidationTest, run)
{
    using namespace model::resource::elements;
    using model::resource::ElementName;
    testutils::ShiftFhirResourceViewsGuard noShift(testutils::ShiftFhirResourceViewsGuard::asConfigured);
    EnvironmentVariableGuard guard{ConfigurationKey::FHIR_VALIDATION_URN_UUID_CHECK_FROM, GetParam().urnUuidCheckFrom};
    static rapidjson::Pointer fullUrlPtr{ElementName::path(entry, 1, fullUrl)};
    static rapidjson::Pointer subjectReferencePtr{ElementName::path(entry, 0, resource, subject, reference)};
    const auto& converter = Fhir::instance().converter();

    auto authoredOn = model::Timestamp::now();
    const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
    auto kbvBundle = model::UnspecifiedResource::fromXmlNoValidation(
                         ResourceTemplates::kbvBundleXml({.prescriptionId = taskId, .authoredOn = authoredOn}))
                         .jsonDocument();
    kbvBundle.setValue(fullUrlPtr, GetParam().fullUrl);
    kbvBundle.setValue(subjectReferencePtr, GetParam().fullUrl);
    const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
    ActivateTaskHandler handler({});
    auto request = serverRequest(taskJson, converter.jsonToXmlString(kbvBundle), authoredOn);
    request.setAccessToken(mJwtBuilder->makeJwtArzt());
    ServerResponse serverResponse;
    AccessLog accessLog;
    SessionContext sessionContext{mServiceContext, request, serverResponse, accessLog};
    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    if (GetParam().expectSuccess)
    {
        ASSERT_NO_THROW(handler.handleRequest(sessionContext)) << kbvBundle.serializeToJsonString();
    }
    else
    {
        EXPECT_ERP_EXCEPTION_WITH_DIAGNOSTICS(
            handler.handleRequest(sessionContext), HttpStatus::BadRequest, "FHIR-Validation error",
            "Bundle.entry[1].fullUrl: error: value is not a valid lower-case urn:uuid.; ");
    }
}

std::list<ActivateTaskUuidValidationTestParameters> ActivateTaskUuidValidationTest::parameters()
{
    using namespace std::chrono_literals;
    auto now = date::make_zoned(model::Timestamp::GermanTimezone, std::chrono::system_clock::now());
    model::Timestamp today{model::Timestamp::GermanTimezone, date::floor<std::chrono::days>(now.get_local_time())};
    model::Timestamp yesterday{model::Timestamp::GermanTimezone,
                               date::floor<std::chrono::days>(now.get_local_time() - 24h)};
    model::Timestamp tomorrow{model::Timestamp::GermanTimezone,
                              date::floor<std::chrono::days>(now.get_local_time() + 24h)};
    return {
        {yesterday.toGermanDate(), "urn:uuid:16fa9dd1-a702-4627-8405-cd22f01a09c7", true},
        {today.toGermanDate(), "urn:uuid:16fa9dd1-a702-4627-8405-cd22f01a09c7", true},
        {tomorrow.toGermanDate(), "urn:uuid:16fa9dd1-a702-4627-8405-cd22f01a09c7", true},
        {yesterday.toGermanDate(), "urn:uuid:16FA9DD1-A702-4627-8405-CD22F01A09C7", false},
        {today.toGermanDate(), "urn:uuid:16FA9DD1-A702-4627-8405-CD22F01A09C7", false},
        {tomorrow.toGermanDate(), "urn:uuid:16FA9DD1-A702-4627-8405-CD22F01A09C7", true},
    };
}

INSTANTIATE_TEST_SUITE_P(parameters, ActivateTaskUuidValidationTest,
                         testing::ValuesIn(ActivateTaskUuidValidationTest::parameters()));


struct ActivateTaskFullUrlTestParam {
    std::string name;
    std::optional<std::string> fullUrl;
    std::optional<std::string> resourceId;
    std::string message{};
    std::string diagnostics{};
};

class ActivateTaskFullUrlTest : public ActivateTaskTest, public ::testing::WithParamInterface<ActivateTaskFullUrlTestParam>
{
public:
    SessionContext prepareCreateTask()
    {
        using namespace model::resource::elements;
        using model::resource::ElementName;
        static const ElementName section{"section"};
        static const ElementName beneficiary{"beneficiary"};
        static rapidjson::Pointer fullUrlPtr{ElementName::path(entry, 1, fullUrl)};
        static rapidjson::Pointer resourceIdPtr{ElementName::path(entry, 1, resource, id)};
        static rapidjson::Pointer coverageBeneficiaryPtr{ElementName::path(entry, 5, resource, beneficiary)};
        static rapidjson::Pointer coverageBeneficiaryRefPtr{ElementName::path(entry, 5, resource, beneficiary, reference)};
        struct Entry {
            int index;
            rapidjson::Pointer subjectPtr{ElementName::path(entry, index, resource, subject)};
            rapidjson::Pointer subjectRefPtr{ElementName::path(entry, index, resource, subject, reference)};
        };
        static const Entry composition{0};
        static const Entry medicationRequest{6};

        const auto& converter = Fhir::instance().converter();
        auto authoredOn = model::Timestamp::now();
        const auto taskId =
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel, 4713);
        auto kbvBundle = model::UnspecifiedResource::fromXmlNoValidation(
            ResourceTemplates::kbvBundleXml({.prescriptionId = taskId, .authoredOn = authoredOn}))
        .jsonDocument();
        if (GetParam().fullUrl)
        {
            kbvBundle.setValue(fullUrlPtr, *GetParam().fullUrl);
            kbvBundle.setValue(composition.subjectRefPtr, *GetParam().fullUrl);
            kbvBundle.setValue(medicationRequest.subjectRefPtr, *GetParam().fullUrl);
            kbvBundle.setValue(coverageBeneficiaryRefPtr, *GetParam().fullUrl);
        }
        else
        {
            fullUrlPtr.Erase(kbvBundle);
            composition.subjectPtr.Erase(kbvBundle);
            medicationRequest.subjectPtr.Erase(kbvBundle);
            coverageBeneficiaryPtr.Erase(kbvBundle);
        }
        if (GetParam().resourceId)
        {
            kbvBundle.setValue(resourceIdPtr, *GetParam().resourceId);
        }
        else
        {
            resourceIdPtr.Erase(kbvBundle);
        }
        const auto taskJson =
        ResourceTemplates::taskJson({.taskType = ResourceTemplates::TaskType::Ready, .prescriptionId = taskId});
        mServerRequest = serverRequest(taskJson, converter.jsonToXmlString(kbvBundle), authoredOn);
        mServerRequest.setAccessToken(mJwtBuilder->makeJwtArzt());
        return SessionContext{mServiceContext, mServerRequest, mServerResponse, mAccessLog};
    }

    static const std::string& name(const testing::TestParamInfo<ActivateTaskFullUrlTestParam>& p)
    {
        return p.param.name;
    }

    testutils::ShiftFhirResourceViewsGuard noShift{testutils::ShiftFhirResourceViewsGuard::asConfigured};
    ServerRequest mServerRequest{{}};
    ServerResponse mServerResponse;
    AccessLog mAccessLog;
};


TEST_P(ActivateTaskFullUrlTest, checkDisabled)
{
    EnvironmentVariableGuard checkDisabled{ConfigurationKey::FHIR_VALIDATION_FULL_URL_AND_BUNDLE_REFERENCE_CHECK_FROM, "2099-01-01"};
    auto sessionContext = prepareCreateTask();
    ActivateTaskHandler handler({});
    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    try {
        handler.handleRequest(sessionContext);
    }
    catch (const ErpException& ex)
    {
        // should get a different message, when the check ist disabled
        EXPECT_NE(ex.what(), GetParam().message);
    }
    catch (const std::exception& ex)
    {
        FAIL() << "unexpected exception of type " << typeid(ex).name() << ": " << ex.what();
    }
}

TEST_P(ActivateTaskFullUrlTest, checkEnabled)
{
    EnvironmentVariableGuard checkEnabled{ConfigurationKey::FHIR_VALIDATION_FULL_URL_AND_BUNDLE_REFERENCE_CHECK_FROM, "2000-01-01"};
    auto sessionContext = prepareCreateTask();
    ActivateTaskHandler handler({});
    ASSERT_NO_THROW(handler.preHandleRequestHook(sessionContext));
    try {
        handler.handleRequest(sessionContext);
        FAIL() << "expected exception not thrown";
    }
    catch (const ErpException& ex)
    {
        EXPECT_EQ(ex.what(), GetParam().message) << mServerRequest.getBody();
    }
    catch (const std::exception& ex)
    {
        FAIL() << "unexpected exception of type " << typeid(ex).name() << ": " << ex.what();
    }
}


INSTANTIATE_TEST_SUITE_P(reject, ActivateTaskFullUrlTest,
                         ::testing::ValuesIn(std::initializer_list<ActivateTaskFullUrlTestParam>{
                             {
                                 .name = "bundledResourceMissingId",
                                 .fullUrl = "http://pvs.praxis.local/fhir/Patient/16fa9dd1-a702-4627-8405-cd22f01a09c7",
                                 .resourceId = std::nullopt,
                                 .message = "Die ID einer Ressource im Bundle ist nicht vorhanden",
                             },
                             {
                                 .name = "bundleFullUrlIdMissmatch",
                                 .fullUrl = "http://pvs.praxis.local/fhir/Patient/16fa9dd1-a702-4627-8405-cd22f01a09c7",
                                 .resourceId = "Impatient",
                                 .message = "Die ID einer Ressource und die ID der zugehörigen fullUrl stimmen nicht überein.",
                             },
                             {
                                 .name = "bundleFullUrlInvalidFormat",
                                 .fullUrl = "urn:oid:0.8.15.47.11",
                                 .resourceId = "16fa9dd1-a702-4627-8405-cd22f01a09c7",
                                 .message = "Format der fullUrl ist ungültig.",
                             },
                         }),
                         &ActivateTaskFullUrlTest::name);
