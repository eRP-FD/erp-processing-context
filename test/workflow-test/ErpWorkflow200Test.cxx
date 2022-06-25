/*
* (C) Copyright IBM Deutschland GmbH 2021
* (C) Copyright IBM Corp. 2021
*/

#include "erp/fhir/Fhir.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"


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
    ASSERT_NO_FATAL_FAILURE(consentDelete(std::string(consent->id().value()), kvnr));

    // Assure that consent does no longer exist
    ASSERT_NO_FATAL_FAILURE(getConsent = consentGet(kvnr));
    ASSERT_FALSE(getConsent.has_value());

    // assure that all charging data for this kvnr are deleted
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemsGet(
        JwtBuilder::testBuilder().makeJwtVersicherter(kvnr), ContentMimeType::fhirJsonUtf8));
    EXPECT_EQ(chargeItemsBundle->getResourceCount(), 0);

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents(
        { {}, prescriptionId, prescriptionId, prescriptionId, prescriptionId, {} },
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
        { prescriptionId, prescriptionId, {}, prescriptionId, prescriptionId },
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
    EXPECT_EQ(chargeItem1.id(), chargeItem2.id());
    EXPECT_EQ(chargeItem1.prescriptionId(), chargeItem2.prescriptionId());
    EXPECT_EQ(chargeItem1.subjectKvnr(), chargeItem2.subjectKvnr());
    EXPECT_EQ(chargeItem1.entererTelematikId(), chargeItem2.entererTelematikId());
    EXPECT_EQ(chargeItem1.enteredDate(), chargeItem2.enteredDate());
}

void checkEqualityMarking(
    const model::ChargeItem& chargeItem1,
    const model::ChargeItem& chargeItem2)
{
    ASSERT_TRUE(chargeItem1.markingFlag().has_value());
    ASSERT_TRUE(chargeItem2.markingFlag().has_value());
    EXPECT_EQ(chargeItem1.markingFlag()->getAllMarkings(), chargeItem2.markingFlag()->getAllMarkings());
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
        ASSERT_NO_FATAL_FAILURE(
            chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8, chargeItems[i].id(), std::nullopt,
                                                kbvBundles[i], closeReceipts[i]));
        const auto chargeItemFromBundle = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
        ASSERT_EQ(chargeItemFromBundle.size(), 1);
        ASSERT_NO_FATAL_FAILURE(checkEquality(chargeItemFromBundle[0], *createdChargeItems[i]));
    }

    // Get list of charge items by pharmacy
    ASSERT_NO_FATAL_FAILURE(
        chargeItemsBundle = chargeItemsGet(
            jwtApotheke(), ContentMimeType::fhirXmlUtf8,
            "entered-date=ge" + startTime.toXsDateTimeWithoutFractionalSeconds().substr(0, 19) + "Z&_sort=entered-date"));
    chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    EXPECT_EQ(chargeItems.size(), numOfTasks);
    // Read all charge items individually by pharmacy
    for(std::size_t i = 0; i < numOfTasks; ++i)
    {
        const auto accessCode = chargeItems[i].accessCode();
        ASSERT_TRUE(accessCode.has_value());
        ASSERT_NO_FATAL_FAILURE(
            chargeItemsBundle = chargeItemGetId(jwtApotheke(), ContentMimeType::fhirXmlUtf8, chargeItems[i].id(), accessCode, {}, {}));
        const auto chargeItemFromBundle = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
        ASSERT_EQ(chargeItemFromBundle.size(), 1);
        ASSERT_NO_FATAL_FAILURE(checkEqualityExceptSupportingInfo(chargeItemFromBundle[0], *createdChargeItems[i]));
        EXPECT_EQ(chargeItemFromBundle[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem),
                  createdChargeItems[i]->supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItem));
    }

    // Pharmacy deletes first charge item
    ASSERT_NO_FATAL_FAILURE(chargeItemDelete(*prescriptionIds[0], jwtApotheke()));
    // Insurant deletes third charge item
    ASSERT_NO_FATAL_FAILURE(chargeItemDelete(*prescriptionIds[2], jwtInsurant));

    // Check remaining charge items
    ASSERT_NO_FATAL_FAILURE(
        chargeItemsBundle = chargeItemsGet(
            jwtInsurant, ContentMimeType::fhirJsonUtf8,
            "entered-date=ge" + startTime.toXsDateTimeWithoutFractionalSeconds().substr(0, 19) + "Z&_sort=entered-date"));
    chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    ASSERT_EQ(chargeItems.size(), 2);
    EXPECT_EQ(chargeItems[0].id(), createdChargeItems[1]->id());
    EXPECT_EQ(chargeItems[1].id(), createdChargeItems[3]->id());

    // Change of marking by insurant
    std::optional<model::ChargeItem> chargeItem2Changed;
    ASSERT_NO_FATAL_FAILURE(
        chargeItem2Changed = chargeItemPut(jwtInsurant, ContentMimeType::fhirJsonUtf8, *createdChargeItems[1], {},
                                           std::make_tuple(true, false, true)));
    ASSERT_NO_FATAL_FAILURE(
        chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8, createdChargeItems[1]->id(),
                                            std::nullopt,
                                            kbvBundles[1], closeReceipts[1]));
    chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    EXPECT_EQ(chargeItems.size(), 1);
    ASSERT_NO_FATAL_FAILURE(checkEqualityBasic(chargeItems[0], *createdChargeItems[1]));
    ASSERT_NO_FATAL_FAILURE(checkEqualitySupportingInfoReference(chargeItems[0], *createdChargeItems[1]));
    EXPECT_TRUE(chargeItems[0].markingFlag()->getAllMarkings()["insuranceProvider"]);
    EXPECT_FALSE(chargeItems[0].markingFlag()->getAllMarkings()["subsidy"]);
    EXPECT_TRUE(chargeItems[0].markingFlag()->getAllMarkings()["taxOffice"]);

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
    ASSERT_NO_FATAL_FAILURE(
        chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8, createdChargeItems[1]->id(), accessCode,
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

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents(
        { {},
          prescriptionIds[0], prescriptionIds[0], prescriptionIds[0],
          prescriptionIds[1], prescriptionIds[1], prescriptionIds[1],
          prescriptionIds[2], prescriptionIds[2], prescriptionIds[2],
          prescriptionIds[3], prescriptionIds[3], prescriptionIds[3],
          prescriptionIds[0], prescriptionIds[1], prescriptionIds[2], prescriptionIds[3],
          prescriptionIds[0], prescriptionIds[2],
          prescriptionIds[1],
          prescriptionIds[1] },
        kvnr, "de", startTime,
        { kvnr, // POST Consent
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, // 4 * POST ChargeItem
          telematicIdPharmacy, kvnr, // DELETE ChargeItem by pharmacy / insurant
          kvnr, // PUT ChargeItem by insurant
          telematicIdPharmacy // PUT ChargeItem by pharmacy
        },
        { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 20 },
        { model::AuditEvent::SubType::create,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::update, model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::update, model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::update, model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::update, model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::create, model::AuditEvent::SubType::create, model::AuditEvent::SubType::create, model::AuditEvent::SubType::create,
          model::AuditEvent::SubType::del, model::AuditEvent::SubType::del,
          model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::update });
}
