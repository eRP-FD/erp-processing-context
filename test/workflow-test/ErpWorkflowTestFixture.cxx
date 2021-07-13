#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include "erp/crypto/CadesBesSignature.hxx"
#include "erp/erp-serverinfo.hxx"
#include "erp/fhir/Fhir.hxx"
#include "erp/fhir/FhirConverter.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/validation/XmlValidator.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestConfiguration.hxx"

#include <random>
#include <regex>

#undef DELETE


void ErpWorkflowTest::checkTaskCreate(
    std::optional<model::PrescriptionId>& createdId,
    std::string& createdAccessCode)
{
    // invoke POST /task/$create
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(task = taskCreate());
    ASSERT_TRUE(task);
    EXPECT_EQ(task->status(), model::Task::Status::draft);
    // extract prescriptionId and accessCode
    ASSERT_NO_THROW(createdId.emplace(task->prescriptionId()));
    ASSERT_TRUE(createdId);
    ASSERT_NO_THROW(createdAccessCode = task->accessCode());
    ASSERT_FALSE(createdAccessCode.empty());
}

void ErpWorkflowTest::checkTaskActivate(
    std::string& qesBundle,
    std::vector<model::Communication>& communications,
    const model::PrescriptionId& prescriptionId,
    const std::string& kvnr,
    const std::string& accessCode)
{
    // repare QES-Bundle for invokation of POST /task/<id>/$activate
    ASSERT_NO_THROW(qesBundle = makeQESBundle(kvnr, prescriptionId, model::Timestamp::now()));
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
        "Ist das Medikament bei Ihnen vorrätig?"));
    if (communicationResponse.has_value()) communications.emplace_back(std::move(communicationResponse.value()));
    ASSERT_NO_FATAL_FAILURE(communicationResponse = communicationPost(
        model::Communication::MessageType::Reply, task.value(),
        ActorRole::Pharmacists, telematicId.value(),
        ActorRole::Insurant, kvnr,
        "Wir haben das Medikament."));
    if (communicationResponse.has_value()) communications.emplace_back(std::move(communicationResponse.value()));
    ASSERT_NO_FATAL_FAILURE(communicationResponse = communicationPost(
        model::Communication::MessageType::Representative, task.value(),
        ActorRole::Insurant, kvnr,
        ActorRole::Representative, kvnrRepresentative,
        "Kannst Du das Medikament für mich holen?"));
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

void ErpWorkflowTest::checkTaskAccept(
    std::string& createdSecret,
    std::optional<model::Timestamp>& lastModifiedDate,
    const model::PrescriptionId& prescriptionId,
    const std::string&,
    const std::string& accessCode,
    const std::string& qesBundle)
{
    // invoke /task/<id>/$accept
    std::optional<model::Bundle> acceptResultBundle;
    ASSERT_NO_FATAL_FAILURE(acceptResultBundle = taskAccept(prescriptionId, accessCode));
    ASSERT_TRUE(acceptResultBundle);
    ASSERT_EQ(acceptResultBundle->getResourceCount(), 2);
    const auto tasks = acceptResultBundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    ASSERT_EQ(tasks[0].prescriptionId().toString(), prescriptionId.toString());
    ASSERT_EQ(tasks[0].status(), model::Task::Status::inprogress);
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
}

void ErpWorkflowTest::checkTaskClose(
    const model::PrescriptionId& prescriptionId,
    const std::string& kvnr,
    const std::string& secret,
    const model::Timestamp& lastModified,
    const std::vector<model::Communication>& communications)
{
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
    ASSERT_NO_FATAL_FAILURE(closeReceipt = taskClose(prescriptionId, secret, kvnr));
    ASSERT_TRUE(closeReceipt);
    const auto compositionResources = closeReceipt->getResourcesByType<model::Composition>("Composition");
    ASSERT_EQ(compositionResources.size(), 1);
    const auto& composition = compositionResources.front();
    EXPECT_TRUE(composition.telematicId().has_value());
    EXPECT_EQ(composition.telematicId().value(), telematicId.value());
    EXPECT_TRUE(composition.periodStart().has_value());
    EXPECT_EQ(composition.periodStart().value(), lastModified);
    EXPECT_TRUE(composition.periodEnd().has_value());
    EXPECT_TRUE(composition.date().has_value());
    const auto deviceResources = closeReceipt->getResourcesByType<model::Device>("Device");
    ASSERT_EQ(deviceResources.size(), 1);
    const auto& device = deviceResources.front();
    EXPECT_EQ(device.serialNumber(), ErpServerInfo::ReleaseVersion);
    EXPECT_EQ(device.version(), ErpServerInfo::ReleaseVersion);
    const auto signature = closeReceipt->getSignature();
    ASSERT_TRUE(signature.has_value());
    EXPECT_TRUE(signature->when().has_value());
    EXPECT_TRUE(signature->data().has_value());
    std::string signatureData;
    ASSERT_NO_THROW(signatureData = signature->data().value().data());
    std::initializer_list<Certificate> certs{
        Certificate::fromPemString(TestConfiguration::instance().getStringValue(TestConfigurationKey::TEST_GEM_RCA3)),
        Certificate::fromPemString(TestConfiguration::instance().getStringValue(TestConfigurationKey::TEST_GEM_KOMP_CA10))};
    CadesBesSignature cms(certs, signatureData);

    // This cannot be validated using XSD, because the Signature is defined mantatory, but not contained in the receipt
    // from the signature.
    const auto receiptFromSignature = model::ErxReceipt::fromXml(cms.payload());
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
    ASSERT_NO_THROW((void)Fhir::instance().converter().xmlStringToJsonWithValidation(
        bundles.back().serializeToXmlString(), *getXmlValidator(), SchemaType::Gem_erxReceiptBundle));
    const model::ErxReceipt receipt = model::ErxReceipt::fromXml(bundles.back().serializeToXmlString());
    // Must be the same as the result from the service call:
    EXPECT_EQ(canonicalJson(receipt.serializeToJsonString()), canonicalJson(closeReceipt->serializeToJsonString()));

    // Check medication dispense saved during /Task/Close
    ASSERT_TRUE(task->kvnr().has_value());
    std::optional<model::MedicationDispense> medicationDispenseForTask;
    ASSERT_NO_FATAL_FAILURE(getMedicationDispenseForTask(medicationDispenseForTask, prescriptionId, kvnr));
    ASSERT_TRUE(medicationDispenseForTask.has_value());
    EXPECT_EQ(medicationDispenseForTask->telematikId(), telematicId);

    // Check deletion of task related communication objects:
    checkCommunicationsDeleted(prescriptionId, kvnr, communications);
}

void ErpWorkflowTest::checkTaskAbort(
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
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(prescriptionId, kvnr, accessCode, HttpStatus::Gone));
    ASSERT_FALSE(taskBundle);   // cancelled Task can not be retrieved

    // check that no MedicationDispense has been created for the kvnr.
    std::optional<model::MedicationDispense> medicationDispenseForTask;
    ASSERT_NO_FATAL_FAILURE(getMedicationDispenseForTask(medicationDispenseForTask, prescriptionId, kvnr));
    ASSERT_FALSE(medicationDispenseForTask.has_value());

    // Check deletion of task related communication objects:
    checkCommunicationsDeleted(prescriptionId, kvnr, communications);
}

void ErpWorkflowTest::checkTaskReject(
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

void ErpWorkflowTest::checkCommunicationsDeleted(
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

void ErpWorkflowTest::checkAuditEventsFrom(const std::string& insurantKvnr)
{
    std::optional<model::Bundle> auditEventBundle;
    ASSERT_NO_FATAL_FAILURE(auditEventBundle = auditEventGet(insurantKvnr, "de", ""));
    ASSERT_TRUE(auditEventBundle);
    const auto auditEvents = auditEventBundle->getResourcesByType<model::AuditEvent>("AuditEvent");
    EXPECT_FALSE(auditEvents.empty());
    for(const auto& auditEvent : auditEvents)
        EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
}

void ErpWorkflowTest::checkAuditEvents(
    const model::PrescriptionId& prescriptionId,
    const std::string& insurantKvnr,
    const std::string& language,
    const model::Timestamp& startTime,
    const std::vector<std::string>& actorIdentifiers,
    const std::unordered_set<std::size_t>& actorTelematicIdIndices,
    const std::unordered_set<std::size_t>& noPrescriptionIdIndices,
    const std::vector<model::AuditEvent::SubType>& expectedActions)
{
    Expect(actorIdentifiers.size() == expectedActions.size(), "Invalid parameters");

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
        if(noPrescriptionIdIndices.count(i))
        {
            EXPECT_TRUE(!entityWhatIdentifierSystem.has_value());
            EXPECT_TRUE(!entityWhatIdentifierValue.has_value());

            // If no unique prescriptionId is available, the field AuditEvent.entity.description is filled with "+".
            EXPECT_EQ(auditEvent.entityDescription(), "+");
        }
        else
        {
            EXPECT_TRUE(entityWhatIdentifierSystem.has_value());
            EXPECT_EQ(entityWhatIdentifierSystem.value(), model::resource::naming_system::prescriptionID);
            EXPECT_TRUE(entityWhatIdentifierValue.has_value());
            EXPECT_EQ(entityWhatIdentifierValue.value(), prescriptionId.toString());
            EXPECT_EQ(auditEvent.entityDescription(), prescriptionId.toString());
        }
        EXPECT_FALSE(auditEvent.entityWhatReference().empty());
        EXPECT_EQ(auditEvent.entityName(), insurantKvnr);
    }
}


void ErpWorkflowTest::checkAuditEventSorting(
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

void ErpWorkflowTest::checkAuditEventSearchSubType(const std::string& kvnr)
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

void ErpWorkflowTest::checkAuditEventPaging(
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
    }
}

std::string ErpWorkflowTest::getAuthorizationBearerValueForJwt(const JWT& jwt)
{
    return "Bearer " + jwt.serialize();
}

std::string ErpWorkflowTest::toCadesBesSignature(const std::string& content)
{
    auto privKey = EllipticCurveUtils::pemToPrivatePublicKeyPair(SafeString{privateKey});
    auto cert = Certificate::fromPemString(certificate);
    CadesBesSignature cadesBesSignature{cert, privKey, content};
    return Base64::encode(cadesBesSignature.get());
}
std::string ErpWorkflowTest::toExpectedRole(const ErpWorkflowTest::RequestArguments& args) const
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
std::string ErpWorkflowTest::toExpectedOperation(const ErpWorkflowTest::RequestArguments& args)
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

    if (String::starts_with(vauPath, "/Task") || String::starts_with(vauPath, "/MedicationDispense"))
    {
        std::regex matcher(R"--((\d{3}\.){5}\d{2})--");
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
    return expectedOperation.append(" ").append(vauPath);
}
void ErpWorkflowTest::getMedicationDispenseForTask(
    std::optional<model::MedicationDispense>& medicationDispenseForTask,
    const model::PrescriptionId& prescriptionId, const std::string& kvnr)
{
    try
    {
        medicationDispenseForTask = medicationDispenseGet(kvnr, prescriptionId);
    }
    catch (const model::ModelException&)
    {
    }
}
void ErpWorkflowTest::communicationsGetInternal(std::optional<model::Bundle>& communicationsBundle,
                                                const JWT& jwt, const std::string_view& requestArgs)
{
    std::string getPath = "/Communication/";
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
                .withHeader(Header::Accept, accept)));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    if (serverResponse.getHeader().contentType() == std::string(ContentMimeType::fhirXmlUtf8))
    {
        ASSERT_NO_THROW(communicationsBundle = model::Bundle::fromXml(
            serverResponse.getBody(), *getXmlValidator(), SchemaType::Gem_erxBundle));
    }
    else
    {
        communicationsBundle = model::Bundle::fromJson(serverResponse.getBody());
        ASSERT_NO_THROW(getJsonValidator()->validate(
            model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(communicationsBundle->jsonDocument()),
            SchemaType::Gem_erxBundle));
    }
    if (communicationsBundle->getResourceCount() > 0)
    {
        for (const auto& communication :
             communicationsBundle->getResourcesByType<model::Communication>("Communication"))
        {
            ASSERT_NO_THROW((void) Fhir::instance().converter().xmlStringToJsonWithValidation(
                communication.serializeToXmlString(), *getXmlValidator(),
                model::Communication::messageTypeToSchemaType(communication.messageType())));
        }
    }
}
void ErpWorkflowTest::communicationGetInternal(std::optional<model::Communication>& communication,
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
    if (serverResponse.getHeader().status() == HttpStatus::NotFound)
    {
        ASSERT_TRUE(serverResponse.getBody().empty());
    }
    else
    {
        ASSERT_TRUE(!serverResponse.getBody().empty());
        if (serverResponse.getHeader().contentType() == std::string(ContentMimeType::fhirXmlUtf8))
        {
            ASSERT_NO_THROW(communication = model::Communication::fromXml(
                serverResponse.getBody(), *getXmlValidator(), SchemaType::fhir));
        }
        else
        {
            ASSERT_NO_THROW(communication = model::Communication::fromJson(serverResponse.getBody()));
        }
        if (communication.has_value())
        {
            ASSERT_NO_THROW(getJsonValidator()->validate(
                model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(communication->jsonDocument()),
                model::Communication::messageTypeToSchemaType(communication->messageType())));
        }
    }
}

void ErpWorkflowTest::communicationPostInternal(
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
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) = send(requestArguments));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::Created);
    if (serverResponse.getHeader().contentType() == std::string(ContentMimeType::fhirXmlUtf8))
    {
        ASSERT_NO_THROW(communicationResponse = model::Communication::fromXml(
            serverResponse.getBody(), *getXmlValidator(),
            model::Communication::messageTypeToSchemaType(messageType)));
    }
    else
    {
        communicationResponse = model::Communication::fromJson(serverResponse.getBody());
        ASSERT_NO_THROW(getJsonValidator()->validate(
            model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(communicationResponse->jsonDocument()),
            model::Communication::messageTypeToSchemaType(messageType)));
    }
}
void ErpWorkflowTest::medicationDispenseGetInternal(
    std::optional<model::MedicationDispense>& medicationDispense,
    const std::string& kvnr,
    const model::PrescriptionId& prescriptionId)
{
    std::string getPath = "/MedicationDispense/";
    getPath += prescriptionId.toString();
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
        medicationDispense = model::MedicationDispense::fromJson(serverResponse.getBody());
        ASSERT_TRUE(medicationDispense);
        ASSERT_NO_THROW(getJsonValidator()->validate(model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(
                                                        medicationDispense->jsonDocument()),
                                                        SchemaType::Gem_erxMedicationDispense));
    }
}

void ErpWorkflowTest::medicationDispenseGetAllInternal(std::optional<model::Bundle>& medicationDispenseBundle,
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
        std::tie(std::ignore, serverResponse) = send(RequestArguments{HttpMethod::GET, getPath, ""}.withJwt(theJwt)));
    ASSERT_EQ(serverResponse.getHeader().status(), HttpStatus::OK);
    medicationDispenseBundle = model::Bundle::fromJson(serverResponse.getBody());
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
            ASSERT_NO_THROW(getJsonValidator()->validate(copyToOriginalFormat(md.jsonDocument()),
                                                        SchemaType::Gem_erxMedicationDispense));
        }
    }
}


void ErpWorkflowTest::taskGetInternal(std::optional<model::Bundle>& taskBundle,
                                      const std::string& kvnr,
                                      const std::string& searchArguments,
                                      const HttpStatus expectedStatus)
{
    using namespace std::string_view_literals;
    RequestArguments args(HttpMethod::GET, "/Task", {});
    if (!searchArguments.empty())
    {
        args.vauPath.append("?").append(searchArguments);
    }

    args.jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    args.headerFields.emplace(Header::Authorization, getAuthorizationBearerValueForJwt(args.jwt.value()));
    ClientResponse response;
    ASSERT_NO_FATAL_FAILURE(tie(std::ignore, response) = send(args));
    ASSERT_EQ(response.getHeader().status(), expectedStatus);
    if(expectedStatus == HttpStatus::OK)
    {
        auto contentType = response.getHeader().header(Header::ContentType);
        ASSERT_TRUE(contentType);
        EXPECT_EQ(contentType.value(), "application/fhir+json;charset=utf-8"sv);
        ASSERT_NO_THROW(taskBundle = model::Bundle::fromJson(response.getBody()));
        ASSERT_TRUE(taskBundle);

        ASSERT_NO_THROW(getJsonValidator()->validate(
            model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(taskBundle->jsonDocument()),
            SchemaType::fhir));
        if (taskBundle->getResourceCount() > 0)
        {
            const auto tasks = taskBundle->getResourcesByType<model::Task>("Task");
            for (const auto& task : tasks)
            {
                ASSERT_NO_THROW(getJsonValidator()->validate(
                    model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(task.jsonDocument()),
                    SchemaType::Gem_erxTask));
            }
            EXPECT_LE(tasks.size(), taskBundle->getTotalSearchMatches());
        }
    }
}
void ErpWorkflowTest::taskGetIdInternal(std::optional<model::Bundle>& taskBundle,
                                        const model::PrescriptionId& prescriptionId,
                                        const std::string& kvnrOrTid,
                                        const std::optional<std::string>& accessCodeOrSecret,
                                        const HttpStatus expectedStatus,
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
    ClientResponse response;
    ASSERT_NO_FATAL_FAILURE(tie(std::ignore, response) = send(args));
    ASSERT_EQ(response.getHeader().status(), expectedStatus);
    if(response.getHeader().status() == HttpStatus::OK)
    {
        if (response.getHeader().contentType() == std::string(ContentMimeType::fhirJsonUtf8))
        {
            taskBundle = model::Bundle::fromJson(response.getBody());
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
            ASSERT_NO_THROW(getJsonValidator()->validate(
                model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(tasks[0].jsonDocument()),
                SchemaType::Gem_erxTask));

            auto patientConfirmationOrReceipt = taskBundle->getResourcesByType<model::Bundle>("Bundle");
            if(!patientConfirmationOrReceipt.empty())
            {
                auto schemaType = isPatient ? SchemaType::KBV_PR_ERP_Bundle : SchemaType::Gem_erxReceiptBundle;
                ASSERT_NO_THROW(
                    getJsonValidator()->validate(model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(
                                                     patientConfirmationOrReceipt[0].jsonDocument()),
                                                 schemaType));
            }
        }
    }
}

std::string ErpWorkflowTest::medicationDispense(const std::string& kvnr,
                                        const std::string& prescriptionIdForMedicationDispense)
{
    auto closeBody =
        FileHelper::readFileAsString(dataPath + "/medication_dispense_input1.xml");

    closeBody = String::replaceAll(closeBody, "###PRESCRIPTIONID###", prescriptionIdForMedicationDispense);
    closeBody = String::replaceAll(closeBody, "X234567890", kvnr);
    closeBody = String::replaceAll(closeBody, "3-SMC-B-Testkarte-883110000120312",
                                   jwtApotheke().stringForClaim(JWT::idNumberClaim).value());

    return closeBody;
}


void ErpWorkflowTest::taskCloseInternal(std::optional<model::ErxReceipt>& receipt,
                                        const model::PrescriptionId& prescriptionId,
                                        const std::string& secret,
                                        const std::string& kvnr,
                                        const std::string& prescriptionIdForMedicationDispense,
                                        const HttpStatus expectedInnerStatus)
{
    auto closeBody = medicationDispense(kvnr, prescriptionIdForMedicationDispense);

    std::string closePath = "/Task/" + prescriptionId.toString() + "/$close?secret=" + secret;
    ClientResponse serverResponse;
    JWT jwt{ jwtApotheke() };
    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::POST, closePath, closeBody, "application/fhir+xml"}
                .withJwt(jwt).withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))));
    ASSERT_EQ(serverResponse.getHeader().status(), expectedInnerStatus);
    if(expectedInnerStatus == HttpStatus::OK || !serverResponse.getBody().empty())
    {
        ASSERT_NO_THROW((void) Fhir::instance().converter().xmlStringToJsonWithValidation(
            serverResponse.getBody(), *getXmlValidator(), SchemaType::Gem_erxReceiptBundle));
        receipt = model::ErxReceipt::fromXml(serverResponse.getBody());
    }
}
void ErpWorkflowTest::taskAcceptInternal(std::optional<model::Bundle>& bundle,
                                         const model::PrescriptionId& prescriptionId,
                                         const std::string& accessCode, HttpStatus expectedInnerStatus)
{
    std::string acceptPath = "/Task/" + prescriptionId.toString() + "/$accept?ac=" + accessCode;
    ClientResponse serverResponse;
    JWT jwt{ jwtApotheke() };
    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::POST, acceptPath, {}}
                .withJwt(jwt).withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))));
    ASSERT_EQ(serverResponse.getHeader().status(), expectedInnerStatus);
    if (!serverResponse.getBody().empty())
    {
        ASSERT_NO_THROW(bundle = model::Bundle::fromXml(serverResponse.getBody(),
                                                        *getXmlValidator(),
                                                        SchemaType::Gem_erxBundle));
    }
}
void ErpWorkflowTest::taskActivateInternal(std::optional<model::Task>& task,
                                           const model::PrescriptionId& prescriptionId,
                                           const std::string& accessCode,
                                           const std::string& qesBundle,
                                           HttpStatus expectedInnerStatus)
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
    JWT jwt{jwtArzt()};
    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::POST, activatePath,
                                  activateBody, "application/fhir+xml"}
                .withJwt(jwt)
                .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
                .withHeader("X-AccessCode", accessCode)));
    ASSERT_EQ(serverResponse.getHeader().status(), expectedInnerStatus);
    if(expectedInnerStatus == HttpStatus::OK || !serverResponse.getBody().empty())
    {
        ASSERT_NO_THROW(task = model::Task::fromXml(serverResponse.getBody(), *getXmlValidator(), SchemaType::Gem_erxTask));
    }
}
void ErpWorkflowTest::makeQESBundleInternal(std::string& qesBundle, const std::string& kvnr,
                                            const model::PrescriptionId& prescriptionId,
                                            const model::Timestamp& now)
{
    qesBundle = R"(
<Bundle xmlns="http://hl7.org/fhir">
    <id value="281a985c-f25b-4aae-91a6-41ad744080b0"/>
    <meta>
        <profile value="https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Bundle|1.0.1"/>
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
              <profile value="https://fhir.kbv.de/StructureDefinition/KBV_PR_ERP_Composition|1.0.1" />
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
                <!--  Referenz auf Krankenkasse/KostentrĂ¤ger   -->
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
                            <code value="GKV"/>
                        </coding>
                    </type>
                    <system value="http://fhir.de/NamingSystem/gkv/kvid-10"/>
                    <value value=")" + kvnr + R"("/>
                </identifier>
                <name>
                    <use value="official"/>
                    <family value="Ludger Königsstein">
                        <extension url="http://hl7.org/fhir/StructureDefinition/humanname-own-name">
                            <valueString value="Königsstein"/>
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
</Bundle>

)";
}
void ErpWorkflowTest::taskCreateInternal(std::optional<model::Task>& task, HttpStatus expectedOuterStatus,
                                         HttpStatus expectedInnerStatus)
{
    using namespace std::string_view_literals;
    using model::Task;
    using model::Timestamp;

    static const char* create =
        "<Parameters xmlns=\"http://hl7.org/fhir\">\n"
        "  <parameter>\n"
        "    <name value=\"workflowType\"/>\n"
        "    <valueCoding>\n"
        "      <system value=\"https://gematik.de/fhir/CodeSystem/Flowtype\"/>\n"
        "      <code value=\"160\"/>\n"
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
                                    .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))));

    ASSERT_EQ(outerResponse.getHeader().status(), expectedOuterStatus);
    ASSERT_EQ(serverResponse.getHeader().status(), expectedInnerStatus);
    if (!serverResponse.getBody().empty())
    {
        ASSERT_NO_THROW(task = Task::fromXml(serverResponse.getBody(), *getXmlValidator(), SchemaType::Gem_erxTask));
    }
}

void ErpWorkflowTest::auditEventGetInternal(
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
         .withJwt(jwt)));
    ASSERT_EQ(response.getHeader().status(), HttpStatus::OK);
    auto contentType = response.getHeader().header(Header::ContentType);
    ASSERT_TRUE(contentType);
    EXPECT_EQ(contentType.value(), "application/fhir+json;charset=utf-8"sv);
    ASSERT_NO_THROW(auditEventBundle = model::Bundle::fromJson(response.getBody()));
    ASSERT_TRUE(auditEventBundle);

    ASSERT_NO_THROW(getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(auditEventBundle->jsonDocument()),
        SchemaType::fhir));
    if (auditEventBundle->getResourceCount() > 0)
    {
        auto auditEvents = auditEventBundle->getResourcesByType<model::AuditEvent>("AuditEvent");
        for (const auto& auditEvent : auditEvents)
        {
            ASSERT_NO_THROW(getJsonValidator()->validate(
                model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(auditEvent.jsonDocument()),
                SchemaType::Gem_erxAuditEvent));
        }
    }
}

void ErpWorkflowTest::metaDataGetInternal(
    std::optional<model::MetaData>& metaData,
    const ContentMimeType& acceptContentType)
{
    ClientResponse response;
    JWT jwt{JwtBuilder::testBuilder().makeJwtApotheke()};  // could be any valid jwt;
    ASSERT_NO_FATAL_FAILURE(tie(std::ignore, response) = send(RequestArguments{HttpMethod::GET, "/metadata", {}}
        .withJwt(jwt)
        .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
        .withHeader(Header::Accept, acceptContentType)));
    ASSERT_EQ(response.getHeader().status(), HttpStatus::OK);
    auto contentType = response.getHeader().header(Header::ContentType);
    ASSERT_TRUE(contentType);
    EXPECT_EQ(contentType.value(), static_cast<std::string>(ContentMimeType::fhirJsonUtf8));
    metaData = model::MetaData::fromJson(response.getBody());
    ASSERT_TRUE(metaData);
    ASSERT_NO_THROW(getJsonValidator()->validate(
        model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(metaData->jsonDocument()), SchemaType::fhir));
}

void ErpWorkflowTest::deviceGetInternal(
    std::optional<model::Device>& device,
    const ContentMimeType& acceptContentType)
{
    ClientResponse response;
    JWT jwt{ JwtBuilder::testBuilder().makeJwtApotheke() };  // could be any valid jwt;
    ASSERT_NO_FATAL_FAILURE(tie(std::ignore, response) = send(RequestArguments{ HttpMethod::GET, "/Device", {} }
        .withJwt(jwt)
        .withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))
        .withHeader(Header::Accept, acceptContentType)));
    ASSERT_EQ(response.getHeader().status(), HttpStatus::OK);
    auto contentType = response.getHeader().header(Header::ContentType);
    ASSERT_TRUE(contentType);
    EXPECT_EQ(contentType.value(), static_cast<std::string>(acceptContentType));
    if (static_cast<std::string>(acceptContentType) == static_cast<std::string>(ContentMimeType::fhirJsonUtf8))
    {
        device = model::Device::fromJson(response.getBody());
        ASSERT_TRUE(device);
        ASSERT_NO_THROW(getJsonValidator()->validate(
            model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(device->jsonDocument()), SchemaType::fhir));
    }
    else
    {
        ASSERT_NO_THROW(device = model::Device::fromXml(response.getBody(),
                                                        *StaticData::getXmlValidator(),
                                                        SchemaType::fhir));
    }
}

void ErpWorkflowTest::sendInternal(std::tuple<ClientResponse, ClientResponse>& result,
                                   const ErpWorkflowTest::RequestArguments& args)
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
        Header(args.method, std::string(args.vauPath), Header::Version_1_1, std::move(hdrFlds), HttpStatus::Unknown, false),
        std::string(args.clearTextBody));

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
              {Header::ContentType, "application/octet-stream"} },
            HttpStatus::Unknown,
            false),
        creatTeeRequest(
            getEciesCertificate(),
            innerRequest,
            jwt));

    outerResponse = client->send(outerRequest);

    // Verify the outer response
    if (!args.skipOuterResponseVerification)
    {
        ASSERT_EQ(outerResponse.getHeader().status(), HttpStatus::OK) << "header status was " << toString(outerResponse.getHeader().status());
        ASSERT_TRUE(outerResponse.getHeader().hasHeader(Header::ContentType));
        ASSERT_EQ(outerResponse.getHeader().header(Header::ContentType).value(), "application/octet-stream");
    }

    // the inner response;
    try
    {
        innerResponse = teeProtocol.parseResponse(outerResponse);

        // verify Inner-* headers
        // this check make only sense when testing without tls-proxy and erp-service
        // and against an erp-processing-context directly
        auto hostaddress = client->getHostAddress();
        if (hostaddress == "localhost" || hostaddress == "127.0.0.1")
        {
            EXPECT_EQ(outerResponse.getHeader().header(Header::InnerResponseCode).value_or("XXX"),
                      std::to_string(static_cast<int>(innerResponse.getHeader().status())));

            auto expectedOperation = toExpectedOperation(args);
            EXPECT_EQ(outerResponse.getHeader().header(Header::InnerRequestOperation).value_or("XXX"),
                      expectedOperation);

            auto expectedRole = toExpectedRole(args);
            EXPECT_EQ(outerResponse.getHeader().header(Header::InnerRequestRole).value_or("XXX"),
                      expectedRole);
        }
        if (innerResponse.getHeader().contentType() == std::string{ContentMimeType::fhirXmlUtf8})
        {
            EXPECT_TRUE(regex_search(innerResponse.getBody(), XMLUtf8EncodingRegex));
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

std::string ErpWorkflowTest::creatTeeRequest(const Certificate& serverPublicKeyCertificate,
                                             const ClientRequest& request, const JWT& jwt)
{
    return teeProtocol.createRequest(serverPublicKeyCertificate, request, jwt);
}

void ErpWorkflowTest::checkTaskMeta(const rapidjson::Value& meta)
{
    using namespace std::string_view_literals;
    ASSERT_TRUE(meta.IsObject());
    const auto& profile = meta.FindMember("profile");
    ASSERT_NE(profile, meta.MemberEnd());
    ASSERT_TRUE(profile->value.IsArray());
    const auto& profileArr = profile->value.GetArray();
    ASSERT_EQ(profileArr.Size(), static_cast<rapidjson::SizeType>(1));
    ASSERT_TRUE(profile->value[0].IsString());
    ASSERT_EQ(model::NumberAsStringParserDocument::getStringValueFromValue(&profile->value[0]),
        "https://gematik.de/fhir/StructureDefinition/ErxTask"sv);
    const auto& lastUpdated = meta.FindMember("lastUpdated");
    if (lastUpdated != meta.MemberEnd())
    {
        ASSERT_TRUE(lastUpdated->value.IsString());
        EXPECT_TRUE(regex_match(model::NumberAsStringParserDocument::getStringValueFromValue(&lastUpdated->value).data(), instantRegex));
    }
}
void ErpWorkflowTest::checkTaskSingleIdentifier(const rapidjson::Value& id)
{
    ASSERT_TRUE(id.IsObject());
}
void ErpWorkflowTest::checkTaskIdentifiers(const rapidjson::Value& identifiers)
{
    ASSERT_TRUE(identifiers.IsArray());
    for (const auto& id : identifiers.GetArray())
    {
        ASSERT_NO_FATAL_FAILURE(checkTaskSingleIdentifier(id));
    }
}
void ErpWorkflowTest::checkTask(const rapidjson::Value& task)
{
    const auto& meta = task.FindMember("meta");
    ASSERT_NE(meta, task.MemberEnd());
    ASSERT_NO_FATAL_FAILURE(checkTaskMeta(meta->value));
    const auto& identifier = task.FindMember("identifier");
    ASSERT_NE(identifier, task.MemberEnd());
    ASSERT_NO_FATAL_FAILURE(checkTaskIdentifiers(identifier->value));
}
void ErpWorkflowTest::getTaskFromBundle(std::optional<model::Task>& task, const model::Bundle& bundle)
{
    auto taskList = bundle.getResourcesByType<model::Task>("Task");
    ASSERT_EQ(taskList.size(), 1);
    task = std::move(taskList.front());
}

void ErpWorkflowTest::SetUp()
{
}

std::tuple<ClientResponse, ClientResponse>
ErpWorkflowTest::send(const ErpWorkflowTest::RequestArguments& args)
{
    std::tuple<ClientResponse, ClientResponse> result;
    sendInternal(result, args);
    return result;
}
std::optional<model::Task> ErpWorkflowTest::taskCreate(HttpStatus expectedOuterStatus, HttpStatus expectedInnerStatus)
{
    std::optional<model::Task> task;
    taskCreateInternal(task, expectedOuterStatus, expectedInnerStatus);
    return task;
}
std::optional<model::Task>
ErpWorkflowTest::taskActivate(const model::PrescriptionId& prescriptionId,
                              const std::string& accessCode, const std::string& qesBundle,
                              HttpStatus expectedInnerStatus)
{
    std::optional<model::Task> task;
    taskActivateInternal(task, prescriptionId, accessCode, qesBundle, expectedInnerStatus);
    return task;
}
std::optional<model::Bundle>
ErpWorkflowTest::taskAccept(const model::PrescriptionId& prescriptionId,
                            const std::string& accessCode, HttpStatus expectedInnerStatus)
{
    std::optional<model::Bundle> bundle;
    taskAcceptInternal(bundle, prescriptionId, accessCode, expectedInnerStatus);
    return bundle;
}
std::optional<model::ErxReceipt>
ErpWorkflowTest::taskClose(const model::PrescriptionId& prescriptionId, const std::string& secret,
                           const std::string& kvnr, HttpStatus expectedInnerStatus)
{
    std::optional<model::ErxReceipt> receipt;
    taskCloseInternal(receipt, prescriptionId, secret, kvnr, prescriptionId.toString(), expectedInnerStatus);
    return receipt;
}
void ErpWorkflowTest::taskClose_MedicationDispenseInvalidPrescriptionId(
    const model::PrescriptionId& prescriptionId,
    const std::string& invalidPrescriptionId,
    const std::string& secret,
    const std::string& kvnr)
{
    std::optional<model::ErxReceipt> receipt;
    taskCloseInternal(receipt, prescriptionId, secret, kvnr, invalidPrescriptionId, HttpStatus::BadRequest);
    EXPECT_TRUE(!receipt.has_value());
}
void ErpWorkflowTest::taskAbort(const model::PrescriptionId& prescriptionId, JWT jwt,
                                const std::optional<std::string>& accessCode,
                                const std::optional<std::string>& secret,
                                const HttpStatus expectedStatus)
{
    std::string abortPath = "/Task/" + prescriptionId.toString() + "/$abort";
    if(secret.has_value())
        abortPath += ("?secret=" + secret.value());
    RequestArguments requestArguments{HttpMethod::POST, abortPath, {}};
    requestArguments.headerFields.emplace(Header::Authorization, getAuthorizationBearerValueForJwt(jwt));
    requestArguments.jwt = std::move(jwt);
    if(accessCode.has_value())
        requestArguments.headerFields.emplace("X-AccessCode", accessCode.value());
    ClientResponse serverResponse;
    ASSERT_NO_FATAL_FAILURE(std::tie(std::ignore, serverResponse) = send(requestArguments));
    ASSERT_EQ(serverResponse.getHeader().status(), expectedStatus);
}

void ErpWorkflowTest::taskReject(const model::PrescriptionId& prescriptionId,
                                 const std::string& secret,
                                 HttpStatus expectedInnerStatus)
{
    taskReject(prescriptionId.toString(), secret, expectedInnerStatus);
}
void ErpWorkflowTest::taskReject(const std::string& prescriptionIdString,
                                 const std::string& secret,
                                 HttpStatus expectedInnerStatus)
{
    std::string rejectPath = "/Task/" + prescriptionIdString + "/$reject?secret=" + secret;
    ClientResponse serverResponse;
    JWT jwt{ jwtApotheke() };
    ASSERT_NO_FATAL_FAILURE(
        std::tie(std::ignore, serverResponse) =
            send(RequestArguments{HttpMethod::POST, rejectPath, {}}.withJwt(jwt).withHeader(Header::Authorization, getAuthorizationBearerValueForJwt(jwt))));
    ASSERT_EQ(serverResponse.getHeader().status(), expectedInnerStatus);
}

std::optional<model::Bundle>
ErpWorkflowTest::taskGetId(const model::PrescriptionId& prescriptionId, const std::string& kvnrOrTid,
                           const std::optional<std::string>& accessCodeOrSecret,
                           const HttpStatus expectedStatus,
                           bool withAuditEvents)
{
    std::optional<model::Bundle> taskBundle;
    taskGetIdInternal(taskBundle, prescriptionId, kvnrOrTid, accessCodeOrSecret, expectedStatus, withAuditEvents);
    return taskBundle;
}
std::optional<model::Bundle> ErpWorkflowTest::taskGet(const std::string& kvnr,
                                                      const std::string& searchArguments,
                                                      const HttpStatus expectedStatus)
{
    std::optional<model::Bundle> taskBundle;
    taskGetInternal(taskBundle, kvnr, searchArguments, expectedStatus);
    return taskBundle;
}
std::string ErpWorkflowTest::makeQESBundle(const std::string& kvnr,
                                           const model::PrescriptionId& prescriptionId,
                                           const model::Timestamp& timestamp)
{
    std::string qesBundle;
    makeQESBundleInternal(qesBundle, kvnr, prescriptionId, timestamp);
    return toCadesBesSignature(qesBundle);
}
std::optional<model::MedicationDispense> ErpWorkflowTest::medicationDispenseGet(
    const std::string& kvnr, const model::PrescriptionId& prescriptionId)
{
    std::optional<model::MedicationDispense> medicationDispense;
    medicationDispenseGetInternal(medicationDispense, kvnr, prescriptionId);
    return medicationDispense;
}

std::optional<model::Bundle> ErpWorkflowTest::medicationDispenseGetAll(
    const std::string_view& searchArguments, const std::optional<JWT>& jwt)
{
    std::optional<model::Bundle> medicationDispenses;
    medicationDispenseGetAllInternal(medicationDispenses, searchArguments, jwt);
    return medicationDispenses;
}

std::optional<model::Communication> ErpWorkflowTest::communicationPost(
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
std::optional<model::Bundle> ErpWorkflowTest::communicationsGet(const JWT& jwt, const std::string_view& requestArgs)
{
    std::optional<model::Bundle> communicationsBundle;
    communicationsGetInternal(communicationsBundle, jwt, requestArgs);
    return communicationsBundle;
}
std::optional<model::Communication> ErpWorkflowTest::communicationGet(const JWT& jwt, const Uuid& communicationId)
{
    std::optional<model::Communication> communication;
    communicationGetInternal(communication, jwt, communicationId);
    return communication;
}

void ErpWorkflowTest::communicationDelete(const JWT& jwt, const Uuid& communicationId)
{
    auto commId = communicationId.toString();
    TVLOG(0) << "Delete Communication: " << commId;
    ASSERT_NO_FATAL_FAILURE(send(RequestArguments{HttpMethod::DELETE, "/Communication/" + commId, {}}.withJwt(jwt)));
}

void ErpWorkflowTest::communicationDeleteAll(const JWT& jwt)
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

std::size_t ErpWorkflowTest::countTaskBasedCommunications(
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

std::optional<model::Bundle> ErpWorkflowTest::auditEventGet(
    const std::string& kvnr,
    const std::string& language,
    const std::string& searchArguments)
{
    std::optional<model::Bundle> auditEventBundle;
    auditEventGetInternal(auditEventBundle, kvnr, language, searchArguments);
    return auditEventBundle;
}

std::optional<model::MetaData> ErpWorkflowTest::metaDataGet(const ContentMimeType& acceptContentType)
{
    std::optional<model::MetaData> metaData;
    metaDataGetInternal(metaData, acceptContentType);
    return metaData;
}

std::optional<model::Device> ErpWorkflowTest::deviceGet(const ContentMimeType& acceptContentType)
{
    std::optional<model::Device> device;
    deviceGetInternal(device, acceptContentType);
    return device;
}

void ErpWorkflowTest::generateNewRandomKVNR(std::string& kvnr)
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

std::shared_ptr<XmlValidator> ErpWorkflowTest::getXmlValidator()
{
    return StaticData::getXmlValidator();
}

std::shared_ptr<JsonValidator> ErpWorkflowTest::getJsonValidator()
{
    return StaticData::getJsonValidator();
}

void ErpWorkflowTest::writeCurrentTestOutputFile(
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


Certificate ErpWorkflowTest::getEciesCertificate (void)
{
    const auto pemString = Configuration::instance().getOptionalStringValue(ConfigurationKey::ECIES_CERTIFICATE, "");
    if (pemString.empty())
        return Certificate::build().withPublicKey(MockCryptography::getEciesPublicKey()).build();
    else
        return Certificate::fromPemString(pemString);
}
