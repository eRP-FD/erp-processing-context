/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include "erp/erp-serverinfo.hxx"
#include "erp/beast/BoostBeastStringWriter.hxx"
#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "test/erp/pc/CFdSigErpTestHelper.hxx"
#include "test/util/CertificateDirLoader.h"
#include "test/util/CryptoHelper.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestConfiguration.hxx"
#include "test/util/ResourceManager.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/util/Expect.hxx"

#include <random>
#include <regex>

#undef DELETE


namespace
{

template<class TModelElem>
//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void modelElemfromString(
    std::optional<TModelElem>& result,
    const std::string_view& str,
    const std::string& contentType,
    SchemaType schemaType)
{
    if (contentType == std::string(ContentMimeType::fhirXmlUtf8))
    {
        ASSERT_NO_THROW(
            result = TModelElem::fromXml(
                str, *ErpWorkflowTestBase::getXmlValidator(), *StaticData::getInCodeValidator(), schemaType));
    }
    else
    {
        ASSERT_NO_THROW(
            result = TModelElem::fromJson(
                str, *ErpWorkflowTestBase::getJsonValidator(),
                *ErpWorkflowTestBase::getXmlValidator(), *StaticData::getInCodeValidator(), schemaType));
    }
}

}


ErpWorkflowTestBase::~ErpWorkflowTestBase() = default;

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkTaskCreate(
    std::optional<model::PrescriptionId>& createdId,
    std::string& createdAccessCode,
    model::PrescriptionType workflowType)
{
    // invoke POST /task/$create
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate(workflowType));
    ASSERT_TRUE(task);
    EXPECT_EQ(task->status(), model::Task::Status::draft);
    // extract prescriptionId and accessCode
    ASSERT_NO_THROW(createdId.emplace(task->prescriptionId()));
    ASSERT_TRUE(createdId);
    ASSERT_NO_THROW(createdAccessCode = task->accessCode());
    ASSERT_FALSE(createdAccessCode.empty());
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkTaskActivate(
    std::string& qesBundle,
    std::vector<model::Communication>& communications,
    const model::PrescriptionId& prescriptionId,
    const std::string& kvnr,
    const std::string& accessCode)
{
    // repare QES-Bundle for invokation of POST /task/<id>/$activate
    ASSERT_NO_THROW(qesBundle = std::get<0>(makeQESBundle(kvnr, prescriptionId, model::Timestamp::now())));
    ASSERT_FALSE(qesBundle.empty());
    std::optional<model::Task> task;
    // invoke /task/<id>/$activate
    ASSERT_NO_FATAL_FAILURE(task = taskActivate(prescriptionId, accessCode, qesBundle));
    ASSERT_TRUE(task);
    EXPECT_NO_THROW(EXPECT_EQ(task->prescriptionId().toString(), prescriptionId.toString()));
    ASSERT_TRUE(task->kvnr());
    EXPECT_EQ(*task->kvnr(), kvnr);
    EXPECT_EQ(task->status(), model::Task::Status::ready);

    // retrieve activated Task by invoking GET /task/<id>?ac=<accessCode>
    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(prescriptionId, kvnr, accessCode));
    ASSERT_TRUE(taskBundle);
    ASSERT_NO_FATAL_FAILURE(getTaskFromBundle(task,*taskBundle));
    ASSERT_TRUE(task);
    EXPECT_NO_THROW(EXPECT_EQ(task->prescriptionId().toString(), prescriptionId.toString()));
    EXPECT_EQ(task->status(), model::Task::Status::ready);

    auto bundleList = taskBundle->getResourcesByType<model::Bundle>("Bundle");
    ASSERT_EQ(bundleList.size(), 1);
    ASSERT_EQ(bundleList.front().getId().toString(), std::string(*task->patientConfirmationUuid()));

    const auto telematicId = jwtApotheke().stringForClaim(JWT::idNumberClaim);
    ASSERT_TRUE(telematicId.has_value());

    std::string kvnrRepresentative = "X234567890";
    ASSERT_NE(kvnr, kvnrRepresentative);

    std::optional<model::Communication> communicationResponse;
    ASSERT_NO_FATAL_FAILURE(communicationResponse = communicationPost(
        model::Communication::MessageType::InfoReq, task.value(),
        ActorRole::Insurant, kvnr,
        ActorRole::Pharmacists, telematicId.value(),
        "Ist das Medikament bei Ihnen vorr??tig?"));
    if (communicationResponse.has_value()) communications.emplace_back(std::move(communicationResponse.value()));
    ASSERT_NO_FATAL_FAILURE(communicationResponse = communicationPost(
        model::Communication::MessageType::Reply, task.value(),
        ActorRole::Pharmacists, telematicId.value(),
        ActorRole::Insurant, kvnr,
        "Wir haben das Medikament."));
    if (communicationResponse.has_value()) communications.emplace_back(std::move(communicationResponse.value()));

    // communications below not possible for WF169, because the access code is not delivered to the patient.
    if (task.value().type() == model::PrescriptionType::direkteZuweisung) return;
    ASSERT_NO_FATAL_FAILURE(communicationResponse = communicationPost(
        model::Communication::MessageType::Representative, task.value(),
        ActorRole::Insurant, kvnr,
        ActorRole::Representative, kvnrRepresentative,
        "Kannst Du das Medikament f??r mich holen?"));
    if (communicationResponse.has_value()) communications.emplace_back(std::move(communicationResponse.value()));
    ASSERT_NO_FATAL_FAILURE(communicationResponse = communicationPost(
        model::Communication::MessageType::Representative, task.value(),
        ActorRole::Representative, kvnrRepresentative,
        ActorRole::Insurant, kvnr,
        "Bin krank. Musst Du Dir schicken lassen."));
    if (communicationResponse.has_value()) communications.emplace_back(std::move(communicationResponse.value()));
    ASSERT_NO_FATAL_FAILURE(communicationResponse = communicationPost(
        model::Communication::MessageType::DispReq, task.value(),
        ActorRole::Insurant, kvnr,
        ActorRole::Pharmacists, telematicId.value(),
        "Bitte schicken Sie einen Boten."));
    if (communicationResponse.has_value()) communications.emplace_back(std::move(communicationResponse.value()));
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkTaskAccept(
    std::string& createdSecret,
    std::optional<model::Timestamp>& lastModifiedDate,
    const model::PrescriptionId& prescriptionId,
    const std::string& kvnr,
    const std::string& accessCode,
    const std::string& qesBundle,
    bool withConsent)
{
    const auto expectedResourceCount = withConsent ? 3 : 2;
    // invoke /task/<id>/$accept
    std::optional<model::Bundle> acceptResultBundle;
    ASSERT_NO_FATAL_FAILURE(acceptResultBundle = taskAccept(prescriptionId, accessCode));
    ASSERT_TRUE(acceptResultBundle);
    ASSERT_EQ(acceptResultBundle->getResourceCount(), expectedResourceCount);
    const auto tasks = acceptResultBundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    ASSERT_EQ(tasks[0].prescriptionId().toString(), prescriptionId.toString());
    ASSERT_EQ(tasks[0].status(), model::Task::Status::inprogress);
    EXPECT_FALSE(tasks[0].patientConfirmationUuid().has_value());
    const auto secret = tasks[0].secret();
    ASSERT_TRUE(secret.has_value());
    const auto binaryResources = acceptResultBundle->getResourcesByType<model::Binary>("Binary");
    ASSERT_EQ(binaryResources.size(), 1);
    ASSERT_TRUE(binaryResources[0].data().has_value());
    ASSERT_EQ(binaryResources[0].data().value(), qesBundle);
    ASSERT_EQ(std::string(*binaryResources[0].id()), std::string(*tasks[0].healthCarePrescriptionUuid()));

    // retrieve accepted Task by invoking GET /task/<id>?secret=<secret>
    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(prescriptionId, "x-dummy", std::string(*secret)));
    ASSERT_TRUE(taskBundle);
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(getTaskFromBundle(task,*taskBundle));
    ASSERT_TRUE(task);
    EXPECT_EQ(task->status(), model::Task::Status::inprogress);
    ASSERT_TRUE(task->secret().has_value());
    ASSERT_EQ(secret.value(), task->secret().value());
    createdSecret = secret.value();
    lastModifiedDate = task->lastModifiedDate();

    if(withConsent)
    {
        // Check consent:
        const auto consents = acceptResultBundle->getResourcesByType<model::Consent>("Consent");
        ASSERT_EQ(consents.size(), 1);
        EXPECT_TRUE(consents[0].isChargingConsent());
        EXPECT_EQ(consents[0].patientKvnr(), kvnr);
        EXPECT_EQ(consents[0].id(), model::Consent::createIdString(model::Consent::Type::CHARGCONS, kvnr));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkTaskClose(
    const model::PrescriptionId& prescriptionId,
    const std::string& kvnr,
    const std::string& secret,
    const model::Timestamp& lastModified,
    const std::vector<model::Communication>& communications,
    size_t numMedicationDispenses)
{
    const auto& testConfig = TestConfiguration::instance();
    const auto telematicId = jwtApotheke().stringForClaim(JWT::idNumberClaim);
    ASSERT_TRUE(telematicId.has_value());

    // The following communication messages have been sent:
    // InfoReq (Insurant -> Pharmacy), Reply (Pharmacy -> Insurant),
    // Representative (Insurant -> Representative), Representative (Representative -> Insurant)
    // DispReq(Insurant->Pharmacy)
    JWT jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    std::optional<model::Bundle> communicationsBundle;
    ASSERT_NO_FATAL_FAILURE(communicationsBundle = communicationsGet(jwtInsurant));
    ASSERT_TRUE(communicationsBundle);
    EXPECT_EQ(countTaskBasedCommunications(*communicationsBundle, prescriptionId), communications.size());

    // invoke /Task/<id>/$close
    std::optional<model::ErxReceipt> closeReceipt;
    ASSERT_NO_FATAL_FAILURE(closeReceipt =
                                taskClose(prescriptionId, secret, kvnr, HttpStatus::OK, {}, numMedicationDispenses));
    ASSERT_TRUE(closeReceipt);
    const auto compositionResources = closeReceipt->getResourcesByType<model::Composition>("Composition");
    ASSERT_EQ(compositionResources.size(), 1);
    const auto& composition = compositionResources.front();
    EXPECT_TRUE(composition.telematikId().has_value());
    EXPECT_EQ(composition.telematikId().value(), telematicId.value());
    EXPECT_TRUE(composition.periodStart().has_value());
    EXPECT_EQ(composition.periodStart().value(), lastModified);
    EXPECT_TRUE(composition.periodEnd().has_value());
    EXPECT_TRUE(composition.date().has_value());
    const auto deviceResources = closeReceipt->getResourcesByType<model::Device>("Device");
    ASSERT_EQ(deviceResources.size(), 1);
    const auto& device = deviceResources.front();
    EXPECT_EQ(std::string{device.serialNumber()}, std::string{ErpServerInfo::ReleaseVersion});
    EXPECT_EQ(std::string{device.version()}, std::string{ErpServerInfo::ReleaseVersion});
    const auto signature = closeReceipt->getSignature();
    ASSERT_TRUE(signature.has_value());
    EXPECT_TRUE(signature->when().has_value());
    EXPECT_TRUE(signature->data().has_value());
    std::string signatureData;
    ASSERT_NO_THROW(signatureData = signature->data().value().data());
    const auto& cadesBesTrustedCertDir =
    testConfig.getOptionalStringValue(TestConfigurationKey::TEST_CADESBES_TRUSTED_CERT_DIR, "test/cadesBesSignature/certificates");
    auto certs = CertificateDirLoader::loadDir(cadesBesTrustedCertDir);
    auto cms = runsInCloudEnv() ? CadesBesSignature(signatureData) : CadesBesSignature(certs, signatureData);

    // This cannot be validated using XSD, because the Signature is defined mantatory, but not contained in the receipt
    // from the signature.
    const auto receiptFromSignature = model::ErxReceipt::fromXmlNoValidation(cms.payload());
    EXPECT_FALSE(receiptFromSignature.getSignature().has_value());

    // retrieve closed Task by invoking GET /task/<id>?secret=<secret>
    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(prescriptionId, "X-dummy", secret));
    ASSERT_TRUE(taskBundle);
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(getTaskFromBundle(task,*taskBundle));
    ASSERT_TRUE(task);
    EXPECT_EQ(task->status(), model::Task::Status::completed);
    EXPECT_TRUE(task->receiptUuid().has_value());
    EXPECT_EQ(Uuid(std::string(task->receiptUuid().value())), closeReceipt->getId());

    // Check receipt bundle saved during /Task/Close
    const auto bundles = taskBundle->getResourcesByType<model::Bundle>("Bundle");
    ASSERT_EQ(bundles.size(), 1);  // 1.: Receipt bundle
    std::optional<model::ErxReceipt> receipt;
    ASSERT_NO_THROW(receipt = model::ErxReceipt::fromXml(bundles.back().serializeToXmlString(), *getXmlValidator(),
                                                         *StaticData::getInCodeValidator(),
                                                         SchemaType::Gem_erxReceiptBundle));
    ASSERT_TRUE(receipt);
    // Must be the same as the result from the service call:
    EXPECT_EQ(canonicalJson(receipt->serializeToJsonString()), canonicalJson(closeReceipt->serializeToJsonString()));

    // Check medication dispense saved during /Task/Close
    ASSERT_TRUE(task->kvnr().has_value());
    std::optional<model::MedicationDispense> medicationDispenseForTask;
    ASSERT_NO_FATAL_FAILURE(medicationDispenseGetInternal(medicationDispenseForTask, kvnr, prescriptionId.toString()));
    ASSERT_TRUE(medicationDispenseForTask.has_value());
    EXPECT_EQ(medicationDispenseForTask->telematikId(), telematicId);

    // Check deletion of task related communication objects:
    checkCommunicationsDeleted(prescriptionId, kvnr, communications);
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkTaskAbort(
    const model::PrescriptionId& prescriptionId,
    const JWT& jwt,
    const std::string& kvnr,
    const std::optional<std::string>& accessCode,
    const std::optional<std::string>& secret,
    const std::vector<model::Communication>& communications)
{
    JWT jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    std::optional<model::Bundle> communicationsBundle;
    ASSERT_NO_FATAL_FAILURE(communicationsBundle = communicationsGet(jwtInsurant));
    ASSERT_TRUE(communicationsBundle);
    EXPECT_EQ(countTaskBasedCommunications(*communicationsBundle, prescriptionId), communications.size());

    ASSERT_NO_FATAL_FAILURE(taskAbort(prescriptionId, jwt, accessCode, secret));
    // Assert that Task is cancelled and can not be retrieved by GET /task/<id>?ac=<accessCode>
    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(prescriptionId, kvnr, accessCode,
                                                   HttpStatus::Gone, model::OperationOutcome::Issue::Type::processing));
    ASSERT_FALSE(taskBundle);   // cancelled Task can not be retrieved

    // check that no MedicationDispense has been created for the kvnr.
    std::optional<model::MedicationDispense> medicationDispenseForTask;
    ASSERT_NO_FATAL_FAILURE(medicationDispenseGetInternal(medicationDispenseForTask, kvnr, prescriptionId.toString()));
    ASSERT_FALSE(medicationDispenseForTask.has_value());

    // Check deletion of task related communication objects:
    checkCommunicationsDeleted(prescriptionId, kvnr, communications);
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkTaskReject(
    const model::PrescriptionId& prescriptionId,
    const std::string& kvnr,
    const std::string& accessCode,
    const std::string& secret)
{
    // invoke /task/<id>/$reject
    ASSERT_NO_FATAL_FAILURE(taskReject(prescriptionId, secret));

    // retrieve rejected Task by invoking GET /task/<id>?ac=<accessCode>
    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(prescriptionId, kvnr, accessCode));
    ASSERT_TRUE(taskBundle);
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(getTaskFromBundle(task,*taskBundle));
    ASSERT_TRUE(task);
    // Check status and deletion of secret
    EXPECT_EQ(task->status(), model::Task::Status::ready);
    ASSERT_FALSE(task->secret().has_value());
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkCommunicationsDeleted(
    const model::PrescriptionId& prescriptionId,
    const std::string& kvnr,
    const std::vector<model::Communication>& communications)
{
    JWT jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    std::optional<model::Bundle> communicationsBundle;
    ASSERT_NO_FATAL_FAILURE(communicationsBundle = communicationsGet(jwtInsurant));
    ASSERT_TRUE(communicationsBundle);
    // TODO: the following fails because of missing "entry" array:
    //std::vector<model::Communication> communications = communicationsBundle->getResourcesByType<model::Communication>("Communication");
    EXPECT_EQ(countTaskBasedCommunications(*communicationsBundle, prescriptionId), 0);
    for (const auto& communication : communications)
    {
        JWT jwt1;
        JWT jwt2;
        switch (communication.messageType())
        {
        case model::Communication::MessageType::InfoReq:
            jwt1 = JwtBuilder::testBuilder().makeJwtVersicherter(std::string(communication.sender().value()));
            jwt2 = JwtBuilder::testBuilder().makeJwtApotheke(std::string(communication.recipient().value()));
            break;
        case model::Communication::MessageType::Reply:
            jwt1 = JwtBuilder::testBuilder().makeJwtApotheke(std::string(communication.sender().value()));
            jwt2 = JwtBuilder::testBuilder().makeJwtVersicherter(std::string(communication.recipient().value()));
            break;
        case model::Communication::MessageType::DispReq:
            jwt1 = JwtBuilder::testBuilder().makeJwtVersicherter(std::string(communication.sender().value()));
            jwt2 = JwtBuilder::testBuilder().makeJwtApotheke(std::string(communication.recipient().value()));
            break;
        case model::Communication::MessageType::Representative:
            jwt1 = JwtBuilder::testBuilder().makeJwtVersicherter(std::string(communication.sender().value()));
            jwt2 = JwtBuilder::testBuilder().makeJwtVersicherter(std::string(communication.recipient().value()));
            break;
        }
        EXPECT_FALSE(communicationGet(jwt1, communication.id().value()).has_value());
        EXPECT_FALSE(communicationGet(jwt2, communication.id().value()).has_value());
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkAuditEventsFrom(const std::string& insurantKvnr)
{
    std::optional<model::Bundle> auditEventBundle;
    ASSERT_NO_FATAL_FAILURE(auditEventBundle = auditEventGet(insurantKvnr, "de", ""));
    ASSERT_TRUE(auditEventBundle);
    const auto auditEvents = auditEventBundle->getResourcesByType<model::AuditEvent>("AuditEvent");
    EXPECT_FALSE(auditEvents.empty());
    for(const auto& auditEvent : auditEvents)
        EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkAuditEvents(
    std::vector<std::optional<model::PrescriptionId>> prescriptionIds,
    const std::string& insurantKvnr,
    const std::string& language,
    const model::Timestamp& startTime,
    const std::vector<std::string>& actorIdentifiers,
    const std::unordered_set<std::size_t>& actorTelematicIdIndices,
    const std::vector<model::AuditEvent::SubType>& expectedActions)
{
    Expect3(actorIdentifiers.size() == expectedActions.size(), "Invalid parameters", std::logic_error);

    if(prescriptionIds.size() == 1 && !actorIdentifiers.empty())
    {
        // if only a single prescription id is provided it is used for all elements
        const auto id = prescriptionIds[0];
        prescriptionIds.reserve(actorIdentifiers.size());
        for(std::size_t i = 1; i < actorIdentifiers.size(); ++i)
            prescriptionIds.emplace_back(id);
    }
    Expect3(prescriptionIds.size() == actorIdentifiers.size(), "Invalid parameters", std::logic_error);

    std::optional<model::Bundle> auditEventBundle;
    ASSERT_NO_FATAL_FAILURE(
        auditEventBundle = auditEventGet(
            insurantKvnr, language, "date=ge" + startTime.toXsDateTimeWithoutFractionalSeconds().substr(0, 19) + "Z&_sort=date"));
    ASSERT_TRUE(auditEventBundle);
    ASSERT_EQ(auditEventBundle->getResourceCount(), expectedActions.size());

    const auto auditEvents = auditEventBundle->getResourcesByType<model::AuditEvent>("AuditEvent");
    for(std::size_t i = 0; i < auditEvents.size(); ++i)
    {
        const auto& auditEvent = auditEvents[i];
        EXPECT_TRUE(Uuid(auditEvent.id()).isValidIheUuid());
        EXPECT_EQ(auditEvent.language(), language);
        EXPECT_FALSE(auditEvent.textDiv().empty());
        EXPECT_EQ(auditEvent.subTypeCode(), expectedActions[i]);
        EXPECT_EQ(model::AuditEvent::ActionToSubType.at(auditEvent.action()), expectedActions[i]);
        EXPECT_GT(auditEvent.recorded(), startTime);
        EXPECT_EQ(auditEvent.outcome(), model::AuditEvent::Outcome::success);

        const auto[agentWhoSystem, agentWhoValue] = auditEvent.agentWho();
        ASSERT_TRUE(agentWhoSystem.has_value());
        if(actorTelematicIdIndices.count(i))
            EXPECT_EQ(agentWhoSystem.value(), model::resource::naming_system::telematicID) << "for i=" << i;
        else
            EXPECT_EQ(agentWhoSystem.value(), model::resource::naming_system::gkvKvid10) << "for i=" << i;

        ASSERT_TRUE(agentWhoValue.has_value());
        EXPECT_EQ(agentWhoValue.value(), actorIdentifiers[i]) << "for i=" << i;
        EXPECT_FALSE(auditEvent.agentName().empty());
        EXPECT_FALSE(auditEvent.sourceObserverReference().empty());

        const auto[entityWhatIdentifierSystem, entityWhatIdentifierValue] = auditEvent.entityWhatIdentifier();
        if(!prescriptionIds.at(i).has_value())
        {
            EXPECT_TRUE(!entityWhatIdentifierSystem.has_value());
            EXPECT_TRUE(!entityWhatIdentifierValue.has_value());

            // If no unique prescriptionId is available, the field AuditEvent.entity.description is filled with "+" or "-"
            const char* const expectedDescription = auditEvent.action() == model::AuditEvent::Action::del ? "-" : "+";
            EXPECT_EQ(auditEvent.entityDescription(), expectedDescription);
        }
        else
        {
            ASSERT_TRUE(entityWhatIdentifierSystem.has_value());
            EXPECT_EQ(entityWhatIdentifierSystem.value(), model::resource::naming_system::prescriptionID);
            ASSERT_TRUE(entityWhatIdentifierValue.has_value());
            EXPECT_EQ(entityWhatIdentifierValue.value(), prescriptionIds.at(i)->toString());
            EXPECT_EQ(auditEvent.entityDescription(), prescriptionIds.at(i)->toString());
        }
        EXPECT_FALSE(auditEvent.entityWhatReference().empty());
        EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
    }
}


void ErpWorkflowTestBase::checkAuditEventSorting(
    const std::string& kvnr,
    const std::string& searchCriteria,
    const std::function<void(const model::AuditEvent&, const model::AuditEvent&)>& compare)
{
    std::optional<model::Bundle> auditEventBundle;
    ASSERT_NO_FATAL_FAILURE(auditEventBundle = auditEventGet(kvnr, "de", searchCriteria));
    ASSERT_TRUE(auditEventBundle);
    auto auditEvents = auditEventBundle->getResourcesByType<model::AuditEvent>("AuditEvent");

    const model::AuditEvent* previousElement = nullptr;
    for (const auto& auditEvent : auditEvents)
    {
        if(previousElement != nullptr)
            compare(*previousElement, auditEvent);
        previousElement = &auditEvent;
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkAuditEventSearchSubType(const std::string& kvnr)
{
    std::unordered_map<model::AuditEvent::SubType, std::size_t> numOfEventsBySubType;
    std::optional<model::Bundle> auditEventBundle;
    ASSERT_NO_FATAL_FAILURE(auditEventBundle = auditEventGet(kvnr, "de", {}));
    ASSERT_TRUE(auditEventBundle);
    auto auditEvents = auditEventBundle->getResourcesByType<model::AuditEvent>("AuditEvent");
    for (const auto& auditEvent : auditEvents)
        ++numOfEventsBySubType[auditEvent.subTypeCode()];

    for(const auto& subTypeAndNum : numOfEventsBySubType)
    {
        const auto subTypeName = model::AuditEvent::SubTypeNames.at(subTypeAndNum.first);
        ASSERT_NO_FATAL_FAILURE(auditEventBundle = auditEventGet(kvnr, "de", "subtype=" + std::string(subTypeName)));
        ASSERT_TRUE(auditEventBundle);
        auditEvents = auditEventBundle->getResourcesByType<model::AuditEvent>("AuditEvent");
        EXPECT_EQ(auditEvents.size(), subTypeAndNum.second);
        for(const auto& auditEvent : auditEvents)
        {
            EXPECT_EQ(auditEvent.subTypeCode(), subTypeAndNum.first);
        }
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkAuditEventPaging(
    const std::string& kvnr,
    const std::size_t numEventsExpected,
    const std::size_t pageSize,
    const std::string& addSearch)
{
    std::optional<model::Bundle> auditEventBundle;
    ASSERT_NO_FATAL_FAILURE(auditEventBundle = auditEventGet(kvnr, "de", addSearch));
    ASSERT_TRUE(auditEventBundle);
    const auto numEvents = auditEventBundle->getResourceCount();
    ASSERT_GE(numEvents, numEventsExpected);

    const unsigned int numCalls = gsl::narrow_cast<unsigned int>(numEvents / pageSize);
    const unsigned int rest = gsl::narrow_cast<unsigned int>(numEvents % pageSize);
    for(unsigned int i = 0; i <= numCalls; ++i)
    {
        ASSERT_NO_FATAL_FAILURE(auditEventBundle = auditEventGet(
                kvnr, "de", "_count=" + std::to_string(pageSize) + "&__offset=" + std::to_string(i * pageSize) + "&_sort=-date" +
                (addSearch.empty() ? "" : "&") + addSearch));
        ASSERT_TRUE(auditEventBundle);
        EXPECT_EQ(auditEventBundle->getResourceCount(), i < numCalls ? pageSize : rest);
        EXPECT_EQ(auditEventBundle->getTotalSearchMatches(), numEventsExpected);
        if(i < numCalls)
            ASSERT_TRUE(auditEventBundle->getLink(model::Link::Type::Next).has_value());
        else
            ASSERT_FALSE(auditEventBundle->getLink(model::Link::Type::Next).has_value());
    }
}

std::string ErpWorkflowTestBase::getAuthorizationBearerValueForJwt(const JWT& jwt)
{
    return "Bearer " + jwt.serialize();
}

std::string ErpWorkflowTestBase::toCadesBesSignature(const std::string& content,
                                                     const std::optional<model::Timestamp>& signingTime)
{
    return CryptoHelper::toCadesBesSignature(content, signingTime);
}

std::string ErpWorkflowTestBase::toExpectedClientId(const ErpWorkflowTestBase::RequestArguments& args) const
{
    if (!args.overrideExpectedInnerClientId.empty())
    {
        return args.overrideExpectedInnerClientId;
    }
    const auto& jwt = args.jwt.value_or(defaultJwt());
    const auto clientid = jwt.stringForClaim(JWT::clientIdClaim);
    return clientid.value();
}

std::string ErpWorkflowTestBase::toExpectedRole(const ErpWorkflowTestBase::RequestArguments& args) const
{
    // outcome pharmacy, doctor, patient
    if (!args.overrideExpectedInnerRole.empty())
    {
        return args.overrideExpectedInnerRole;
    }
    const auto& jwt = args.jwt.value_or(defaultJwt());
    const auto oid = jwt.stringForClaim(JWT::professionOIDClaim);
    if (oid == profession_oid::oid_versicherter)
    {
        return "patient";
    }
    else if (oid == profession_oid::oid_oeffentliche_apotheke || oid == profession_oid::oid_krankenhausapotheke)
    {
        return "pharmacy";
    }
    else
    {
        return "doctor";
    }
}
std::string ErpWorkflowTestBase::toExpectedOperation(const ErpWorkflowTestBase::RequestArguments& args)
{
    // outcome e.g. "POST /Task/<id>/$activate"
    if (!args.overrideExpectedInnerOperation.empty())
    {
        return args.overrideExpectedInnerOperation;
    }
    auto expectedOperation = toString(args.method);
    std::string vauPath;
    std::tie(vauPath, std::ignore, std::ignore) = UrlHelper::splitTarget(args.vauPath);
    vauPath = UrlHelper::removeTrailingSlash(vauPath);

    if (String::starts_with(vauPath, "/Task") ||
        String::starts_with(vauPath, "/MedicationDispense") ||
        String::starts_with(vauPath, "/ChargeItem"))
    {
        std::regex matcher(R"--((\d{3}\.){5}\d{2}(-\d+)?)--");
        if (std::regex_search(vauPath, matcher))
        {
            vauPath = std::regex_replace(vauPath, matcher, "<id>");
        }
    }
    else if (String::starts_with(vauPath, "/Communication"))
    {
        std::regex matcher(R"--([0-9a-fA-F]{8}-([0-9a-fA-F]{4}-){3}[0-9a-fA-F]{12})--");
        if (std::regex_search(vauPath, matcher))
        {
            vauPath = std::regex_replace(vauPath, matcher, "<id>");
        }
    }
    else if (String::starts_with(vauPath, "/Consent"))
    {
        std::regex matcher(R"--(CHARGCONS-.{10})--");
        if (std::regex_search(vauPath, matcher))
        {
            vauPath = std::regex_replace(vauPath, matcher, "<id>");
        }
    }
    return expectedOperation.append(" ").append(vauPath);
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::communicationsGetInternal(std::optional<model::Bundle>& communicationsBundle,
                                                const JWT& jwt, const std::string_view& requestArgs)
{
    std::string getPath = "/Communication";
    if (!requestArgs.empty())
    {
        getPath.append("?").append(requestArgs);
    }
    ClientResponse serverResponse;
    // Please note that the content type of the response depends on the recipient's role.
    // In case of an insurant this should be json. But a Json Validator for schema "Gem_erxBundle"
    // is not available (not necessary) with the erp processing context. For this the content
    // will be requested as xml data.
    MimeType accept = MimeType::fhirXml;
    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::GET, getPath, ""}
                .withJwt(jwt)
                .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                .withHeader(Header::Accept, accept).withExpectedInnerStatus(HttpStatus::OK)));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);

    ASSERT_NO_FATAL_FAILURE(
        modelElemfromString<model::Bundle>(
            communicationsBundle, serverResponse.getBody(), *serverResponse.getHeader().contentType(), SchemaType::fhir));

    if (communicationsBundle->getResourceCount() > 0)
    {
        for (const auto& communication :
             communicationsBundle->getResourcesByType<model::Communication>("Communication"))
        {
            ASSERT_NO_THROW((void) model::Communication::fromXml(
                communication.serializeToXmlString(), *getXmlValidator(), *StaticData::getInCodeValidator(),
                model::Communication::messageTypeToSchemaType(communication.messageType())));
        }
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::communicationGetInternal(std::optional<model::Communication>& communication,
                                               const JWT& jwt,
                                               const Uuid& communicationId)
{
    std::string getPath = "/Communication/" + communicationId.toString();
    ClientResponse serverResponse;
    // Please note that the content type of the response depends on the recipient's role.
    // In case of an insurant this should be json. But a Json Validator for schema "Gem_erxBundle"
    // is not available (not necessary) with the erp processing context. For this the content
    // will be requested as xml data.
    MimeType accept = MimeType::fhirXml;
    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
        send(RequestArguments{ HttpMethod::GET, getPath, "" }
            .withJwt(jwt)
            .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
            .withHeader(Header::Accept, accept)));
    ASSERT_TRUE(serverResponse.getHeader().status() == HttpStatus::OK
             || serverResponse.getHeader().status() == HttpStatus::NotFound);
    ASSERT_TRUE(!serverResponse.getBody().empty());
    if (serverResponse.getHeader().status() == HttpStatus::NotFound)
    {
        ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(serverResponse.getBody(), false/*isJson*/,
                                                      model::OperationOutcome::Issue::Type::not_found));
    }
    else
    {
        ASSERT_NO_FATAL_FAILURE(
            modelElemfromString<model::Communication>(
                communication, serverResponse.getBody(), *serverResponse.getHeader().contentType(),
                model::Communication::messageTypeToSchemaType(communication->messageType())));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::communicationPostInternal(
    std::optional<model::Communication>& communicationResponse,
    model::Communication::MessageType messageType,
    const model::Task& task,
    ActorRole senderRole, const std::string& sender,
    ActorRole recipientRole, const std::string& recipient,
    const std::string& content)
{
    auto builder = CommunicationJsonStringBuilder(messageType);
    builder.setPrescriptionId(task.prescriptionId().toString());
    if (messageType == model::Communication::MessageType::DispReq)
    {
        if (task.status() == model::Task::Status::ready || task.status() == model::Task::Status::inprogress)
        {
            builder.setAccessCode(std::string(task.accessCode()));
        }
    }
    else if (messageType == model::Communication::MessageType::Representative)
    {
        if (task.status() == model::Task::Status::ready || task.status() == model::Task::Status::inprogress)
        {
            if (sender == std::string(task.kvnr().value()))
            {
                builder.setAccessCode(std::string(task.accessCode()));
            }
        }
    }
    if (!recipient.empty())
    {
        builder.setRecipient(recipientRole, recipient);
    }
    if (!content.empty())
    {
        builder.setPayload(content);
    }
    if (messageType == model::Communication::MessageType::InfoReq)
    {
        // The info request is the only communication profile whose element "about" is mandatory.
        builder.setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de");
    }
    MimeType contentType = MimeType::fhirJson;
    MimeType accept = MimeType::fhirJson;
    std::string communicationPath = "/Communication";
    ClientResponse serverResponse;
    ASSERT_TRUE(senderRole != ActorRole::Doctor) << "Invalid sender role";
    ASSERT_TRUE(recipientRole != ActorRole::Doctor) << "Invalid recipient role";
    JWT jwt;
    if (senderRole == ActorRole::Insurant || senderRole == ActorRole::Representative)
    {
        jwt = JwtBuilder::testBuilder().makeJwtVersicherter(sender);
    }
    else if (senderRole == ActorRole::Pharmacists)
    {
        jwt = JwtBuilder::testBuilder().makeJwtApotheke(sender);
        contentType = MimeType::fhirXml;
        accept = MimeType::fhirXml;
    }
    std::string body;
    if (contentType == std::string(MimeType::fhirJson))
    {
        body = builder.createJsonString();
    }
    else
    {
        body = builder.createXmlString();
    }
    RequestArguments requestArguments(HttpMethod::POST, communicationPath, body, contentType);
    requestArguments.jwt = jwt;
    requestArguments.headerFields.emplace(Header::Authorization, getAuthorizationBearerValueForJwt(jwt));
    requestArguments.headerFields.emplace(Header::Accept, accept);
    if (senderRole == ActorRole::Insurant || senderRole == ActorRole::Representative)
    {
        if (task.status() == model::Task::Status::ready || task.status() == model::Task::Status::inprogress)
        {
            if (sender != std::string(task.kvnr().value()))
            {
                requestArguments.headerFields.emplace(Header::XAccessCode, task.accessCode());
            }
        }
    }
    requestArguments.expectedInnerStatus = HttpStatus::Created;
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) = send(requestArguments));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::Created);

    ASSERT_NO_FATAL_FAILURE(
        modelElemfromString<model::Communication>(
            communicationResponse, serverResponse.getBody(), *serverResponse.getHeader().contentType(),
            model::Communication::messageTypeToSchemaType(messageType)));
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::medicationDispenseGetInternal(
    std::optional<model::MedicationDispense>& medicationDispense,
    const std::string& kvnr,
    const std::string& medicationDispenseId)
{
    std::string getPath = "/MedicationDispense/" + medicationDispenseId;
    ClientResponse serverResponse;
    JWT jwt{ JwtBuilder::testBuilder().makeJwtVersicherter(kvnr) };
    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::GET, getPath, ""}
                .withJwt(jwt).withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))));
    HttpStatus status = serverResponse.getHeader().status();
    ASSERT_TRUE((status == HttpStatus::OK) || (status == HttpStatus::NotFound));
    if (status == HttpStatus::OK)
    {
        ASSERT_NO_THROW(medicationDispense = model::MedicationDispense::fromJson(
                            serverResponse.getBody(), *getJsonValidator(), *getXmlValidator(),
                            *StaticData::getInCodeValidator(), SchemaType::Gem_erxMedicationDispense));
        ASSERT_TRUE(medicationDispense);
    }
    else
    {
        ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(serverResponse.getBody(), true/*isJson*/,
                                                      model::OperationOutcome::Issue::Type::not_found));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::medicationDispenseGetAllInternal(std::optional<model::Bundle>& medicationDispenseBundle,
                                                       const std::string_view& searchArguments,
                                                       const std::optional<JWT>& jwt)
{
    using namespace std::string_literals;
    constexpr auto copyToOriginalFormat = &model::NumberAsStringParserDocumentConverter::copyToOriginalFormat;
    std::string getPath = "/MedicationDispense/";
    if (!searchArguments.empty())
    {
        getPath.append("?").append(searchArguments);
    }
    ClientResponse serverResponse;
    const auto& theJwt = jwt.value_or(jwtVersicherter());
    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) = send(
            RequestArguments{HttpMethod::GET, getPath, ""}.withJwt(theJwt).withExpectedInnerStatus(HttpStatus::OK)));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    medicationDispenseBundle = model::Bundle::fromJsonNoValidation(serverResponse.getBody());
    ASSERT_TRUE(medicationDispenseBundle);
    ASSERT_NO_THROW(getJsonValidator()->validate(copyToOriginalFormat(
                                                    medicationDispenseBundle->jsonDocument()),
                                                    SchemaType::fhir));
    std::vector<model::MedicationDispense> medicationDispenses;
    if (medicationDispenseBundle->getResourceCount() > 0)
    {
        medicationDispenses =
            medicationDispenseBundle->getResourcesByType<model::MedicationDispense>("MedicationDispense");
        for (const auto& md: medicationDispenses)
        {
            ASSERT_NO_THROW(model::MedicationDispense::fromXml(md.serializeToXmlString(), *getXmlValidator(),
                                                               *StaticData::getInCodeValidator(),
                                                               SchemaType::Gem_erxMedicationDispense));
        }
    }
}


//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::taskGetInternal(std::optional<model::Bundle>& taskBundle,
                                      const std::string& kvnr,
                                      const std::string& searchArguments,
                                      const HttpStatus expectedStatus,
                                      const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    using namespace std::string_view_literals;
    RequestArguments args(HttpMethod::GET, "/Task", {});
    if (!searchArguments.empty())
    {
        args.vauPath.append("?").append(searchArguments);
    }

    args.jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    args.headerFields.emplace(Header::Authorization, getAuthorizationBearerValueForJwt(args.jwt.value()));
    args.expectedInnerStatus = expectedStatus;
    ClientResponse response;
    ASSERT_NO_FATAL_FAILURE(tie(std::ignore, response) = send(args));
    ASSERT_EQ(response.getHeader().status(), expectedStatus);
    if(expectedStatus == HttpStatus::OK)
    {
        auto contentType = response.getHeader().header(Header::ContentType);
        ASSERT_TRUE(contentType);
        EXPECT_EQ(contentType.value(), "application/fhir+json;charset=utf-8"sv);
        ASSERT_NO_THROW(taskBundle = model::Bundle::fromJsonNoValidation(response.getBody()));
        ASSERT_TRUE(taskBundle);

        ASSERT_NO_THROW(getJsonValidator()->validate(
            model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(taskBundle->jsonDocument()),
            SchemaType::fhir));
        if (taskBundle->getResourceCount() > 0)
        {
            const auto tasks = taskBundle->getResourcesByType<model::Task>("Task");
            for (const auto& task : tasks)
            {
                ASSERT_NO_THROW(model::Task::fromXml(task.serializeToXmlString(), *getXmlValidator(),
                                                     *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));
                ASSERT_FALSE(task.healthCarePrescriptionUuid().has_value());
                ASSERT_FALSE(task.patientConfirmationUuid().has_value());
                ASSERT_FALSE(task.receiptUuid().has_value());
            }
            EXPECT_LE(tasks.size(), taskBundle->getTotalSearchMatches());
        }
    }
    else
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(response.getBody(), true/*isJson*/, expectedErrorCode.value()));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::taskGetIdInternal(std::optional<model::Bundle>& taskBundle,
                                        const model::PrescriptionId& prescriptionId,
                                        const std::string& kvnrOrTid,
                                        const std::optional<std::string>& accessCodeOrSecret,
                                        const HttpStatus expectedStatus,
                                        const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode,
                                        bool withAuditEvents)
{
    using namespace std::string_literals;
    RequestArguments args(HttpMethod::GET, "/Task/"s + prescriptionId.toString(), {});
    bool isPatient = (kvnrOrTid.find('-') == std::string::npos);
    if (isPatient)
    {
        args.jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnrOrTid);
    }
    else
    {
        args.jwt = jwtApotheke();
    }
    args.headerFields.emplace(Header::Authorization, getAuthorizationBearerValueForJwt(args.jwt.value()));
    std::string revPfx = "?";
    if (accessCodeOrSecret && isPatient)
    {
        args.headerFields.emplace("X-AccessCode", accessCodeOrSecret.value());
        args.vauPath.append("?ac=").append(*accessCodeOrSecret);
        revPfx = "&";
    }
    else if (accessCodeOrSecret && !isPatient)
    {
        args.vauPath.append("?secret=").append(*accessCodeOrSecret);
        revPfx = "&";
    }
    if (withAuditEvents)
    {
        args.vauPath.append(revPfx + "_revinclude=AuditEvent:entity.what");
    }
    args.expectedInnerStatus = expectedStatus;
    ClientResponse response;
    ASSERT_NO_FATAL_FAILURE(tie(std::ignore, response) = send(args));
    ASSERT_EQ(response.getHeader().status(), expectedStatus);
    if(response.getHeader().status() == HttpStatus::OK)
    {
        if (response.getHeader().contentType() == std::string(ContentMimeType::fhirJsonUtf8))
        {
            taskBundle = model::Bundle::fromJsonNoValidation(response.getBody());
        }
        else
        {
            taskBundle = model::Bundle::fromXmlNoValidation(response.getBody());
        }
        ASSERT_TRUE(taskBundle);

        ASSERT_NO_THROW(getJsonValidator()->validate(
            model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(taskBundle->jsonDocument()),
            SchemaType::fhir));
        if (taskBundle->getResourceCount() > 0)
        {
            auto tasks = taskBundle->getResourcesByType<model::Task>("Task");
            ASSERT_EQ(tasks.size(), 1);
            ASSERT_NO_THROW(model::Task::fromXml(tasks[0].serializeToXmlString(), *getXmlValidator(),
                                                 *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));

            auto patientConfirmationOrReceipt = taskBundle->getResourcesByType<model::Bundle>("Bundle");
            if (! patientConfirmationOrReceipt.empty())
            {
                if (!isPatient)
                {
                    ASSERT_NO_THROW(model::ErxReceipt::fromXml(patientConfirmationOrReceipt[0].serializeToXmlString(),
                                                               *getXmlValidator(), *StaticData::getInCodeValidator(),
                                                               SchemaType::Gem_erxReceiptBundle));
                    ASSERT_FALSE(tasks[0].healthCarePrescriptionUuid().has_value());
                    ASSERT_FALSE(tasks[0].patientConfirmationUuid().has_value());
                    if (tasks[0].status() == model::Task::Status::completed)
                    {
                        ASSERT_TRUE(tasks[0].receiptUuid().has_value());
                    }
                    else
                    {
                        ASSERT_FALSE(tasks[0].receiptUuid().has_value());
                    }
                }
                else
                {
                    ASSERT_NO_THROW(model::KbvBundle::fromXml(patientConfirmationOrReceipt[0].serializeToXmlString(),
                                                              *getXmlValidator(), *StaticData::getInCodeValidator(),
                                                              SchemaType::KBV_PR_ERP_Bundle));
                    ASSERT_FALSE(tasks[0].healthCarePrescriptionUuid().has_value());
                    ASSERT_TRUE(tasks[0].patientConfirmationUuid().has_value());
                    ASSERT_FALSE(tasks[0].receiptUuid().has_value());
                }
            }
        }
    }
    else
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(response.getBody(), true/*isJson*/, expectedErrorCode.value()));
    }
}

std::string ErpWorkflowTestBase::medicationDispense(const std::string& kvnr,
                                                    const std::string& prescriptionIdForMedicationDispense,
                                                    const std::string& whenHandedOver)
{
    auto closeBody =
        ResourceManager::instance().getStringResource("test/EndpointHandlerTest/medication_dispense_input1.xml");

    closeBody = String::replaceAll(closeBody, "###PRESCRIPTIONID###", prescriptionIdForMedicationDispense);
    closeBody = String::replaceAll(closeBody, "X234567890", kvnr);
    closeBody = String::replaceAll(closeBody, "2021-09-21", whenHandedOver);
    closeBody = String::replaceAll(closeBody, "3-SMC-B-Testkarte-883110000120312",
                                   jwtApotheke().stringForClaim(JWT::idNumberClaim).value());

    return closeBody;
}

std::string ErpWorkflowTestBase::medicationDispenseBundle(const std::string& kvnr,
                                                          const std::string& prescriptionIdForMedicationDispense,
                                                          const std::string& whenHandedOver,
                                                          size_t numMedicationDispenses)
{
    model::Bundle closeBody(model::BundleType::collection, model::ResourceBase::NoProfile, Uuid());
    for (size_t i = 0; i < numMedicationDispenses; ++i)
    {
        auto dispense = model::MedicationDispense::fromXmlNoValidation(
            medicationDispense(kvnr, prescriptionIdForMedicationDispense, whenHandedOver));
        closeBody.addResource({}, {}, {}, dispense.jsonDocument());
    }
    return closeBody.serializeToXmlString();
}


std::string ErpWorkflowTestBase::patchVersionsInBundle(const std::string& bundle)
{
    return String::replaceAll(
        bundle, "|1.0.2",
        "|" + std::string(v_str(model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>())));
}


bool ErpWorkflowTestBase::runsInCloudEnv() const
{
    const auto hostaddress = client->getHostAddress();
    return hostaddress != "localhost" && hostaddress != "127.0.0.1";
}


//static
bool ErpWorkflowTestBase::isUnsupportedFlowtype(const model::PrescriptionType workflowType)
{
    bool featurePkvEnabled = Configuration::instance().getOptionalBoolValue(ConfigurationKey::FEATURE_PKV, false);
    return workflowType == model::PrescriptionType::apothekenpflichtigeArzneimittelPkv && !featurePkvEnabled;
}


//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::taskCloseInternal(std::optional<model::ErxReceipt>& receipt,
                                        const model::PrescriptionId& prescriptionId,
                                        const std::string& secret,
                                        const std::string& kvnr,
                                        const std::string& prescriptionIdForMedicationDispense,
                                        const HttpStatus expectedInnerStatus,
                                        const std::optional<model::OperationOutcome::Issue::Type>& expectedErrorCode,
                                        const std::optional<std::string>& expectedErrorText,
                                        const std::optional<std::string>& expectedDiagnostics,
                                        const std::string& whenHandedOver,
                                        size_t numMedicationDispenses)
{
    std::string closeBody;
    if (numMedicationDispenses == 1)
    {
        closeBody = medicationDispense(kvnr, prescriptionIdForMedicationDispense, whenHandedOver);
    }
    else
    {
        closeBody =
            medicationDispenseBundle(kvnr, prescriptionIdForMedicationDispense, whenHandedOver, numMedicationDispenses);
    }

    std::string closePath = "/Task/" + prescriptionId.toString() + "/$close?secret=" + secret;
    ClientResponse serverResponse;
    JWT jwt{ jwtApotheke() };
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) =
                                send(RequestArguments{HttpMethod::POST, closePath, closeBody, "application/fhir+xml"}
                                         .withJwt(jwt)
                                         .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                                         .withExpectedInnerStatus(expectedInnerStatus)
                                         .withExpectedInnerFlowType(std::to_string(magic_enum::enum_integer(prescriptionId.type())))));
    ASSERT_EQ(serverResponse.getHeader().status(), expectedInnerStatus);
    if(expectedInnerStatus == HttpStatus::OK)
    {
        ASSERT_NO_THROW(receipt = model::ErxReceipt::fromXml(serverResponse.getBody(), *getXmlValidator(),
                                                             *StaticData::getInCodeValidator(),
                                                             SchemaType::Gem_erxReceiptBundle));
    }
    else
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(serverResponse.getBody(), false/*isJson*/,
                                                      expectedErrorCode.value(), expectedErrorText, expectedDiagnostics));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::taskAcceptInternal(std::optional<model::Bundle>& bundle,
                                         const model::PrescriptionId& prescriptionId,
                                         const std::string& accessCode, HttpStatus expectedInnerStatus,
                                         const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    std::string acceptPath = "/Task/" + prescriptionId.toString() + "/$accept?ac=" + accessCode;
    ClientResponse serverResponse;
    JWT jwt{ jwtApotheke() };
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) =
                                send(RequestArguments{HttpMethod::POST, acceptPath, {}}
                                         .withJwt(jwt)
                                         .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                                         .withExpectedInnerStatus(expectedInnerStatus)
                                         .withExpectedInnerFlowType(std::to_string(magic_enum::enum_integer(prescriptionId.type())))));
    ASSERT_EQ(serverResponse.getHeader().status(), expectedInnerStatus);
    if(expectedInnerStatus == HttpStatus::OK)
    {
        ASSERT_NO_THROW(bundle = model::Bundle::fromXml(serverResponse.getBody(),
                                                        *getXmlValidator(), *StaticData::getInCodeValidator(),
                                                        SchemaType::fhir));
    }
    else
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(serverResponse.getBody(), false/*isJson*/, expectedErrorCode.value()));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::taskActivateInternal(std::optional<model::Task>& task,
                                               const model::PrescriptionId& prescriptionId,
                                               const std::string& accessCode,
                                               const std::string& qesBundle,
                                               HttpStatus expectedInnerStatus,
                                               const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode,
                                               const std::optional<std::string>& expectedIssueText,
                                               const std::optional<std::string>& expectedIssueDiagnostics)
{
    using namespace std::string_literals;
    std::string activateBody = R"(<Parameters xmlns="http://hl7.org/fhir">)""\n"
                               "    <parameter>\n"
                               "      <name value=\"ePrescription\" />\n"
                               "      <resource>\n"
                               "        <Binary>\n"
                               "          <contentType value=\"application/pkcs7-mime\" />\n"
                               "          <data value=\"" + qesBundle + "\" />\n"
                                                                        "        </Binary>\n"
                                                                        "      </resource>\n"
                                                                        "    </parameter>\n"
                                                                        "</Parameters>\n";


    std::string activatePath = "/Task/" + prescriptionId.toString() + "/$activate";
    ClientResponse serverResponse;
    ClientResponse outerResponse;
    JWT jwt{jwtArzt()};
    ASSERT_NO_FATAL_FAILURE(
        std::tie(outerResponse, serverResponse) =
            send(RequestArguments{HttpMethod::POST, activatePath,
                                  activateBody, "application/fhir+xml"}
                .withJwt(jwt)
                .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                .withHeader("X-AccessCode", accessCode)
                .withExpectedInnerStatus(expectedInnerStatus)
                .withExpectedInnerFlowType(std::to_string(magic_enum::enum_integer(prescriptionId.type())))));

    ASSERT_EQ(serverResponse.getHeader().status(), expectedInnerStatus);
    if(expectedInnerStatus == HttpStatus::OK)
    {
        ASSERT_NO_THROW(task = model::Task::fromXml(serverResponse.getBody(), *getXmlValidator(),
                                                    *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));
    }
    else
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        ASSERT_NO_FATAL_FAILURE(
            checkOperationOutcome(serverResponse.getBody(), false/*isJson*/, expectedErrorCode.value(),
                                  expectedIssueText, expectedIssueDiagnostics)
        );
    }
}
void ErpWorkflowTestBase::makeQESBundleInternal(std::string& qesBundle, const std::string& kvnr,
                                            const model::PrescriptionId& prescriptionId,
                                            const model::Timestamp& now)
{
    std::string kvType = "GKV";
    std::string patientIdentifierAssigner;
    if(prescriptionId.type() == model::PrescriptionType::apothekenpflichtigeArzneimittelPkv)
    {
        kvType = "PKV";
        patientIdentifierAssigner = R"(
                    <assigner>
                        <display value="Assigning organization"/>
                    </assigner>)";
    }

    qesBundle = R"(
<Bundle xmlns="http://hl7.org/fhir">
    <id value="281a985c-f25b-4aae-91a6-41ad744080b0"/>
    <meta>
        <profile value="https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|)" + std::string(v_str(model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>())) + R"("/>
    </meta>
    <identifier>
        <system value="https://gematik.de/fhir/NamingSystem/PrescriptionID"/>
        <value value=")" + prescriptionId.toString() + R"("/>
    </identifier>
    <type value="document"/>
    <timestamp value=")" + now.toXsDateTime() + R"("/>
    <entry>
        <fullUrl value="http://pvs.praxis-topp-gluecklich.local/fhir/Composition/5e709fc5-c233-456c-bf81-20534cbf9565" />
        <resource>
          <Composition>
            <id value="5e709fc5-c233-456c-bf81-20534cbf9565" />
            <meta>
              <profile value="https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Composition|)" + std::string(v_str(model::ResourceVersion::current<model::ResourceVersion::KbvItaErp>())) + R"("/>
            </meta>
            <extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_FOR_Legal_basis">
              <valueCoding>
                <system value="https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_STATUSKENNZEICHEN" />
                <code value="00" />
              </valueCoding>
            </extension>
            <status value="final" />
            <type>
              <coding>
                <system value="https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_FORMULAR_ART" />
                <code value="e16A" />
              </coding>
            </type>
            <subject>
              <reference value="Patient/9774f67f-a238-4daf-b4e6-679deeef3811" />
            </subject>
            <date value="2020-02-03T11:30:02Z" />
            <author>
              <reference value="Practitioner/20597e0e-cb2a-45b3-95f0-dc3dbdb617c3" />
              <type value="Practitioner" />
            </author>
            <author>
              <type value="Device" />
              <identifier>
                <system value="https://fhir.kbv.de/NamingSystem/KBV_NS_FOR_Pruefnummer" />
                <value value="Y/400/1910/36/346" />
              </identifier>
            </author>
            <title value="elektronische Arzneimittelverordnung" />
            <custodian>
              <reference value="Organization/cf042e44-086a-4d51-9c77-172f9a972e3b" />
            </custodian>
            <section>
              <code>
                <coding>
                  <system value="https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Section_Type" />
                  <code value="Prescription" />
                </coding>
              </code>
              <entry>
                <!--  Referenz auf Verordnung (MedicationRequest)  -->
                <reference value="MedicationRequest/931e9384-1b80-41f0-a40b-d2ced5e6d856" />
              </entry>
            </section>
            <section>
              <code>
                <coding>
                  <system value="https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_Section_Type" />
                  <code value="Coverage" />
                </coding>
              </code>
              <entry>
                <!--  Referenz auf Krankenkasse/Kostentr????ger   -->
                <reference value="Coverage/1b1ffb6e-eb05-43d7-87eb-e7818fe9661a" />
              </entry>
            </section>
          </Composition>
        </resource>
      </entry>
    <entry>
        <fullUrl value="http://pvs.praxis-topp-gluecklich.local/fhir/Patient/9774f67f-a238-4daf-b4e6-679deeef3811"/>
        <resource>
            <Patient>
                <id value="9774f67f-a238-4daf-b4e6-679deeef3811"/>
                <meta>
                    <profile value="https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Patient|1.0.3"/>
                </meta>
                <identifier>
                    <type>
                        <coding>
                            <system value="http://fhir.de/CodeSystem/identifier-type-de-basis"/>
                            <code value=")" + kvType + R"("/>
                        </coding>
                    </type>
                    <system value="http://fhir.de/NamingSystem/gkv/kvid-10"/>
                    <value value=")" + kvnr + R"("/>)" + patientIdentifierAssigner + R"(
                </identifier>
                <name>
                    <use value="official"/>
                    <family value="Ludger K??nigsstein">
                        <extension url="http://hl7.org/fhir/StructureDefinition/humanname-own-name">
                            <valueString value="K??nigsstein"/>
                        </extension>
                    </family>
                    <given value="Ludger"/>
                </name>
                <birthDate value="1935-06-22"/>
                <address>
                    <type value="both"/>
                    <line value="Musterstr. 1">
                        <extension url="http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-streetName">
                            <valueString value="Musterstr."/>
                        </extension>
                        <extension url="http://hl7.org/fhir/StructureDefinition/iso21090-ADXP-houseNumber">
                            <valueString value="1"/>
                        </extension>
                    </line>
                    <city value="Berlin"/>
                    <postalCode value="10623"/>
                </address>
            </Patient>
        </resource>
    </entry>
    <entry>
        <fullUrl value="http://pvs.praxis-topp-gluecklich.local/fhir/Coverage/312e170e-de2f-47bf-8ee3-e8c3a353f6bd"/>
        <resource>
            <Coverage>
                <id value="312e170e-de2f-47bf-8ee3-e8c3a353f6bd"/>
                <meta>
                    <profile value="https://fhir.kbv.de/StructureDefinition/KBV_PR_FOR_Coverage|1.0.3"/>
                </meta>
                <extension url="http://fhir.de/StructureDefinition/gkv/versichertenart">
                    <valueCoding>
                        <system value="https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_VERSICHERTENSTATUS"/>
                        <code value="1"/>
                    </valueCoding>
                </extension>
                <extension url="http://fhir.de/StructureDefinition/gkv/besondere-personengruppe">
                    <valueCoding>
                        <system value="https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_PERSONENGRUPPE"/>
                        <code value="00"/>
                    </valueCoding>
                </extension>
                <extension url="http://fhir.de/StructureDefinition/gkv/dmp-kennzeichen">
                    <valueCoding>
                        <system value="https://fhir.kbv.de/CodeSystem/KBV_CS_SFHIR_KBV_DMP"/>
                        <code value="00"/>
                    </valueCoding>
                </extension>
                <status value="active"/>
                <type>
                    <coding>
                        <system value="http://fhir.de/CodeSystem/versicherungsart-de-basis"/>
                        <code value=")" + kvType + R"("/>
                    </coding>
                </type>
                <beneficiary>
                    <reference value="Patient/9774f67f-a238-4daf-b4e6-679deeef3811"/>
                </beneficiary>
                <payor>
                    <identifier>
                        <system value="http://fhir.de/NamingSystem/arge-ik/iknr"/>
                        <value value="109500969"/>
                    </identifier>
                    <display value="Test KV"/>
                </payor>
            </Coverage>
        </resource>
    </entry>
    <entry>
        <fullUrl value="http://pvs.praxis.local/fhir/MedicationRequest/005cd690-7473-4bf4-a502-30a32f2e7623"/>
        <resource>
            <MedicationRequest>
                <id value="005cd690-7473-4bf4-a502-30a32f2e7623"/>
                <meta>
                    <profile value="https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Prescription|1.0.2"/>
                </meta>
                <extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_EmergencyServicesFee">
                    <valueBoolean value="false"/>
                </extension>
                <extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_BVG">
                    <valueBoolean value="false"/>
                </extension>
                <extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_Multiple_Prescription">
                    <extension url="Kennzeichen">
                        <valueBoolean value="false"/>
                    </extension>
                </extension>
                <extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_StatusCoPayment">
                    <valueCoding>
                        <system value="https://fhir.kbv.de/CodeSystem/KBV_CS_ERP_StatusCoPayment"/>
                        <code value="0"/>
                    </valueCoding>
                </extension>
                <status value="active"/>
                <intent value="order"/>
                <medicationReference>
                    <reference value="Medication/4980b470-e8e5-44bf-a53a-47ade2561cdb"/>
                </medicationReference>
                <subject>
                    <reference value="Patient/9774f67f-a238-4daf-b4e6-679deeef3811"/>
                </subject>
                <authoredOn value="2021-06-08"/>
                <requester>
                    <reference value="Practitioner/6e6c209e-1b86-43e0-9794-f54ec147fc49"/>
                </requester>
                <insurance>
                    <reference value="Converage/312e170e-de2f-47bf-8ee3-e8c3a353f6bd"/>
                </insurance>
                <dosageInstruction>
                    <extension url="https://fhir.kbv.de/StructureDefinition/KBV_EX_ERP_DosageFlag">
                        <valueBoolean value="true"/>
                    </extension>
                    <text value="1x t&#xE4;glich"/>
                </dosageInstruction>
                <dispenseRequest>
                    <quantity>
                        <value value="1"/>
                        <system value="http://unitsofmeasure.org"/>
                        <code value="{Package}"/>
                    </quantity>
                </dispenseRequest>
                <substitution>
                    <allowedBoolean value="false"/>
                </substitution>
            </MedicationRequest>
        </resource>
        </entry>
</Bundle>
)";
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::taskCreateInternal(std::optional<model::Task>& task, HttpStatus expectedOuterStatus,
                                         HttpStatus expectedInnerStatus,
                                         const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode,
                                         model::PrescriptionType workflowType)
{
    using namespace std::string_view_literals;
    using model::Task;
    using model::Timestamp;

    std::string create =
        "<Parameters xmlns=\"http://hl7.org/fhir\">\n"
        "  <parameter>\n"
        "    <name value=\"workflowType\"/>\n"
        "    <valueCoding>\n"
        "      <system value=\"https://gematik.de/fhir/CodeSystem/Flowtype\"/>\n"
        "      <code value=\"";
    switch (workflowType)
    {
        case model::PrescriptionType::apothekenpflichigeArzneimittel:
            create += "160";
            break;
        case model::PrescriptionType::direkteZuweisung:
            create += "169";
            break;
        case model::PrescriptionType::apothekenpflichtigeArzneimittelPkv:
            create += "200";
            break;
    }
    create += "\"/>\n"
        "    </valueCoding>\n"
        "  </parameter>\n"
        "</Parameters>\n";

    ClientResponse serverResponse;
    ClientResponse outerResponse;

    JWT jwt{ jwtArzt() };
    ASSERT_NO_FATAL_FAILURE(std::tie(outerResponse, serverResponse) =
                                send(RequestArguments{HttpMethod::POST, "/Task/$create", create,
                                                      "application/fhir+xml"}
                                    .withJwt(jwt)
                                    .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                                    .withExpectedInnerStatus(expectedInnerStatus)));

    ASSERT_EQ(outerResponse.getHeader().status(), expectedOuterStatus) << outerResponse.getBody();
    ASSERT_EQ(serverResponse.getHeader().status(), expectedInnerStatus) << serverResponse.getBody();
    if (serverResponse.getHeader().status() == HttpStatus::Created)
    {
        ASSERT_NO_THROW(task = Task::fromXml(serverResponse.getBody(), *getXmlValidator(),
                                             *StaticData::getInCodeValidator(), SchemaType::Gem_erxTask));
    }
    else if(expectedErrorCode.has_value())
    {
        // There exist test cases where no error message is created in case of error because inner request could not
        // be decooded. Then the parameter expectedErrorCode is not set.
        ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(serverResponse.getBody(), false/*isJson*/, expectedErrorCode.value()));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::auditEventGetInternal(
    std::optional<model::Bundle>& auditEventBundle,
    const std::string& kvnr,
    const std::string& language,
    const std::string& searchArguments)
{
    using namespace std::string_view_literals;
    std::string vauPath = "/AuditEvent";
    if (!searchArguments.empty())
        vauPath.append("?").append(searchArguments);

    ClientResponse response;
    JWT jwt{JwtBuilder::testBuilder().makeJwtVersicherter(kvnr)};
    ASSERT_NO_FATAL_FAILURE(tie(std::ignore, response) = send(RequestArguments{HttpMethod::GET, vauPath, {}}
         .withHeader(Header::AcceptLanguage, language)
         .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
         .withJwt(jwt).withExpectedInnerStatus(HttpStatus::OK)));
    ASSERT_EQ(response.getHeader().status(), HttpStatus::OK);
    auto contentType = response.getHeader().header(Header::ContentType);
    ASSERT_TRUE(contentType);
    EXPECT_EQ(contentType.value(), "application/fhir+json;charset=utf-8"sv);
    ASSERT_NO_THROW(auditEventBundle = model::Bundle::fromJsonNoValidation(response.getBody()));
    ASSERT_TRUE(auditEventBundle);

    ASSERT_NO_THROW(getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(auditEventBundle->jsonDocument()),
        SchemaType::fhir));
    if (auditEventBundle->getResourceCount() > 0)
    {
        auto auditEvents = auditEventBundle->getResourcesByType<model::AuditEvent>("AuditEvent");
        for (const auto& auditEvent : auditEvents)
        {
            ASSERT_NO_THROW(model::AuditEvent::fromXml(auditEvent.serializeToXmlString(), *getXmlValidator(),
                                                       *StaticData::getInCodeValidator(),
                                                       SchemaType::Gem_erxAuditEvent));
        }
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::metaDataGetInternal(
    std::optional<model::MetaData>& metaData,
    const ContentMimeType& acceptContentType)
{
    ClientResponse response;
    JWT jwt{JwtBuilder::testBuilder().makeJwtApotheke()};  // could be any valid jwt;
    ASSERT_NO_FATAL_FAILURE(tie(std::ignore, response) = send(RequestArguments{HttpMethod::GET, "/metadata", {}}
        .withJwt(jwt)
        .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
        .withHeader(Header::Accept, acceptContentType).withExpectedInnerStatus(HttpStatus::OK)));
    ASSERT_EQ(response.getHeader().status(), HttpStatus::OK);
    auto contentType = response.getHeader().header(Header::ContentType);
    ASSERT_TRUE(contentType);
    EXPECT_EQ(contentType.value(), static_cast<std::string>(acceptContentType));

    ASSERT_NO_FATAL_FAILURE(
        modelElemfromString<model::MetaData>(
            metaData, response.getBody(), contentType.value(), SchemaType::fhir));
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::deviceGetInternal(
    std::optional<model::Device>& device,
    const ContentMimeType& acceptContentType)
{
    ClientResponse response;
    JWT jwt{ JwtBuilder::testBuilder().makeJwtApotheke() };  // could be any valid jwt;
    ASSERT_NO_FATAL_FAILURE(tie(std::ignore, response) = send(RequestArguments{ HttpMethod::GET, "/Device", {} }
        .withJwt(jwt)
        .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
        .withHeader(Header::Accept, acceptContentType).withExpectedInnerStatus(HttpStatus::OK)));
    ASSERT_EQ(response.getHeader().status(), HttpStatus::OK);
    auto contentType = response.getHeader().header(Header::ContentType);
    ASSERT_TRUE(contentType);
    EXPECT_EQ(contentType.value(), static_cast<std::string>(acceptContentType));

    ASSERT_NO_FATAL_FAILURE(
        modelElemfromString<model::Device>(
            device, response.getBody(), contentType.value(), SchemaType::fhir));
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::consentPostInternal(
    std::optional<model::Consent>& consent,
    const std::string& kvnr,
    const model::Timestamp& dateTime,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    auto consentBody = ResourceManager::instance().getStringResource("test/EndpointHandlerTest/consent_template.json");
    consentBody = String::replaceAll(String::replaceAll(consentBody, "##KVNR##", kvnr),
                                     "##DATETIME##", dateTime.toXsDateTimeWithoutFractionalSeconds());

    ClientResponse serverResponse;
    ClientResponse outerResponse;
    JWT jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);

    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::POST, "/Consent", consentBody, static_cast<std::string>(ContentMimeType::fhirJsonUtf8)}
                .withJwt(jwt)
                .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                .withExpectedInnerStatus(expectedStatus)));

    if(expectedStatus == HttpStatus::Created)
    {
         ASSERT_NO_THROW(consent = model::Consent::fromJson(serverResponse.getBody(), *getJsonValidator(), *getXmlValidator(),
                                                            *StaticData::getInCodeValidator(), SchemaType::Gem_erxConsent));
    }
    else
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(serverResponse.getBody(), true/*isJson*/, expectedErrorCode.value()));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::consentGetInternal(
    std::optional<model::Consent>& consent,
    const std::string& kvnr,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    ClientResponse serverResponse;
    ClientResponse outerResponse;
    JWT jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);

    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::GET, "/Consent", ""}
                .withJwt(jwt)
                .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                .withExpectedInnerStatus(expectedStatus)));

    if(expectedStatus == HttpStatus::OK)
    {
        std::optional<model::Bundle> consentBundle;
        ASSERT_NO_THROW(consentBundle = model::Bundle::fromJson(serverResponse.getBody(), *getJsonValidator(), *getXmlValidator(),
                                                                *StaticData::getInCodeValidator(), SchemaType::fhir));
        ASSERT_TRUE(consentBundle.has_value());
        ASSERT_LE(consentBundle->getResourceCount(), 1);
        if(consentBundle->getResourceCount() == 1)
        {
            auto consents = consentBundle->getResourcesByType<model::Consent>("Consent");
            ASSERT_EQ(consents.size(), 1);
            consent = std::move(consents.front());
        }
    }
    else
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(serverResponse.getBody(), true/*isJson*/, expectedErrorCode.value()));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::chargeItemPostInternal(
    std::optional<model::ChargeItem>& resultChargeItem,
    const model::PrescriptionId& prescriptionId,
    const std::string& kvnr,
    const std::string& telematikId,
    const std::string& secret,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    auto& resourceManager = ResourceManager::instance();
    const auto dispenseBundleString = resourceManager.getStringResource("test/EndpointHandlerTest/dispense_item.xml");
    const auto chargeItemTemplate = resourceManager.getStringResource("test/EndpointHandlerTest/charge_item_POST_template.xml");

    const auto serviceContext = StaticData::makePcServiceContext();

    std::optional<Certificate> certificate{};
    std::optional<shared_EVP_PKEY> prvKey{};
    if (runsInCloudEnv())
    {
        certificate.emplace(CryptoHelper::cHpQesWansim());
        prvKey.emplace(CryptoHelper::cHpQesPrvWansim());
    }
    else
    {
        certificate.emplace(CryptoHelper::cHpQes());
        prvKey.emplace(CryptoHelper::cHpQesPrv());
    }

    CadesBesSignature cadesBesSignature{*certificate,
                                        *prvKey,
                                        dispenseBundleString,
                                        std::nullopt};
    const auto chargeItemString =
        String::replaceAll(
            String::replaceAll(
                String::replaceAll(chargeItemTemplate, "##PRESCRIPTION_ID##", prescriptionId.toString()),
                "##KVNR##", kvnr),
            "##DISPENSE_BUNDLE##", Base64::encode(cadesBesSignature.get()));
    auto inputChargeItem = model::ChargeItem::fromXmlNoValidation(chargeItemString);
    inputChargeItem.setEntererTelematikId(telematikId);

    ClientResponse serverResponse;
    ClientResponse outerResponse;
    JWT jwt = JwtBuilder::testBuilder().makeJwtApotheke(std::string(inputChargeItem.entererTelematikId()));
    std::string postChargeItemPath = "/ChargeItem?task=" + prescriptionId.toString() + "&secret=" + secret;

    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::POST, postChargeItemPath, inputChargeItem.serializeToXmlString(),
                                  static_cast<std::string>(ContentMimeType::fhirXmlUtf8)}
                 .withJwt(jwt)
                 .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                 .withExpectedInnerStatus(expectedStatus)));

    if(expectedStatus == HttpStatus::Created)
    {
        ASSERT_NO_THROW(resultChargeItem = model::ChargeItem::fromXml(
            serverResponse.getBody(), *getXmlValidator(), *StaticData::getInCodeValidator(), SchemaType::Gem_erxChargeItem));
    }
    else
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(serverResponse.getBody(), false/*isJson*/, expectedErrorCode.value()));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::chargeItemPutInternal(
    std::optional<model::ChargeItem>& resultChargeItem,
    const JWT& jwt,
    const ContentMimeType& contentType,
    const model::ChargeItem& inputChargeItem,
    const std::optional<std::string>& newMedicationDispenseString,
    const std::optional<std::tuple<bool, bool, bool>>& newMarking,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    auto chargeItemString = inputChargeItem.serializeToXmlString();
    if (newMedicationDispenseString.has_value())
    {
        const auto serviceContext = StaticData::makePcServiceContext();
        CadesBesSignature cadesBesSignature{CryptoHelper::cHpQes(),
                                            CryptoHelper::cHpQesPrv(),
                                            *newMedicationDispenseString,
                                            std::nullopt};

        chargeItemString = String::replaceAll(
            chargeItemString, "</meta>",
            "</meta><contained><Binary><id value=\"dispense-1\"/><contentType value=\"application/pkcs7-mime\"/>"
            "<data value=\"" + Base64::encode(cadesBesSignature.get()) + "\"/></Binary></contained>");
    }

    if(newMarking.has_value())
    {
        const auto [insuranceProvider, subsidy, taxOffice] = newMarking.value();
        chargeItemString = String::replaceAll(
            chargeItemString, "<extension url=\"insuranceProvider\"><valueBoolean value=\"false\"/>",
            std::string("<extension url=\"insuranceProvider\"><valueBoolean value=\"") + (insuranceProvider ? "true" : "false") + "\"/>");
        chargeItemString = String::replaceAll(
            chargeItemString, "<extension url=\"subsidy\"><valueBoolean value=\"false\"/>",
            std::string("<extension url=\"subsidy\"><valueBoolean value=\"") + (subsidy ? "true" : "false") + "\"/>");
        chargeItemString = String::replaceAll(
            chargeItemString, "<extension url=\"taxOffice\"><valueBoolean value=\"false\"/>",
            std::string("<extension url=\"taxOffice\"><valueBoolean value=\"") + (taxOffice ? "true" : "false") + "\"/>");
    }
    if(std::string(contentType) == std::string(ContentMimeType::fhirJsonUtf8))
    {
        chargeItemString = model::ChargeItem::fromXmlNoValidation(chargeItemString).serializeToJsonString();
    }

    ClientResponse serverResponse;
    ClientResponse outerResponse;
    std::string putChargeItemPath = "/ChargeItem/" + inputChargeItem.prescriptionId().toString();

    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::PUT, putChargeItemPath, chargeItemString,//inputChargeItem.serializeToXmlString(),
                                  static_cast<std::string>(contentType)}
                 .withJwt(jwt)
                 .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                 .withHeader(Header::Accept, contentType)
                 .withHeader(Header::XAccessCode, inputChargeItem.accessCode()->data())
                 .withExpectedInnerStatus(expectedStatus)));

    if(expectedStatus == HttpStatus::OK)
    {
        const auto resultContentType = serverResponse.getHeader().header(Header::ContentType);
        ASSERT_TRUE(resultContentType);
        EXPECT_EQ(resultContentType.value(), static_cast<std::string>(contentType));
        ASSERT_NO_FATAL_FAILURE(
            modelElemfromString<model::ChargeItem>(resultChargeItem, serverResponse.getBody(), *resultContentType, SchemaType::Gem_erxChargeItem));
    }
    else
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        ASSERT_NO_FATAL_FAILURE(
            checkOperationOutcome(serverResponse.getBody(), std::string(contentType) == std::string(ContentMimeType::fhirJsonUtf8)/*isJson*/,
                                  expectedErrorCode.value()));
    }
}


//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::chargeItemsGetInternal(
    std::optional<model::Bundle>& chargeItemsBundle,
    const JWT& jwt,
    const ContentMimeType& contentType,
    const std::string_view& searchArguments,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    std::string vauPath = "/ChargeItem";
    if (!searchArguments.empty())
    {
        vauPath.append("?").append(searchArguments);
    }
    ClientResponse response;
    ASSERT_NO_FATAL_FAILURE(
        tie(std::ignore, response) =
            send(RequestArguments{HttpMethod::GET, vauPath, {}}
                     .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                     .withJwt(jwt)
                     .withHeader(Header::Accept, contentType)
                     .withExpectedInnerStatus(expectedStatus)));

    if(expectedStatus == HttpStatus::OK)
    {
        const auto resultContentType = response.getHeader().header(Header::ContentType);
        ASSERT_TRUE(resultContentType);
        EXPECT_EQ(resultContentType.value(), static_cast<std::string>(contentType));

        ASSERT_NO_FATAL_FAILURE(modelElemfromString<model::Bundle>(chargeItemsBundle, response.getBody(),
                                                                   *resultContentType, SchemaType::fhir));

        if(chargeItemsBundle->getResourceCount() > 0)
        {
            const auto chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
            for (const auto& chargeItem : chargeItems)
            {
                ASSERT_NO_THROW(model::ChargeItem::fromXml(chargeItem.serializeToXmlString(), *getXmlValidator(),
                                                           *StaticData::getInCodeValidator(),
                                                           SchemaType::Gem_erxChargeItem));
            }
        }
    }
    else
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        ASSERT_NO_FATAL_FAILURE(
            checkOperationOutcome(response.getBody(), std::string(contentType) == std::string(ContentMimeType::fhirJsonUtf8)/*isJson*/,
                                  expectedErrorCode.value()));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::chargeItemGetIdInternal(
    std::optional<model::Bundle>& chargeItemResultBundle,
    const JWT& jwt,
    const ContentMimeType& contentType,
    const model::PrescriptionId& id,
    const std::optional<std::string_view>& accessCode,
    const std::optional<model::KbvBundle>& expectedKbvBundle,
    const std::optional<model::ErxReceipt>& expectedReceipt,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    std::string vauPath = "/ChargeItem/" + id.toString();
    if (accessCode.has_value())
    {
        vauPath += "?ac=";
        vauPath += accessCode->data();
    }

    ClientResponse response;
    ASSERT_NO_FATAL_FAILURE(
        tie(std::ignore, response) =
            send(RequestArguments{HttpMethod::GET, vauPath, {}}
                .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                .withJwt(jwt)
                .withHeader(Header::Accept, contentType)
                 .withExpectedInnerStatus(expectedStatus)));
    if(expectedStatus == HttpStatus::OK)
    {
        const auto resultContentType = response.getHeader().header(Header::ContentType);
        ASSERT_TRUE(resultContentType);
        EXPECT_EQ(resultContentType.value(), static_cast<std::string>(contentType));

        ASSERT_NO_FATAL_FAILURE(
            modelElemfromString<model::Bundle>(chargeItemResultBundle, response.getBody(), *resultContentType, SchemaType::fhir));

        ASSERT_GE(chargeItemResultBundle->getResourceCount(), 2);
        const auto chargeItems = chargeItemResultBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
        ASSERT_EQ(chargeItems.size(), 1);
        ASSERT_NO_THROW(
            model::ChargeItem::fromXml(chargeItems[0].serializeToXmlString(), *getXmlValidator(),
                                       *StaticData::getInCodeValidator(), SchemaType::Gem_erxChargeItem));

        const auto professionOIDClaim = jwt.stringForClaim(JWT::professionOIDClaim);
        if(professionOIDClaim == profession_oid::oid_versicherter)
        {
            ASSERT_EQ(chargeItemResultBundle->getResourceCount(), 4);
            const auto bundles = chargeItemResultBundle->getResourcesByType<model::Bundle>("Bundle");
            EXPECT_EQ(bundles.size(), 3);

            const model::Bundle& kbvBundle = bundles[1];
            const model::Bundle& receipt = bundles[2];

            const auto& cadesBesTrustedCertDir = TestConfiguration::instance().getOptionalStringValue(
                    TestConfigurationKey::TEST_CADESBES_TRUSTED_CERT_DIR, "test/cadesBesSignature/certificates");
            const auto certs = CertificateDirLoader::loadDir(cadesBesTrustedCertDir);
            {
                // check KBV bundle signature
                Expect(expectedKbvBundle.has_value(), "expectedKbvBundle must be set");
                const auto signature = kbvBundle.getSignature();
                ASSERT_TRUE(signature.has_value());
                std::string signatureData;
                ASSERT_NO_THROW(signatureData = signature->data().value().data());
                CadesBesSignature cms(certs, signatureData);
                std::optional<model::KbvBundle> kbvBundleFromSignature;
                ASSERT_NO_THROW(kbvBundleFromSignature = model::KbvBundle::fromXml(
                    cms.payload(), *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(), SchemaType::KBV_PR_ERP_Bundle));
                EXPECT_FALSE(kbvBundleFromSignature->getSignature().has_value());
                EXPECT_EQ(expectedKbvBundle->serializeToJsonString(), kbvBundleFromSignature->serializeToJsonString());
            }
            {
                // check receipt signature
                const auto signature = receipt.getSignature();
                ASSERT_TRUE(signature.has_value());
                std::string signatureData;
                ASSERT_NO_THROW(signatureData = signature->data().value().data());
                CadesBesSignature cms(certs, signatureData);
                std::optional<model::ErxReceipt> receiptFromSignature;
                ASSERT_NO_THROW(receiptFromSignature = model::ErxReceipt::fromXml(
                    cms.payload(), *StaticData::getXmlValidator(), *StaticData::getInCodeValidator(), SchemaType::Gem_erxReceiptBundle));
                EXPECT_FALSE(receiptFromSignature->getSignature().has_value());
                EXPECT_EQ(expectedReceipt->serializeToJsonString(), receiptFromSignature->serializeToJsonString());
            }
        }
    }
    else
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        ASSERT_NO_FATAL_FAILURE(
            checkOperationOutcome(response.getBody(), std::string(contentType) == std::string(ContentMimeType::fhirJsonUtf8)/*isJson*/,
                                  expectedErrorCode.value()));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::sendInternal(std::tuple<ClientResponse, ClientResponse>& result,
                                       const ErpWorkflowTestBase::RequestArguments& args,
                                       const std::function<void(std::string&)>& manipEncryptedInnerRequest,
                                       const std::function<void(Header&)>& manipInnerRequestHeader)
{
    static std::regex XMLUtf8EncodingRegex{R"(^<\?xml[^>]*encoding="utf-8")", std::regex::icase|std::regex::optimize};
    using std::get;
    auto& outerResponse = get<0>(result);
    auto& innerResponse = get<1>(result);
    using namespace std::string_literals;

    Header::keyValueMap_t hdrFlds = args.headerFields;

    auto jwt = args.jwt.value_or(defaultJwt());

    hdrFlds.try_emplace(Header::Authorization, getAuthorizationBearerValueForJwt(jwt));
    hdrFlds.try_emplace(Header::Host, client->getHostHttpHeader());
    hdrFlds.try_emplace(Header::ContentType, args.contentType);

    ClientRequest innerRequest(
        Header(args.method, std::string(args.vauPath), Header::Version_1_1, std::move(hdrFlds), HttpStatus::Unknown),
        std::string(args.clearTextBody));

    std::string teeRequest;
    if(manipInnerRequestHeader)
    {
        manipInnerRequestHeader(innerRequest.header);
        teeRequest = creatUnvalidatedTeeRequest(client->getEciesCertificate(), innerRequest, jwt);
    }
    else
        teeRequest = creatTeeRequest(client->getEciesCertificate(), innerRequest, jwt);

    if(manipEncryptedInnerRequest)
        manipEncryptedInnerRequest(teeRequest);

    std::string outerPath = "/VAU/" + userPseudonym;
    if (userPseudonymType == UserPseudonymType::PreUserPseudonym)
    {
        outerPath += '-';
        outerPath.append(CmacSignature::SignatureSize * 2, '0');
    }

    ClientRequest outerRequest(
        Header(HttpMethod::POST,
            std::move(outerPath),
            Header::Version_1_1,
            { {Header::Host, client->getHostHttpHeader() },
              {Header::UserAgent, "vau-cpp-it-test"s},
              {Header::ContentType, "application/octet-stream"},
              {Header::XRequestId, Uuid().toString()}},
            HttpStatus::Unknown),
        teeRequest);
    const_cast<Header&>(outerRequest.getHeader()).setKeepAlive(true);

    outerResponse = client->send(outerRequest);

    // Verify the outer response
    if (!args.skipOuterResponseVerification)
    {
        ASSERT_EQ(outerResponse.getHeader().status(), HttpStatus::OK)
            << " header status was: " << toString(outerResponse.getHeader().status())
            << " OperationOutcome: " << outerResponse.getBody();
        ASSERT_TRUE(outerResponse.getHeader().hasHeader(Header::ContentType));
        ASSERT_EQ(outerResponse.getHeader().header(Header::ContentType).value(), "application/octet-stream");
    }

    if(outerResponse.getHeader().status() != HttpStatus::OK) // in this case there is no encrypted inner response
        return;

    // the inner response;
    try
    {
        innerResponse = teeProtocol.parseResponse(outerResponse);
        // verify Inner-* headers
        // this check makes only sense when testing without tls-proxy and erp-service
        // and against an erp-processing-context directly
        if (!runsInCloudEnv())
        {
            EXPECT_EQ(outerResponse.getHeader().header(Header::InnerResponseCode).value_or("XXX"),
                      std::to_string(static_cast<int>(innerResponse.getHeader().status())));

            auto expectedOperation = toExpectedOperation(args);
            EXPECT_EQ(outerResponse.getHeader().header(Header::InnerRequestOperation).value_or("XXX"),
                      expectedOperation);

            auto expectedRole = toExpectedRole(args);
            EXPECT_EQ(outerResponse.getHeader().header(Header::InnerRequestRole).value_or("XXX"),
                      expectedRole);
            {
                EXPECT_EQ(outerResponse.getHeader().header(Header::InnerRequestFlowtype).value_or("XXX"),
                          args.expectedInnerFlowType);
            }

            if (jwt.stringForClaim(JWT::clientIdClaim).has_value())
            {
                // Currently, this header field is optional and appears only when the token claim has a value.
                EXPECT_EQ(outerResponse.getHeader().header(Header::InnerRequestClientId).value_or("XXX"), toExpectedClientId(args));
            }
        }
        if (innerResponse.getHeader().contentType() == std::string{ContentMimeType::fhirXmlUtf8})
        {
            EXPECT_TRUE(regex_search(innerResponse.getBody(), XMLUtf8EncodingRegex));
        }
        if (args.expectedInnerStatus != HttpStatus::Unknown)
        {
            EXPECT_EQ(args.expectedInnerStatus, innerResponse.getHeader().status()) << innerResponse.getBody();
        }
    }
    catch (std::exception&)
    {
        // TODO: Kind of a workaround. There are some tests expecting to receive an invalid header.
        // There might be a better solution than reusing the "verifyOuterResponse" flag.
        if (!args.skipOuterResponseVerification)
        {
            throw;
        }
    }
}

std::string ErpWorkflowTestBase::creatTeeRequest(const Certificate& serverPublicKeyCertificate,
                                             const ClientRequest& request, const JWT& jwt)
{
    return teeProtocol.createRequest(serverPublicKeyCertificate, request, jwt);
}

std::string ErpWorkflowTestBase::creatUnvalidatedTeeRequest(const Certificate& serverPublicKeyCertificate,
                                                            const ClientRequest& request, const JWT& jwt)
{
    const auto serializedRequest = BoostBeastStringWriter::serializeRequest(request.getHeader(), request.getBody());
    return teeProtocol.createRequest(serverPublicKeyCertificate, serializedRequest, jwt);
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkTaskMeta(const rapidjson::Value& meta)
{
    using namespace std::string_literals;
    ASSERT_TRUE(meta.IsObject());
    const auto& profile = meta.FindMember("profile");
    ASSERT_NE(profile, meta.MemberEnd());
    ASSERT_TRUE(profile->value.IsArray());
    const auto& profileArr = profile->value.GetArray();
    ASSERT_EQ(profileArr.Size(), static_cast<rapidjson::SizeType>(1));
    ASSERT_TRUE(profile->value[0].IsString());
    ASSERT_EQ(std::string(model::NumberAsStringParserDocument::getStringValueFromValue(&profile->value[0])),
              ::model::ResourceVersion::versionizeProfile("https://gematik.de/fhir/StructureDefinition/ErxTask"));
    const auto& lastUpdated = meta.FindMember("lastUpdated");
    if (lastUpdated != meta.MemberEnd())
    {
        ASSERT_TRUE(lastUpdated->value.IsString());
        EXPECT_TRUE(regex_match(model::NumberAsStringParserDocument::getStringValueFromValue(&lastUpdated->value).data(), instantRegex));
    }
}
void ErpWorkflowTestBase::checkTaskSingleIdentifier(const rapidjson::Value& id)
{
    ASSERT_TRUE(id.IsObject());
}
void ErpWorkflowTestBase::checkTaskIdentifiers(const rapidjson::Value& identifiers)
{
    ASSERT_TRUE(identifiers.IsArray());
    for (const auto& id : identifiers.GetArray())
    {
        ASSERT_NO_FATAL_FAILURE(checkTaskSingleIdentifier(id));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkTask(const rapidjson::Value& task)
{
    const auto& meta = task.FindMember("meta");
    ASSERT_NE(meta, task.MemberEnd());
    ASSERT_NO_FATAL_FAILURE(checkTaskMeta(meta->value));
    const auto& identifier = task.FindMember("identifier");
    ASSERT_NE(identifier, task.MemberEnd());
    ASSERT_NO_FATAL_FAILURE(checkTaskIdentifiers(identifier->value));
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::checkOperationOutcome(const std::string& responseBody,
                                            bool isJson,
                                            const model::OperationOutcome::Issue::Type expectedErrorCode,
                                            const std::optional<std::string>& expectedIssueText,
                                            const std::optional<std::string>& expectedIssueDiagnostics) const
{
    std::optional<model::OperationOutcome> outcome;
    if(isJson)
    {
        ASSERT_NO_FATAL_FAILURE(outcome = model::OperationOutcome::fromJsonNoValidation(responseBody));
        ASSERT_NO_THROW(getJsonValidator()->validate(
            model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(outcome->jsonDocument()),SchemaType::fhir));
    }
    else
    {
        ASSERT_NO_FATAL_FAILURE(outcome = model::OperationOutcome::fromXml(responseBody, *getXmlValidator(),
                                                                           *StaticData::getInCodeValidator(),
                                                                           SchemaType::fhir));
    }
    const auto issues = outcome->issues();
    ASSERT_EQ(issues.size(), 1);
    ASSERT_EQ(issues[0].severity, model::OperationOutcome::Issue::Severity::error);
    ASSERT_EQ(issues[0].code, expectedErrorCode);

    if(expectedIssueText.has_value())
    {
        ASSERT_TRUE((issues[0].detailsText.has_value()));
        EXPECT_EQ(issues[0].detailsText.value(), expectedIssueText.value());
    }
    if(expectedIssueDiagnostics.has_value())
    {
        ASSERT_TRUE((issues[0].diagnostics.has_value()));
        EXPECT_EQ(issues[0].diagnostics.value(), expectedIssueDiagnostics.value());
    }
}

void ErpWorkflowTestBase::getTaskFromBundle(std::optional<model::Task>& task, const model::Bundle& bundle)
{
    auto taskList = bundle.getResourcesByType<model::Task>("Task");
    ASSERT_EQ(taskList.size(), 1);
    task = std::move(taskList.front());
}

std::tuple<ClientResponse, ClientResponse>
ErpWorkflowTestBase::send(const ErpWorkflowTestBase::RequestArguments& args,
                          const std::function<void(std::string&)>& manipEncryptedInnerRequest,
                          const std::function<void(Header&)>& manipInnerRequestHeader)
{
    std::tuple<ClientResponse, ClientResponse> result;
    sendInternal(result, args, manipEncryptedInnerRequest, manipInnerRequestHeader);
    return result;
}
std::optional<model::Task> ErpWorkflowTestBase::taskCreate(model::PrescriptionType workflowType,
                                                           HttpStatus expectedOuterStatus, HttpStatus expectedInnerStatus,
                                                           const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    std::optional<model::Task> task;
    taskCreateInternal(task, expectedOuterStatus, expectedInnerStatus, expectedErrorCode, workflowType);
    return task;
}
std::optional<model::Task>
ErpWorkflowTestBase::taskActivate(const model::PrescriptionId& prescriptionId,
                              const std::string& accessCode, const std::string& qesBundle,
                              HttpStatus expectedInnerStatus,
                              const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode,
                              const std::optional<std::string>& expectedIssueText,
                              const std::optional<std::string>& expectedIssueDiagnostics)
{
    std::optional<model::Task> task;
    taskActivateInternal(task, prescriptionId, accessCode, qesBundle, expectedInnerStatus, expectedErrorCode,
                         expectedIssueText, expectedIssueDiagnostics);
    return task;
}
std::optional<model::Bundle>
ErpWorkflowTestBase::taskAccept(const model::PrescriptionId& prescriptionId,
                            const std::string& accessCode, HttpStatus expectedInnerStatus,
                            const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    std::optional<model::Bundle> bundle;
    taskAcceptInternal(bundle, prescriptionId, accessCode, expectedInnerStatus, expectedErrorCode);
    return bundle;
}
std::optional<model::ErxReceipt>
ErpWorkflowTestBase::taskClose(const model::PrescriptionId& prescriptionId, const std::string& secret,
                               const std::string& kvnr, HttpStatus expectedInnerStatus,
                               const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode,
                               size_t numMedicationDispenses)
{
    std::optional<model::ErxReceipt> receipt;
    taskCloseInternal(receipt, prescriptionId, secret, kvnr, prescriptionId.toString(),
                      expectedInnerStatus, expectedErrorCode, {}, {}, "2020-09-21", numMedicationDispenses);
    return receipt;
}
void ErpWorkflowTestBase::taskClose_MedicationDispense_invalidPrescriptionId(
    const model::PrescriptionId& prescriptionId,
    const std::string& invalidPrescriptionId,
    const std::string& secret,
    const std::string& kvnr)
{
    std::optional<model::ErxReceipt> receipt;
    taskCloseInternal(receipt, prescriptionId, secret, kvnr, invalidPrescriptionId,
                      HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid, {}, {}, "2020-09-21", 1);
    EXPECT_TRUE(!receipt.has_value());
}
void ErpWorkflowTestBase::taskClose_MedicationDispense_invalidWhenHandedOver(
    const model::PrescriptionId& prescriptionId,
    const std::string& secret,
    const std::string& kvnr,
    const std::string& invalidWhenHandedOver)
{
    std::optional<model::ErxReceipt> receipt;
    const std::string expectedDiagnostics = "Element '{http://hl7.org/fhir}whenHandedOver', attribute 'value': '" +
        invalidWhenHandedOver + "' is not a valid value of the union type '{http://hl7.org/fhir}dateTime-primitive'.";
    taskCloseInternal(receipt, prescriptionId, secret, kvnr, prescriptionId.toString(),
                      HttpStatus::BadRequest, model::OperationOutcome::Issue::Type::invalid,
                      "XML error on line 0", expectedDiagnostics, invalidWhenHandedOver, 1);
    EXPECT_TRUE(!receipt.has_value());
}
void ErpWorkflowTestBase::taskAbort(const model::PrescriptionId& prescriptionId, JWT jwt,
                                const std::optional<std::string>& accessCode,
                                const std::optional<std::string>& secret,
                                const HttpStatus expectedStatus,
                                const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    std::string abortPath = "/Task/" + prescriptionId.toString() + "/$abort";
    if(secret.has_value())
        abortPath += ("?secret=" + secret.value());
    const auto professionOIDClaim = jwt.stringForClaim(JWT::professionOIDClaim);
    RequestArguments requestArguments{HttpMethod::POST, abortPath, {}};
    requestArguments.headerFields.emplace(Header::Authorization, getAuthorizationBearerValueForJwt(jwt));
    requestArguments.jwt = std::move(jwt);
    requestArguments.withExpectedInnerFlowType(std::to_string(magic_enum::enum_integer(prescriptionId.type())));
    if(accessCode.has_value())
        requestArguments.headerFields.emplace("X-AccessCode", accessCode.value());
    requestArguments.expectedInnerStatus = expectedStatus;
    ClientResponse serverResponse;
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) = send(requestArguments));
    ASSERT_EQ(serverResponse.getHeader().status(), expectedStatus);

    if(expectedStatus != HttpStatus::NoContent)
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        const auto isJson = professionOIDClaim.has_value() && professionOIDClaim == profession_oid::oid_versicherter;
        checkOperationOutcome(serverResponse.getBody(), isJson, expectedErrorCode.value());
    }
}

void ErpWorkflowTestBase::taskReject(const model::PrescriptionId& prescriptionId,
                                 const std::string& secret,
                                 HttpStatus expectedInnerStatus,
                                 const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    taskReject(prescriptionId.toString(), secret, expectedInnerStatus, expectedErrorCode,
               std::to_string(magic_enum::enum_integer(prescriptionId.type())));
}
void ErpWorkflowTestBase::taskReject(const std::string& prescriptionIdString,
                                     const std::string& secret,
                                     HttpStatus expectedInnerStatus,
                                     const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode,
                                     const std::string& expectedFlowType)
{
    std::string rejectPath = "/Task/" + prescriptionIdString + "/$reject?secret=" + secret;
    ClientResponse serverResponse;
    JWT jwt{ jwtApotheke() };
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) =
                                send(RequestArguments{HttpMethod::POST, rejectPath, {}}
                                         .withJwt(jwt)
                                         .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                                         .withExpectedInnerStatus(expectedInnerStatus)
                                         .withExpectedInnerFlowType(expectedFlowType)));
    ASSERT_EQ(serverResponse.getHeader().status(), expectedInnerStatus);

    if(expectedInnerStatus != HttpStatus::NoContent)
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        const auto professionOIDClaim = jwt.stringForClaim(JWT::professionOIDClaim);
        const auto isJson = professionOIDClaim.has_value() && professionOIDClaim == profession_oid::oid_versicherter;
        checkOperationOutcome(serverResponse.getBody(), isJson, expectedErrorCode.value());
    }
}

std::optional<model::Bundle>
ErpWorkflowTestBase::taskGetId(const model::PrescriptionId& prescriptionId, const std::string& kvnrOrTid,
                           const std::optional<std::string>& accessCodeOrSecret,
                           const HttpStatus expectedStatus,
                           const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode,
                           bool withAuditEvents)
{
    std::optional<model::Bundle> taskBundle;
    taskGetIdInternal(taskBundle, prescriptionId, kvnrOrTid, accessCodeOrSecret, expectedStatus, expectedErrorCode, withAuditEvents);
    return taskBundle;
}
std::optional<model::Bundle> ErpWorkflowTestBase::taskGet(const std::string& kvnr,
                                                      const std::string& searchArguments,
                                                      const HttpStatus expectedStatus,
                                                      const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    std::optional<model::Bundle> taskBundle;
    taskGetInternal(taskBundle, kvnr, searchArguments, expectedStatus, expectedErrorCode);
    return taskBundle;
}
std::tuple<std::string, std::string> ErpWorkflowTestBase::makeQESBundle(
    const std::string& kvnr,
    const model::PrescriptionId& prescriptionId,
    const model::Timestamp& timestamp)
{
    std::string qesBundle;
    makeQESBundleInternal(qesBundle, kvnr, prescriptionId, timestamp);
    return std::make_tuple(toCadesBesSignature(qesBundle, model::Timestamp::fromXsDate("2021-06-08")), qesBundle);
}
std::optional<model::MedicationDispense> ErpWorkflowTestBase::medicationDispenseGet(
    const std::string& kvnr, const std::string& medicationDispenseId)
{
    std::optional<model::MedicationDispense> medicationDispense;
    medicationDispenseGetInternal(medicationDispense, kvnr, medicationDispenseId);
    return medicationDispense;
}

std::optional<model::Bundle> ErpWorkflowTestBase::medicationDispenseGetAll(
    const std::string_view& searchArguments, const std::optional<JWT>& jwt)
{
    std::optional<model::Bundle> medicationDispenses;
    medicationDispenseGetAllInternal(medicationDispenses, searchArguments, jwt);
    return medicationDispenses;
}

std::optional<model::Communication> ErpWorkflowTestBase::communicationPost(
    const model::Communication::MessageType messageType,
    const model::Task& task,
    ActorRole senderRole, const std::string& sender,
    ActorRole recipientRole, const std::string& recipient,
    const std::string& content)
{
    std::optional<model::Communication> communicationResponse;
    communicationPostInternal(communicationResponse, messageType, task,
                              senderRole, sender, recipientRole, recipient, content);
    return communicationResponse;
}
std::optional<model::Bundle> ErpWorkflowTestBase::communicationsGet(const JWT& jwt, const std::string_view& requestArgs)
{
    std::optional<model::Bundle> communicationsBundle;
    communicationsGetInternal(communicationsBundle, jwt, requestArgs);
    return communicationsBundle;
}
std::optional<model::Communication> ErpWorkflowTestBase::communicationGet(const JWT& jwt, const Uuid& communicationId)
{
    std::optional<model::Communication> communication;
    communicationGetInternal(communication, jwt, communicationId);
    return communication;
}

void ErpWorkflowTestBase::communicationDelete(const JWT& jwt, const Uuid& communicationId)
{
    const auto& commId = communicationId.toString();
    TVLOG(0) << "Delete Communication: " << commId;
    ASSERT_NO_FATAL_FAILURE(
        send(RequestArguments{HttpMethod::DELETE, "/Communication/" + commId, {}}.withJwt(jwt).withExpectedInnerStatus(
            HttpStatus::NoContent)));
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::communicationDeleteAll(const JWT& jwt)
{
    std::optional<model::Bundle> commBundle;
    ASSERT_NO_FATAL_FAILURE(
        commBundle = communicationsGet(jwtVersicherter(), "sender=" + jwt.stringForClaim(JWT::idNumberClaim).value()));
    ASSERT_TRUE( commBundle.has_value());
    std::vector<model::Communication> comms;
    try
    {
        // this throws if the bundle doesn't contain any comms
        comms = commBundle->getResourcesByType<model::Communication>("Communication");
    }
    catch (const model::ModelException& e)
    {
        EXPECT_EQ(std::string{e.what()}, "entry array not present in Bundle");
    }
    for (const auto& c: comms)
    {
        ASSERT_NO_FATAL_FAILURE(communicationDelete(jwt, c.id().value()));
    }
}

std::size_t ErpWorkflowTestBase::countTaskBasedCommunications(
    const model::Bundle& communicationsBundle,
    const model::PrescriptionId& prescriptionId)
{
    std::size_t result = 0;
    if (communicationsBundle.getResourceCount() > 0)
    {
        const auto communications = communicationsBundle.getResourcesByType<model::Communication>("Communication");
        result = std::count_if(communications.begin(), communications.end(),
            [&prescriptionId](const auto& elem)
            {
                return (elem.prescriptionId().toString() == prescriptionId.toString());
            });
    }
    return result;
}

std::optional<model::Bundle> ErpWorkflowTestBase::auditEventGet(
    const std::string& kvnr,
    const std::string& language,
    const std::string& searchArguments)
{
    std::optional<model::Bundle> auditEventBundle;
    auditEventGetInternal(auditEventBundle, kvnr, language, searchArguments);
    return auditEventBundle;
}

std::optional<model::MetaData> ErpWorkflowTestBase::metaDataGet(const ContentMimeType& acceptContentType)
{
    std::optional<model::MetaData> metaData;
    metaDataGetInternal(metaData, acceptContentType);
    return metaData;
}

std::optional<model::Device> ErpWorkflowTestBase::deviceGet(const ContentMimeType& acceptContentType)
{
    std::optional<model::Device> device;
    deviceGetInternal(device, acceptContentType);
    return device;
}

std::optional<model::Consent> ErpWorkflowTestBase::consentPost(
    const std::string& kvnr,
    const model::Timestamp& dateTime,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    std::optional<model::Consent> consent;
    consentPostInternal(consent, kvnr, dateTime, expectedStatus, expectedErrorCode);
    return consent;
}

std::optional<model::Consent> ErpWorkflowTestBase::consentGet(
    const std::string& kvnr,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    std::optional<model::Consent> consent;
    consentGetInternal(consent, kvnr, expectedStatus, expectedErrorCode);
    return consent;
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::consentDelete(
    const std::string& consentId,
    const std::string& kvnr,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    ClientResponse serverResponse;
    ClientResponse outerResponse;
    JWT jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);

    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::DELETE, "/Consent/" + consentId, ""}
                     .withJwt(jwt)
                     .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                     .withExpectedInnerStatus(expectedStatus)));

    if(expectedStatus != HttpStatus::NoContent)
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(serverResponse.getBody(), true/*isJson*/, expectedErrorCode.value()));
    }
}

std::optional<model::ChargeItem> ErpWorkflowTestBase::chargeItemPost(
    const model::PrescriptionId& prescriptionId,
    const std::string& kvnr,
    const std::string& telematikId,
    const std::string& secret,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    std::optional<model::ChargeItem> chargeItem;
    chargeItemPostInternal(chargeItem, prescriptionId, kvnr, telematikId, secret, expectedStatus, expectedErrorCode);
    return chargeItem;
}

std::optional<model::ChargeItem> ErpWorkflowTestBase::chargeItemPut(
    //        const model::PrescriptionId& prescriptionId,
    const JWT& jwt,
    const ContentMimeType& contentType,
    //        const std::string& kvnr,
    //        const std::string& telematikId,
    const model::ChargeItem& inputChargeItem,
    const std::optional<std::string>& newMedicationDispenseString,
    const std::optional<std::tuple<bool, bool, bool>>& newMarking,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    std::optional<model::ChargeItem> chargeItem;
    chargeItemPutInternal(
        chargeItem, jwt, contentType, inputChargeItem, newMedicationDispenseString, newMarking, expectedStatus, expectedErrorCode);
    return chargeItem;
}

std::optional<model::Bundle> ErpWorkflowTestBase::chargeItemsGet(
    const JWT& jwt,
    const ContentMimeType& contentType,
    const std::string_view& searchArguments,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    std::optional<model::Bundle> chargeItemsBundle;
    chargeItemsGetInternal(chargeItemsBundle, jwt, contentType, searchArguments, expectedStatus, expectedErrorCode);
    return chargeItemsBundle;
}

std::optional<model::Bundle> ErpWorkflowTestBase::chargeItemGetId(
    const JWT& jwt,
    const ContentMimeType& contentType,
    const model::PrescriptionId& id,
    const std::optional<std::string_view>& accessCode,
    const std::optional<model::KbvBundle>& expectedKbvBundle,
    const std::optional<model::ErxReceipt>& expectedReceipt,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    std::optional<model::Bundle> chargeItemResultBundle;
    chargeItemGetIdInternal(chargeItemResultBundle, jwt, contentType, id, accessCode, expectedKbvBundle, expectedReceipt, expectedStatus, expectedErrorCode);
    return chargeItemResultBundle;
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::chargeItemDelete(
    const model::PrescriptionId& prescriptionId,
    const JWT& jwt,
    const HttpStatus expectedStatus,
    const std::optional<model::OperationOutcome::Issue::Type> expectedErrorCode)
{
    ClientResponse serverResponse;
    ClientResponse outerResponse;

    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::DELETE, "/ChargeItem/" + prescriptionId.toString(), ""}
                     .withJwt(jwt)
                     .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                     .withExpectedInnerStatus(expectedStatus)));

    if(expectedStatus != HttpStatus::NoContent)
    {
        Expect3(expectedErrorCode.has_value(), "expected error code must be set", std::logic_error);
        ASSERT_NO_FATAL_FAILURE(checkOperationOutcome(serverResponse.getBody(),
                                                      jwt.stringForClaim(JWT::professionOIDClaim) == profession_oid::oid_versicherter/*isJson*/,
                                                      expectedErrorCode.value()));
    }
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void ErpWorkflowTestBase::createClosedTask(
    std::optional<model::PrescriptionId>& createdId,
    std::optional<model::KbvBundle>& usedKbvBundle,
    std::optional<model::ErxReceipt>& closeReceipt,
    std::string& createdAccessCode,
    std::string& createdSecret,
    const model::PrescriptionType prescriptionType,
    const std::string& kvnr)
{
    // Create a closed task:
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(createdId, createdAccessCode, prescriptionType));
    ASSERT_TRUE(createdId.has_value());

    std::tuple<std::string, std::string> qesBundle;
    ASSERT_NO_THROW(qesBundle = makeQESBundle(kvnr, *createdId, model::Timestamp::now()));
    ASSERT_NO_THROW(usedKbvBundle = model::KbvBundle::fromXmlNoValidation(std::get<1>(qesBundle)));
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskActivate(*createdId, createdAccessCode, std::get<0>(qesBundle)));
    ASSERT_TRUE(task);

    std::optional<model::Bundle> acceptResultBundle;
    ASSERT_NO_FATAL_FAILURE(acceptResultBundle = taskAccept(*createdId, createdAccessCode));
    ASSERT_TRUE(acceptResultBundle);
    const auto tasks = acceptResultBundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    const auto secret = tasks[0].secret();
    ASSERT_TRUE(secret.has_value());
    createdSecret = *secret;

    ASSERT_NO_FATAL_FAILURE(closeReceipt = taskClose(*createdId, std::string(*secret), kvnr));
    ASSERT_TRUE(closeReceipt);
}

std::string ErpWorkflowTestBase::generateNewRandomKVNR()
{
    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    return kvnr;
}

void ErpWorkflowTestBase::generateNewRandomKVNR(std::string& kvnr)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int64_t> distrib(1, 999999999);
    for (int tries = 0; tries < 1000; ++tries)
    {
        std::ostringstream oss;
        oss << "X" << std::setfill('0') << std::setw(9) << distrib(gen);
        kvnr = oss.str();
        ASSERT_EQ(kvnr.size(), 10u);
        const auto& bundle = taskGet(kvnr);
        if (bundle->getResourceCount() == 0)
        {
            return;
        }
    }
    FAIL() << "could not find a free KVNR";
}

std::shared_ptr<XmlValidator> ErpWorkflowTestBase::getXmlValidator()
{
    return StaticData::getXmlValidator();
}

std::shared_ptr<JsonValidator> ErpWorkflowTestBase::getJsonValidator()
{
    return StaticData::getJsonValidator();
}

void ErpWorkflowTestBase::resetClient()
{
    client = TestClient::create(getXmlValidator());
}

void ErpWorkflowTestBase::writeCurrentTestOutputFile(
    const std::string& testOutput,
    const std::string& fileExtension,
    const std::string& marker)
{
    const ::testing::TestInfo* testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    std::string testName = testInfo->name();
    std::string testCaseName = testInfo->test_case_name();
    std::string fileName = testCaseName + "-" + testName;
    if (!marker.empty())
        fileName += marker;
    fileName += "." + fileExtension;
    FileHelper::writeFile(fileName, testOutput);
}
