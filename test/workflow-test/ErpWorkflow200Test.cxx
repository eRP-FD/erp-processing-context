/*
* (C) Copyright IBM Deutschland GmbH 2021
* (C) Copyright IBM Corp. 2021
*/

#include "erp/ErpRequirements.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <gtest/gtest.h>

TEST_F(ErpWorkflowTest, WF200Consent)//NOLINT(readability-function-cognitive-complexity)
{
    if(isUnsupportedFlowtype(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();

    const std::string kvnr = generateNewRandomKVNR();

    // Create consent
    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(kvnr, startTime));
    ASSERT_TRUE(consent.has_value());

    EXPECT_EQ(consent->id(), model::Consent::createIdString(model::Consent::Type::CHARGCONS, kvnr));
    EXPECT_EQ(consent->patientKvnr(), kvnr);
    EXPECT_EQ(consent->dateTime().toXsDateTimeWithoutFractionalSeconds(), startTime.toXsDateTimeWithoutFractionalSeconds());

    // Retrieve created consent
    std::optional<model::Consent> getConsent;
    ASSERT_NO_FATAL_FAILURE(getConsent = consentGet(kvnr));
    ASSERT_TRUE(getConsent.has_value());

    EXPECT_EQ(getConsent->id(), consent->id());
    EXPECT_EQ(consent->patientKvnr(), kvnr);
    EXPECT_EQ(consent->id(), model::Consent::createIdString(model::Consent::Type::CHARGCONS, kvnr));
    EXPECT_EQ(consent->dateTime().toXsDateTimeWithoutFractionalSeconds(), startTime.toXsDateTimeWithoutFractionalSeconds());

    // Create a closed task:
    std::optional<model::PrescriptionId> prescriptionId;
    std::optional<model::KbvBundle> kbvBundle;
    std::optional<model::ErxReceipt> closeReceipt;
    std::string accessCode;
    std::string secret;
    ASSERT_NO_FATAL_FAILURE(
        createClosedTask(prescriptionId, kbvBundle, closeReceipt, accessCode, secret,
                         model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, kvnr));

    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();

    // Create ChargeItem
    std::optional<model::ChargeItem> chargeItem;
    ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPost(*prescriptionId, kvnr, telematicIdPharmacy, secret));
    ASSERT_TRUE(chargeItem);

    std::optional<model::Bundle> chargeItemsBundle;
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemsGet(
        JwtBuilder::testBuilder().makeJwtVersicherter(kvnr), ContentMimeType::fhirJsonUtf8));
    EXPECT_EQ(chargeItemsBundle->getResourceCount(), 1);

    // Delete created consent
    ASSERT_NO_FATAL_FAILURE(consentDelete(kvnr));

    // Assure that consent does no longer exist
    ASSERT_NO_FATAL_FAILURE(getConsent = consentGet(kvnr));
    ASSERT_FALSE(getConsent.has_value());

    // assure that all charging data for this kvnr are deleted
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemsGet(
        JwtBuilder::testBuilder().makeJwtVersicherter(kvnr), ContentMimeType::fhirJsonUtf8));
    EXPECT_EQ(chargeItemsBundle->getResourceCount(), 0);

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents({"CHARGCONS-" + kvnr, prescriptionId->toString(), prescriptionId->toString(),
                      prescriptionId->toString(), prescriptionId->toString(), "CHARGCONS-" + kvnr},
                     kvnr, "de", startTime,
        { kvnr, // POST Consent
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdPharmacy, // POST ChargeItem
          kvnr },  // DELETE Consent
        { 1, 2, 3, 4 }/*actorTelematicIdIndices*/,
        { model::AuditEvent::SubType::create, model::AuditEvent::SubType::update, model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::create, model::AuditEvent::SubType::del });
}


TEST_F(ErpWorkflowTest, WF200TaskAcceptWithConsent)//NOLINT(readability-function-cognitive-complexity)
{
    if(isUnsupportedFlowtype(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();

    // invoke POST /task/$create
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(
        checkTaskCreate(prescriptionId, accessCode, model::PrescriptionType::apothekenpflichtigeArzneimittelPkv));

    std::string kvnr;
    generateNewRandomKVNR(kvnr);
    std::string qesBundle;
    std::vector<model::Communication> communications;
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

    const auto now = model::Timestamp::now();
    ASSERT_NO_FATAL_FAILURE(consentPost(kvnr, now));

    std::string secret;
    std::optional<model::Timestamp> lastModifiedDate;
    // invoke /task/<id>/$accept
    ASSERT_NO_FATAL_FAILURE(
        checkTaskAccept(secret, lastModifiedDate, *prescriptionId, kvnr, accessCode, qesBundle, true/*withConsent*/));

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents(
        {prescriptionId->toString(), prescriptionId->toString(), "CHARGCONS-" + kvnr, prescriptionId->toString(),
         prescriptionId->toString()},
        kvnr, "de", startTime,
        { telematicIdDoctor, kvnr, kvnr, telematicIdPharmacy, telematicIdPharmacy }, { 0, 3, 4 },
        { model::AuditEvent::SubType::update, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::create,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::read});
}


namespace
{

void checkEqualityBasic(
    const model::ChargeItem& chargeItem1,
    const model::ChargeItem& chargeItem2)
{
    EXPECT_EQ(chargeItem1.id()->toString(), chargeItem2.id()->toString());
    EXPECT_EQ(chargeItem1.prescriptionId()->toString(), chargeItem2.prescriptionId()->toString());
    EXPECT_EQ(chargeItem1.subjectKvnr(), chargeItem2.subjectKvnr());
    EXPECT_EQ(chargeItem1.entererTelematikId(), chargeItem2.entererTelematikId());
    EXPECT_EQ(chargeItem1.enteredDate(), chargeItem2.enteredDate());
}

void checkEqualityMarking(
    const model::ChargeItem& chargeItem1,
    const model::ChargeItem& chargeItem2)
{
    ASSERT_TRUE(chargeItem1.markingFlag().has_value() == chargeItem2.markingFlag().has_value());
    if (chargeItem1.markingFlag().has_value())
    {
        EXPECT_EQ(chargeItem1.markingFlag()->getAllMarkings(), chargeItem2.markingFlag()->getAllMarkings());
    }
}

void checkEqualitySupportingInfoReference(
    const model::ChargeItem& chargeItem1,
    const model::ChargeItem& chargeItem2)
{
    EXPECT_EQ(chargeItem1.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem),
              chargeItem2.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem));
    EXPECT_EQ(chargeItem1.supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt),
              chargeItem2.supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt));
    EXPECT_EQ(chargeItem1.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem),
              chargeItem2.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem));
}
//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkEquality(
    const model::ChargeItem& chargeItem1,
    const model::ChargeItem& chargeItem2)
{
    ASSERT_NO_FATAL_FAILURE(checkEqualityBasic(chargeItem1, chargeItem2));
    ASSERT_NO_FATAL_FAILURE(checkEqualityMarking(chargeItem1, chargeItem2));
    ASSERT_NO_FATAL_FAILURE(checkEqualitySupportingInfoReference(chargeItem1, chargeItem2));
}

//NOLINTNEXTLINE(readability-function-cognitive-complexity)
void checkEqualityExceptSupportingInfo(
    const model::ChargeItem& chargeItem1,
    const model::ChargeItem& chargeItem2)
{
    ASSERT_NO_FATAL_FAILURE(checkEqualityBasic(chargeItem1, chargeItem2));
    ASSERT_NO_FATAL_FAILURE(checkEqualityMarking(chargeItem1, chargeItem2));
}

} // anonymous namespace

TEST_F(ErpWorkflowTest, WF200ChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    if (isUnsupportedFlowtype(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();
    const std::string kvnr = generateNewRandomKVNR();

    // Create consent
    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(kvnr, startTime));
    ASSERT_TRUE(consent.has_value());

    // Create 4 closed tasks
    const std::size_t numOfTasks = 4;
    std::vector<std::optional<model::PrescriptionId>> prescriptionIds(numOfTasks);
    std::vector<std::optional<model::KbvBundle>> kbvBundles(numOfTasks);
    std::vector<std::optional<model::ErxReceipt>> closeReceipts(numOfTasks);
    std::vector<std::string> accessCodes(numOfTasks);
    std::vector<std::string> secrets(numOfTasks);
    for(std::size_t i = 0; i < numOfTasks; ++i)
    {
        ASSERT_NO_FATAL_FAILURE(
            createClosedTask(prescriptionIds[i], kbvBundles[i], closeReceipts[i], accessCodes[i], secrets[i],
                             model::PrescriptionType::apothekenpflichtigeArzneimittelPkv, kvnr));
    }
    // Create a charge item for each task
    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::vector<std::optional<model::ChargeItem>> createdChargeItems(numOfTasks);
    for(std::size_t i = 0; i < numOfTasks; ++i)
    {
        ASSERT_NO_FATAL_FAILURE(
            createdChargeItems[i] = chargeItemPost(*prescriptionIds[i], kvnr, telematicIdPharmacy, secrets[i]));
        EXPECT_FALSE(createdChargeItems[i]->containedBinary());
    }

    const auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);

    // Get list of charge items by insurant
    std::optional<model::Bundle> chargeItemsBundle;
    ASSERT_NO_FATAL_FAILURE(
        chargeItemsBundle = chargeItemsGet(
            jwtInsurant, ContentMimeType::fhirJsonUtf8,
            "entered-date=ge" + startTime.toXsDateTimeWithoutFractionalSeconds().substr(0, 19) + "Z&_sort=entered-date"));
    auto chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    EXPECT_EQ(chargeItems.size(), numOfTasks);
    // Read all charge items individually by insurant
    for(std::size_t i = 0; i < numOfTasks; ++i)
    {
        ASSERT_NO_FATAL_FAILURE(checkEqualityExceptSupportingInfo(chargeItems[i], *createdChargeItems[i]));

        closeReceipts[i]->removeSignature();
        ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8,
                                                                    chargeItems[i].id().value(), std::nullopt,
                                                                    kbvBundles[i], closeReceipts[i]));
        const auto chargeItemFromBundle = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
        ASSERT_EQ(chargeItemFromBundle.size(), 1);
        ASSERT_NO_FATAL_FAILURE(checkEquality(chargeItemFromBundle[0], *createdChargeItems[i]));
        EXPECT_FALSE(chargeItemFromBundle[0].containedBinary());
    }

    // Get list of charge items by pharmacy
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemsGet(
                                jwtInsurant, ContentMimeType::fhirXmlUtf8,
                                "entered-date=ge" + startTime.toXsDateTimeWithoutFractionalSeconds().substr(0, 19) +
                                    "Z&_sort=entered-date"));
    chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    EXPECT_EQ(chargeItems.size(), numOfTasks);
    // Read all charge items individually by pharmacy
    for(std::size_t i = 0; i < numOfTasks; ++i)
    {
        const auto accessCode = chargeItems[i].accessCode();
        ASSERT_TRUE(accessCode.has_value());
        ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemGetId(jwtApotheke(), ContentMimeType::fhirXmlUtf8,
                                                                    chargeItems[i].id().value(), accessCode, {}, {}));
        const auto chargeItemFromBundle = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
        ASSERT_EQ(chargeItemFromBundle.size(), 1);
        ASSERT_NO_FATAL_FAILURE(checkEqualityExceptSupportingInfo(chargeItemFromBundle[0], *createdChargeItems[i]));
        EXPECT_EQ(chargeItemFromBundle[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem),
                  createdChargeItems[i]->supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem));
        EXPECT_FALSE(chargeItemFromBundle[0].containedBinary());
    }

    // Insurant deletes first charge item
    ASSERT_NO_FATAL_FAILURE(chargeItemDelete(*prescriptionIds[0], jwtInsurant));
    // Insurant deletes third charge item
    ASSERT_NO_FATAL_FAILURE(chargeItemDelete(*prescriptionIds[2], jwtInsurant));

    // Check remaining charge items
    ASSERT_NO_FATAL_FAILURE(
        chargeItemsBundle = chargeItemsGet(
            jwtInsurant, ContentMimeType::fhirJsonUtf8,
            "entered-date=ge" + startTime.toXsDateTimeWithoutFractionalSeconds().substr(0, 19) + "Z&_sort=entered-date"));
    chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    ASSERT_EQ(chargeItems.size(), 2);
    EXPECT_EQ(chargeItems[0].id()->toString(), createdChargeItems[1]->id()->toString());
    EXPECT_EQ(chargeItems[1].id()->toString(), createdChargeItems[3]->id()->toString());

    // Change of marking by insurant
    std::optional<model::ChargeItem> chargeItem2Changed;
    ASSERT_NO_FATAL_FAILURE(
        chargeItem2Changed = chargeItemPut(jwtInsurant, ContentMimeType::fhirJsonUtf8, *createdChargeItems[1], {},
                                           std::make_tuple(true, false, true)));
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8,
                                                                createdChargeItems[1]->id().value(), std::nullopt,
                                                                kbvBundles[1], closeReceipts[1]));
    chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    EXPECT_EQ(chargeItems.size(), 1);
    ASSERT_NO_FATAL_FAILURE(checkEqualityBasic(chargeItems[0], *createdChargeItems[1]));
    ASSERT_NO_FATAL_FAILURE(checkEqualitySupportingInfoReference(chargeItems[0], *createdChargeItems[1]));
    EXPECT_FALSE(chargeItems[0].containedBinary());

    // Change of dispense item by pharmacy
    const auto dispenseBundleString = ResourceManager::instance().getStringResource("test/EndpointHandlerTest/dispense_item.xml");
    const Uuid uuid;
    auto dispenseBundle = model::Bundle::fromXmlNoValidation(dispenseBundleString);
    dispenseBundle.setId(uuid);
    std::optional<model::ChargeItem> chargeItem2Changed2;
    ASSERT_NO_FATAL_FAILURE(
        chargeItem2Changed2 = chargeItemPut(jwtApotheke(), ContentMimeType::fhirXmlUtf8, *chargeItem2Changed,
                                            dispenseBundle.serializeToXmlString(), {}));

    const auto accessCode = createdChargeItems[1]->accessCode();
    ASSERT_TRUE(accessCode.has_value());
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8,
                                                                createdChargeItems[1]->id().value(), accessCode,
                                                                kbvBundles[1], closeReceipts[1]));
    chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    EXPECT_EQ(chargeItems.size(), 1);
    ASSERT_NO_FATAL_FAILURE(checkEqualityExceptSupportingInfo(chargeItems[0], *chargeItem2Changed));
    EXPECT_EQ(chargeItems[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem),
              createdChargeItems[1]->supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItem));
    EXPECT_EQ(chargeItems[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt),
              createdChargeItems[1]->supportingInfoReference(model::ChargeItem::SupportingInfoType::receipt));
    EXPECT_EQ(chargeItems[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem),
              "Bundle/" + uuid.toString());
    EXPECT_FALSE(chargeItems[0].containedBinary());

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents(
        { "CHARGCONS-"+kvnr,
          prescriptionIds[0]->toString(), prescriptionIds[0]->toString(), prescriptionIds[0]->toString(),
          prescriptionIds[1]->toString(), prescriptionIds[1]->toString(), prescriptionIds[1]->toString(),
          prescriptionIds[2]->toString(), prescriptionIds[2]->toString(), prescriptionIds[2]->toString(),
          prescriptionIds[3]->toString(), prescriptionIds[3]->toString(), prescriptionIds[3]->toString(),
          prescriptionIds[0]->toString(), prescriptionIds[1]->toString(), prescriptionIds[2]->toString(), prescriptionIds[3]->toString(), // POST ChargeItem for each task
          prescriptionIds[0]->toString(), prescriptionIds[1]->toString(), prescriptionIds[2]->toString(), prescriptionIds[3]->toString(), // GET ChargeItem by insurant for each task
          prescriptionIds[0]->toString(), prescriptionIds[1]->toString(), prescriptionIds[2]->toString(), prescriptionIds[3]->toString(), // GET ChargeItem by pharmacy for each task
          prescriptionIds[0]->toString(), prescriptionIds[2]->toString(), // DELETE ChargeItem
          prescriptionIds[1]->toString(), // PUT ChargeItem
          prescriptionIds[1]->toString(), // GET ChargeItem by insurant
          prescriptionIds[1]->toString(), // PUT ChargeItem
          prescriptionIds[1]->toString(), // GET ChargeItem by insurant
        },
        kvnr, "de", startTime,
        { kvnr, // POST Consent
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, // 4 * POST ChargeItem
          kvnr, kvnr, kvnr, kvnr, // GET ChargeItem by insurant
          telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, // GET ChargeItem by pharmacy
          kvnr, kvnr, // DELETE ChargeItem by insurant
          kvnr, // PUT ChargeItem by insurant
          kvnr, // GET ChargeItem by insurant
          telematicIdPharmacy, // PUT ChargeItem by pharmacy
          kvnr // GET ChargeItem by insurant
        },
        { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 21, 22, 23, 24, 29 },
        { model::AuditEvent::SubType::create,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::update, model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::update, model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::update, model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::update, model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::create, model::AuditEvent::SubType::create, model::AuditEvent::SubType::create, model::AuditEvent::SubType::create,
          model::AuditEvent::SubType::read, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::read, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read, model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::del, model::AuditEvent::SubType::del,
          model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::read});
}

TEST_F(ErpWorkflowTest, WF200CommunicationsChargChange)
{
    if (isUnsupportedFlowtype(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv))
        GTEST_SKIP();

    std::optional<model::PrescriptionId> prescriptionId{};
    std::string accessCode{};
    ASSERT_NO_FATAL_FAILURE(
        checkTaskCreate(prescriptionId, accessCode, model::PrescriptionType::apothekenpflichtigeArzneimittelPkv));

    std::string qesBundle{};
    const std::string kvnr = generateNewRandomKVNR();
    std::vector<model::Communication> communications{};
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

    const auto now = model::Timestamp::now();
    ASSERT_NO_FATAL_FAILURE(consentPost(kvnr, now));

    std::optional<model::Bundle> acceptResultBundle;
    ASSERT_NO_FATAL_FAILURE(acceptResultBundle = taskAccept(*prescriptionId, accessCode));
    ASSERT_TRUE(acceptResultBundle);
    ASSERT_EQ(acceptResultBundle->getResourceCount(), 3);
    const auto tasks = acceptResultBundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    ASSERT_EQ(tasks[0].prescriptionId().toString(), prescriptionId->toString());
    ASSERT_EQ(tasks[0].status(), model::Task::Status::inprogress);
    EXPECT_FALSE(tasks[0].patientConfirmationUuid().has_value());
    const auto secretOpt = tasks[0].secret();
    ASSERT_TRUE(secretOpt.has_value());
    const std::string secret{*secretOpt};
    const auto binaryResources = acceptResultBundle->getResourcesByType<model::Binary>("Binary");
    ASSERT_EQ(binaryResources.size(), 1);
    ASSERT_TRUE(binaryResources[0].data().has_value());

    // the persisted CMS could be extended with embedded OCSP response
    try
    {
        CadesBesSignature cms1(std::string(binaryResources[0].data().value()));
        CadesBesSignature cms2(qesBundle);

        ASSERT_EQ(cms1.payload(), cms2.payload());
        ASSERT_EQ(cms1.getMessageDigest(), cms2.getMessageDigest());
        ASSERT_EQ(cms1.getSigningTime(), cms2.getSigningTime());
    }
    catch(const std::exception& e)
    {
        FAIL() << "exception by qes payload validation: " << e.what();
    }

    ASSERT_EQ(std::string(*binaryResources[0].id()), std::string(*tasks[0].healthCarePrescriptionUuid()));

    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(*prescriptionId, "x-dummy", secret));
    ASSERT_TRUE(taskBundle);
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(getTaskFromBundle(task, *taskBundle));
    ASSERT_TRUE(task);
    EXPECT_EQ(task->status(), model::Task::Status::inprogress);
    ASSERT_TRUE(task->secret().has_value());
    ASSERT_EQ(secret, task->secret().value());

    ASSERT_NO_FATAL_FAILURE(checkTaskClose(*prescriptionId, kvnr, secret, task->lastModifiedDate(), communications));

    auto telematikId = jwtApotheke().stringForClaim(JWT::idNumberClaim);
    ASSERT_TRUE(telematikId.has_value());

    A_22734.test("check existence of charge item for communication -- should return BadRequest");
    auto builder = CommunicationJsonStringBuilder(model::Communication::MessageType::ChargChangeReq);
    builder.setPrescriptionId(task->prescriptionId().toString());
    builder.setRecipient(ActorRole::Pharmacists, *telematikId);
    builder.setPayload("Test request");
    RequestArguments requestArguments(HttpMethod::POST, "/Communication", builder.createJsonString(), MimeType::fhirJson);
    auto jwt = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    requestArguments.jwt = jwt;
    requestArguments.headerFields.emplace(Header::Authorization, getAuthorizationBearerValueForJwt(jwt));
    requestArguments.expectedInnerStatus = HttpStatus::BadRequest;
    ClientResponse response{};
    ASSERT_NO_FATAL_FAILURE(tie(std::ignore, response) = send(requestArguments));
    ASSERT_EQ(response.getHeader().status(), HttpStatus::BadRequest);

    std::optional<model::ChargeItem> chargeItem{};
    ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPost(task->prescriptionId(), kvnr, *telematikId, secret));
    ASSERT_TRUE(chargeItem.has_value());

    A_22734.test("check existence of charge item for the communication");
    std::optional<model::Communication> commChangeChargeItemReq;
    ASSERT_NO_FATAL_FAILURE(commChangeChargeItemReq = communicationPost(
                                model::Communication::MessageType::ChargChangeReq,
                                *task,
                                ActorRole::Insurant,
                                kvnr,
                                ActorRole::Pharmacists,
                                *telematikId,
                                "Test request"));

    ASSERT_TRUE(commChangeChargeItemReq.has_value());
}

TEST_F(ErpWorkflowTest, WF200DeleteConsentRemovesChargeItemsAndCommunications)
{
    if (isUnsupportedFlowtype(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv))
        GTEST_SKIP();

    std::optional<model::PrescriptionId> prescriptionId{};
    std::string accessCode{};
    ASSERT_NO_FATAL_FAILURE(
        checkTaskCreate(prescriptionId, accessCode, model::PrescriptionType::apothekenpflichtigeArzneimittelPkv));

    std::string qesBundle{};
    const std::string kvnr = generateNewRandomKVNR();
    std::vector<model::Communication> communications{};
    ASSERT_NO_FATAL_FAILURE(checkTaskActivate(qesBundle, communications, *prescriptionId, kvnr, accessCode));

    const auto now = model::Timestamp::now();
    ASSERT_NO_FATAL_FAILURE(consentPost(kvnr, now));

    std::optional<model::Bundle> acceptResultBundle;
    ASSERT_NO_FATAL_FAILURE(acceptResultBundle = taskAccept(*prescriptionId, accessCode));
    ASSERT_TRUE(acceptResultBundle);
    ASSERT_EQ(acceptResultBundle->getResourceCount(), 3);
    const auto tasks = acceptResultBundle->getResourcesByType<model::Task>("Task");
    ASSERT_EQ(tasks.size(), 1);
    ASSERT_EQ(tasks[0].prescriptionId().toString(), prescriptionId->toString());
    ASSERT_EQ(tasks[0].status(), model::Task::Status::inprogress);
    EXPECT_FALSE(tasks[0].patientConfirmationUuid().has_value());
    const auto secretOpt = tasks[0].secret();
    ASSERT_TRUE(secretOpt.has_value());
    const std::string secret{*secretOpt};
    const auto binaryResources = acceptResultBundle->getResourcesByType<model::Binary>("Binary");
    ASSERT_EQ(binaryResources.size(), 1);
    ASSERT_TRUE(binaryResources[0].data().has_value());

    // the persisted CMS could be extended with embedded OCSP response
    try
    {
        CadesBesSignature cms1(std::string(binaryResources[0].data().value()));
        CadesBesSignature cms2(qesBundle);

        ASSERT_EQ(cms1.payload(), cms2.payload());
        ASSERT_EQ(cms1.getMessageDigest(), cms2.getMessageDigest());
        ASSERT_EQ(cms1.getSigningTime(), cms2.getSigningTime());
    }
    catch(const std::exception& e)
    {
        FAIL() << "exception by qes payload validation: " << e.what();
    }

    ASSERT_EQ(std::string(*binaryResources[0].id()), std::string(*tasks[0].healthCarePrescriptionUuid()));

    std::optional<model::Bundle> taskBundle;
    ASSERT_NO_FATAL_FAILURE(taskBundle = taskGetId(*prescriptionId, "x-dummy", secret));
    ASSERT_TRUE(taskBundle);
    std::optional<model::Task> task;
    ASSERT_NO_FATAL_FAILURE(getTaskFromBundle(task, *taskBundle));
    ASSERT_TRUE(task);
    EXPECT_EQ(task->status(), model::Task::Status::inprogress);
    ASSERT_TRUE(task->secret().has_value());
    ASSERT_EQ(secret, task->secret().value());

    ASSERT_NO_FATAL_FAILURE(checkTaskClose(*prescriptionId, kvnr, secret, task->lastModifiedDate(), communications));

    auto telematikId = jwtApotheke().stringForClaim(JWT::idNumberClaim);
    ASSERT_TRUE(telematikId.has_value());

    std::optional<model::ChargeItem> chargeItem{};
    ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPost(task->prescriptionId(), kvnr, *telematikId, secret));
    ASSERT_TRUE(chargeItem.has_value());

    A_22734.test("check existence of charge item for the communication");
    std::optional<model::Communication> commChangeChargeItemReq;
    ASSERT_NO_FATAL_FAILURE(commChangeChargeItemReq = communicationPost(
                                model::Communication::MessageType::ChargChangeReq,
                                *task,
                                ActorRole::Insurant,
                                kvnr,
                                ActorRole::Pharmacists,
                                *telematikId,
                                "Test request"));

    ASSERT_TRUE(commChangeChargeItemReq.has_value());
    ASSERT_TRUE(commChangeChargeItemReq->id().has_value());

    // Delete consent
    ASSERT_NO_FATAL_FAILURE(consentDelete(kvnr));

    // Assure that consent does no longer exists
    std::optional<model::Consent> consent{};
    ASSERT_NO_FATAL_FAILURE(consent = consentGet(kvnr));
    ASSERT_FALSE(consent.has_value());

    // assure that all charging data for this kvnr are deleted together will all communications
    std::optional<model::Bundle> chargeItemsBundle;
    const auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemsGet(jwtInsurant, ContentMimeType::fhirJsonUtf8));
    EXPECT_EQ(chargeItemsBundle->getResourceCount(), 0);
    EXPECT_FALSE(communicationGet(jwtInsurant, commChangeChargeItemReq->id().value()).has_value());
}
