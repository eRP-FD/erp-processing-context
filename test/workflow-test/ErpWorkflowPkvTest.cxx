/*
* (C) Copyright IBM Deutschland GmbH 2021, 2023
* (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
*/

#include "erp/ErpRequirements.hxx"
#include "erp/util/ByteHelper.hxx"
#include "test/util/CertificateDirLoader.h"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/TestUtils.hxx"
#include "test/workflow-test/ErpWorkflowTestFixture.hxx"

#include <chrono>
#include <gtest/gtest.h>


class ErpWorkflowPkvTestP : public ErpWorkflowTestP
{
public:
// GEMREQ-start createChargeItemsWithCommunication
    // Creates test data for the tests regarding deletion of ChargeItem or Consent
    void createChargeItemsWithCommunication(
        const std::string& kvnr,
        std::vector<std::optional<model::PrescriptionId>>& prescriptionIds,  // Ids of created Tasks/ChargeItems
        std::vector<std::optional<model::Communication>>& chargeItem1RelatedComm, // Comm. related to ChargeItem 1
        std::vector<std::optional<model::Communication>>& chargeItem2RelatedComm, // Comm. related to ChargeItem 2
        std::vector<std::optional<model::Communication>>& chargeItemUnrelatedComm) // Comm. unrelated to ChargeItems
    {
        const auto now = model::Timestamp::now();
        // Create consent
        ASSERT_NO_FATAL_FAILURE(consentPost(kvnr, now));

        // Create 2 closed tasks
        const std::size_t numOfTasks = 2;
        std::vector<std::optional<model::Task>> closedTasks(numOfTasks);
        std::vector<std::optional<model::KbvBundle>> kbvBundles(numOfTasks);
        std::vector<std::optional<model::ErxReceipt>> closeReceipts(numOfTasks);
        std::vector<std::string> accessCodes(numOfTasks);
        std::vector<std::string> secrets(numOfTasks);
        prescriptionIds.resize(numOfTasks);
        for(std::size_t i = 0; i < numOfTasks; ++i)
        {
            ASSERT_NO_FATAL_FAILURE(
                closedTasks[i] = createClosedTask(
                    prescriptionIds[i], kbvBundles[i], closeReceipts[i], accessCodes[i], secrets[i], GetParam(), kvnr));
        }
        // Create a charge item for each task
        auto telematikId = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
        std::vector<std::optional<model::ChargeItem>> createdChargeItems(numOfTasks);
        for(std::size_t i = 0; i < numOfTasks; ++i)
        {
            ASSERT_NO_FATAL_FAILURE(
                createdChargeItems[i] = chargeItemPost(*prescriptionIds[i], kvnr, telematikId, secrets[i]));
        }

        // Created communication related to first charge item
        chargeItem1RelatedComm.resize(2);
        std::optional<model::Communication> commChangeChargeItemReq1;
        ASSERT_NO_FATAL_FAILURE(
            chargeItem1RelatedComm[0] = communicationPost(
                model::Communication::MessageType::ChargChangeReq, *closedTasks[0],
                ActorRole::Insurant, kvnr, ActorRole::Pharmacists, telematikId, "Test request 1"));
        ASSERT_TRUE(chargeItem1RelatedComm[0].has_value());
        ASSERT_TRUE(chargeItem1RelatedComm[0]->id().has_value());
        ASSERT_NO_FATAL_FAILURE(
            chargeItem1RelatedComm[1] = communicationPost(
                model::Communication::MessageType::ChargChangeReply, *closedTasks[0],
                ActorRole::Pharmacists, telematikId, ActorRole::Insurant, kvnr, "Test reply 1"));
        ASSERT_TRUE(chargeItem1RelatedComm[1].has_value());
        ASSERT_TRUE(chargeItem1RelatedComm[1]->id().has_value());

        // Created communication related to second charge item
        chargeItem2RelatedComm.resize(1);
        ASSERT_NO_FATAL_FAILURE(
            chargeItem2RelatedComm[0] = communicationPost(
                model::Communication::MessageType::ChargChangeReq, *closedTasks[1],
                ActorRole::Insurant, kvnr, ActorRole::Pharmacists, telematikId, "Test request 2"));
        ASSERT_TRUE(chargeItem2RelatedComm[0].has_value());
        ASSERT_TRUE(chargeItem2RelatedComm[0]->id().has_value());

        using namespace std::chrono_literals;
        std::this_thread::sleep_for(1s);

        // The following communication is not related to charging information:
        chargeItemUnrelatedComm.resize(1);
        ASSERT_NO_FATAL_FAILURE(
            chargeItemUnrelatedComm[0] = communicationPost(
                model::Communication::MessageType::InfoReq, *closedTasks[0],
                ActorRole::Insurant, kvnr, ActorRole::Pharmacists, telematikId, "Test info request 1"));
        ASSERT_TRUE(chargeItemUnrelatedComm[0].has_value());
        ASSERT_TRUE(chargeItemUnrelatedComm[0]->id().has_value());
    }
// GEMREQ-end createChargeItemsWithCommunication

    void SetUp() override
    {
        ErpWorkflowTestP::SetUp();
        envVars = testutils::getOverlappingFhirProfileEnvironment();
    }

    void TearDown() override
    {
        envVars.clear();
        ErpWorkflowTestP::TearDown();
    }
private:
    std::vector<EnvironmentVariableGuard> envVars;
};


TEST_P(ErpWorkflowPkvTestP, PkvConsent)//NOLINT(readability-function-cognitive-complexity)
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();

    const auto kvnr = generateNewRandomKVNR();

    // Create consent
    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(kvnr.id(), startTime));
    ASSERT_TRUE(consent.has_value());

    EXPECT_EQ(consent->id(), model::Consent::createIdString(model::Consent::Type::CHARGCONS, kvnr));
    EXPECT_EQ(consent->patientKvnr(), kvnr);
    EXPECT_EQ(consent->dateTime().toXsDateTimeWithoutFractionalSeconds(), startTime.toXsDateTimeWithoutFractionalSeconds());

    // Retrieve created consent
    std::optional<model::Consent> getConsent;
    ASSERT_NO_FATAL_FAILURE(getConsent = consentGet(kvnr.id()));
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
        createClosedTask(prescriptionId, kbvBundle, closeReceipt, accessCode, secret, GetParam(), kvnr.id()));

    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();

    // Create ChargeItem
    std::optional<model::ChargeItem> chargeItem;
    ASSERT_NO_FATAL_FAILURE(chargeItem = chargeItemPost(*prescriptionId, kvnr.id(), telematicIdPharmacy, secret));
    ASSERT_TRUE(chargeItem);

    std::optional<model::Bundle> chargeItemsBundle;
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemsGet(
        JwtBuilder::testBuilder().makeJwtVersicherter(kvnr.id()), ContentMimeType::fhirJsonUtf8));
    EXPECT_EQ(chargeItemsBundle->getResourceCount(), 1);

    // Delete created consent
    ASSERT_NO_FATAL_FAILURE(consentDelete(kvnr.id()));

    // Assure that consent does no longer exist
    ASSERT_NO_FATAL_FAILURE(getConsent = consentGet(kvnr.id()));
    ASSERT_FALSE(getConsent.has_value());

    // assure that all charging data for this kvnr are deleted
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemsGet(
        JwtBuilder::testBuilder().makeJwtVersicherter(kvnr), ContentMimeType::fhirJsonUtf8));
    EXPECT_EQ(chargeItemsBundle->getResourceCount(), 0);

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents({"CHARGCONS-" + kvnr.id(), prescriptionId->toString(), prescriptionId->toString(),
                      prescriptionId->toString(), prescriptionId->toString(), "CHARGCONS-" + kvnr.id()},
                     kvnr.id(), "de", startTime,
        { kvnr.id(), // POST Consent
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdPharmacy, // POST ChargeItem
          kvnr.id() },  // DELETE Consent
        { 1, 2, 3, 4 }/*actorTelematicIdIndices*/,
        { model::AuditEvent::SubType::create, model::AuditEvent::SubType::update, model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::update, model::AuditEvent::SubType::create, model::AuditEvent::SubType::del });
}


TEST_P(ErpWorkflowPkvTestP, PkvTaskAcceptWithConsent)//NOLINT(readability-function-cognitive-complexity)
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();

    // invoke POST /task/$create
    std::optional<model::PrescriptionId> prescriptionId;
    std::string accessCode;
    ASSERT_NO_FATAL_FAILURE(
        checkTaskCreate(prescriptionId, accessCode, GetParam()));

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
    if (chargeItem1.entererTelematikId().has_value() && chargeItem2.entererTelematikId().has_value())
    {
        EXPECT_EQ(chargeItem1.entererTelematikId(), chargeItem2.entererTelematikId());
    }
    EXPECT_EQ(chargeItem1.enteredDate(), chargeItem2.enteredDate());
}

void checkEqualityMarking(
    const model::ChargeItem& chargeItem1,
    const model::ChargeItem& chargeItem2)
{
    ASSERT_TRUE(chargeItem1.markingFlags().has_value() == chargeItem2.markingFlags().has_value());
    if (chargeItem1.markingFlags().has_value())
    {
        EXPECT_EQ(chargeItem1.markingFlags()->getAllMarkings(), chargeItem2.markingFlags()->getAllMarkings());
    }
}

void checkEqualitySupportingInfoReference(
    const model::ChargeItem& chargeItem1,
    const model::ChargeItem& chargeItem2)
{
    EXPECT_EQ(chargeItem1.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle),
              chargeItem2.supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle));
    EXPECT_EQ(chargeItem1.supportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle),
              chargeItem2.supportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle));
    EXPECT_EQ(chargeItem1.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle),
              chargeItem2.supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle));
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

TEST_P(ErpWorkflowPkvTestP, PkvChargeItem)//NOLINT(readability-function-cognitive-complexity)
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();
    const auto kvnr = generateNewRandomKVNR();

    // Create consent
    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(kvnr.id(), startTime));
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
                             GetParam(), kvnr.id()));
    }
    // GEMREQ-start A_22614-02#createAccessCode
    // Create a charge item for each task
    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::vector<std::optional<model::ChargeItem>> createdChargeItems(numOfTasks);
    for(std::size_t i = 0; i < numOfTasks; ++i)
    {
        ASSERT_NO_FATAL_FAILURE(
            createdChargeItems[i] = chargeItemPost(*prescriptionIds[i], kvnr.id(), telematicIdPharmacy, secrets[i]));
        EXPECT_FALSE(createdChargeItems[i]->accessCode().has_value());
        EXPECT_FALSE(createdChargeItems[i]->containedBinary());
    }
    // GEMREQ-end A_22614-02#createAccessCode

    const auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr.id());

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
        EXPECT_FALSE(chargeItems[i].accessCode().has_value());

        closeReceipts[i]->removeSignature();

        // GEMREQ-start A_22614-02#readAccessCode
        ASSERT_NO_FATAL_FAILURE(
            chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8, chargeItems[i].id().value(),
                                                std::nullopt, kbvBundles[i], closeReceipts[i]));
        const auto chargeItemFromBundle = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
        ASSERT_EQ(chargeItemFromBundle.size(), 1);

        ASSERT_TRUE(chargeItemFromBundle[0].accessCode().has_value());
        EXPECT_NO_FATAL_FAILURE((void)ByteHelper::fromHex(*chargeItemFromBundle[0].accessCode()));
        accessCodes[i] = *chargeItemFromBundle[0].accessCode();
        // GEMREQ-end A_22614-02#readAccessCode
        EXPECT_NO_FATAL_FAILURE(checkEquality(chargeItemFromBundle[0], *createdChargeItems[i]));
        EXPECT_FALSE(chargeItemFromBundle[0].containedBinary());
    }
    // Read all charge items individually by pharmacy
    for(std::size_t i = 0; i < numOfTasks; ++i)
    {
        // GEMREQ-start A_22614-02#verifyAccessCode
        ASSERT_NO_FATAL_FAILURE(
            chargeItemsBundle = chargeItemGetId(jwtApotheke(), ContentMimeType::fhirXmlUtf8, prescriptionIds[i].value(),
                                                accessCodes[i], std::nullopt, std::nullopt));
        const auto chargeItemFromBundle = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
        ASSERT_EQ(chargeItemFromBundle.size(), 1);
        // GEMREQ-end A_22614-02#verifyAccessCode
        EXPECT_FALSE(chargeItemFromBundle[0].accessCode().has_value());
        EXPECT_NO_FATAL_FAILURE(checkEqualityExceptSupportingInfo(chargeItemFromBundle[0], *createdChargeItems[i]));
        EXPECT_EQ(chargeItemFromBundle[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle),
                  createdChargeItems[i]->supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle));
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
    EXPECT_FALSE(createdChargeItems[1]->isMarked());
    model::Parameters::MarkingFlag markingFlag{};
    markingFlag.taxOffice.value = true;
    markingFlag.insuranceProvider.value = true;
    markingFlag.subsidy.value = true;
    std::optional<model::ChargeItem> chargeItem2Changed;
    ASSERT_NO_FATAL_FAILURE(
        chargeItem2Changed = chargeItemPatch(*createdChargeItems[1]->id(), jwtInsurant, markingFlag));
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8,
                                                                createdChargeItems[1]->id().value(), std::nullopt,
                                                                kbvBundles[1], closeReceipts[1]));
    chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    EXPECT_EQ(chargeItems.size(), 1);
    ASSERT_NO_FATAL_FAILURE(checkEqualityBasic(chargeItems[0], *createdChargeItems[1]));
    ASSERT_NO_FATAL_FAILURE(checkEqualitySupportingInfoReference(chargeItems[0], *createdChargeItems[1]));
    EXPECT_FALSE(chargeItems[0].containedBinary());
    EXPECT_TRUE(chargeItems[0].isMarked());

    // Change one marking flag back to false (by insurant)
    markingFlag.taxOffice.value = false;
    ASSERT_NO_FATAL_FAILURE(
        chargeItem2Changed = chargeItemPatch(*createdChargeItems[1]->id(), jwtInsurant, markingFlag));
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8,
                                                                createdChargeItems[1]->id().value(), std::nullopt,
                                                                kbvBundles[1], closeReceipts[1]));
    chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    EXPECT_EQ(chargeItems.size(), 1);
    EXPECT_TRUE(chargeItems[0].isMarked());

    // Change marking back to false (by insurant)
    markingFlag.insuranceProvider.value = false;
    markingFlag.subsidy.value = false;
    ASSERT_NO_FATAL_FAILURE(
        chargeItem2Changed = chargeItemPatch(*createdChargeItems[1]->id(), jwtInsurant, markingFlag));
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8,
                                                                createdChargeItems[1]->id().value(), std::nullopt,
                                                                kbvBundles[1], closeReceipts[1]));
    chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    EXPECT_EQ(chargeItems.size(), 1);
    EXPECT_FALSE(chargeItems[0].isMarked());

    // Change of dispense item by pharmacy
    const auto dispenseBundleString = ResourceTemplates::medicationDispenseBundleXml({.medicationDispenses = {{}}});
    const Uuid uuid;
    auto dispenseBundle = model::Bundle::fromXmlNoValidation(dispenseBundleString);
    dispenseBundle.setId(uuid);
    std::variant<model::ChargeItem, model::OperationOutcome> chargeItem2Changed2;
    // remove the dispense item bundle reference, as we want to replace it
    chargeItem2Changed->deleteSupportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle);
    // GEMREQ-start A_22615-02#createAndReadNewAccessCode
    ASSERT_NO_FATAL_FAILURE(
        chargeItem2Changed2 = chargeItemPut(jwtApotheke(), ContentMimeType::fhirXmlUtf8, *chargeItem2Changed,
                                            dispenseBundle.serializeToXmlString(), chargeItem2Changed->accessCode().value()));
    ASSERT_TRUE(std::holds_alternative<model::ChargeItem>(chargeItem2Changed2));
    const auto newAccessCode = std::get<model::ChargeItem>(chargeItem2Changed2).accessCode();
    EXPECT_FALSE(newAccessCode.has_value());
    ASSERT_NO_FATAL_FAILURE(
        chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8,
                                            std::get<model::ChargeItem>(chargeItem2Changed2).id().value(), std::nullopt,
                                            kbvBundles[1], closeReceipts[1]));
    chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    EXPECT_EQ(chargeItems.size(), 1);
    ASSERT_TRUE(chargeItems[0].accessCode().has_value());
    EXPECT_NO_FATAL_FAILURE((void)ByteHelper::fromHex(*chargeItems[0].accessCode()));
    EXPECT_NE(chargeItems[0].accessCode().value(), accessCodes[1]); // should differ from the one created during POST;
    // GEMREQ-end A_22615-02#createAndReadNewAccessCode
    ASSERT_NO_FATAL_FAILURE(checkEqualityExceptSupportingInfo(chargeItems[0], *chargeItem2Changed));
    EXPECT_EQ(chargeItems[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle),
              createdChargeItems[1]->supportingInfoReference(model::ChargeItem::SupportingInfoType::prescriptionItemBundle));
    EXPECT_EQ(chargeItems[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle),
              createdChargeItems[1]->supportingInfoReference(model::ChargeItem::SupportingInfoType::receiptBundle));
    EXPECT_EQ(chargeItems[0].supportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle),
              Uuid{chargeItems[0].prescriptionId()->deriveUuid(model::uuidFeatureDispenseItem)}.toUrn());
    EXPECT_FALSE(chargeItems[0].containedBinary());

    // GEMREQ-start A_22615-02#verifyAccessCode
    ASSERT_NO_FATAL_FAILURE(
        chargeItemsBundle = chargeItemGetId(jwtApotheke(), ContentMimeType::fhirJsonUtf8,
                                            std::get<model::ChargeItem>(chargeItem2Changed2).id().value(),
                                            *chargeItems[0].accessCode(), kbvBundles[1], closeReceipts[1]));
    chargeItems = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    EXPECT_EQ(chargeItems.size(), 1);
    // GEMREQ-end A_22615-02#verifyAccessCode

    // Check audit events
    const auto telematicIdDoctor = jwtArzt().stringForClaim(JWT::idNumberClaim).value();
    checkAuditEvents(
        { "CHARGCONS-"+kvnr.id(),
          prescriptionIds[0]->toString(), prescriptionIds[0]->toString(), prescriptionIds[0]->toString(),
          prescriptionIds[1]->toString(), prescriptionIds[1]->toString(), prescriptionIds[1]->toString(),
          prescriptionIds[2]->toString(), prescriptionIds[2]->toString(), prescriptionIds[2]->toString(),
          prescriptionIds[3]->toString(), prescriptionIds[3]->toString(), prescriptionIds[3]->toString(),
          prescriptionIds[0]->toString(), prescriptionIds[1]->toString(), prescriptionIds[2]->toString(), prescriptionIds[3]->toString(), // POST ChargeItem for each task
          prescriptionIds[0]->toString(), prescriptionIds[1]->toString(), prescriptionIds[2]->toString(), prescriptionIds[3]->toString(), // GET ChargeItem by insurant for each task
          prescriptionIds[0]->toString(), prescriptionIds[1]->toString(), prescriptionIds[2]->toString(), prescriptionIds[3]->toString(), // GET ChargeItem by pharmacy for each task
          prescriptionIds[0]->toString(), prescriptionIds[2]->toString(), // DELETE ChargeItem
          prescriptionIds[1]->toString(), // PATCH ChargeItem
          prescriptionIds[1]->toString(), // GET ChargeItem by insurant
          prescriptionIds[1]->toString(), // PATCH ChargeItem
          prescriptionIds[1]->toString(), // GET ChargeItem by insurant
          prescriptionIds[1]->toString(), // PATCH ChargeItem
          prescriptionIds[1]->toString(), // GET ChargeItem by insurant
          prescriptionIds[1]->toString(), // PUT ChargeItem
          prescriptionIds[1]->toString(), // GET ChargeItem by insurant
          prescriptionIds[1]->toString()  // GET ChargeItem by pharmacy
        },
        kvnr.id(), "de", startTime,
        { kvnr.id(), // POST Consent
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdDoctor, telematicIdPharmacy, telematicIdPharmacy, // Task activate/accept/close
          telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, // 4 * POST ChargeItem
          kvnr.id(), kvnr.id(), kvnr.id(), kvnr.id(), // GET ChargeItem by insurant
          telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, telematicIdPharmacy, // GET ChargeItem by pharmacy
          kvnr.id(), kvnr.id(), // DELETE ChargeItem by insurant
          kvnr.id(), // PATCH ChargeItem by insurant
          kvnr.id(), // GET ChargeItem by insurant
          kvnr.id(), // PATCH ChargeItem by insurant
          kvnr.id(), // GET ChargeItem by insurant
          kvnr.id(), // PATCH ChargeItem by insurant
          kvnr.id(), // GET ChargeItem by insurant
          telematicIdPharmacy, // PUT ChargeItem by pharmacy
          kvnr.id(), // GET ChargeItem by insurant
          telematicIdPharmacy // GET ChargeItem by pharmacy
        },
        { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 21, 22, 23, 24, 33, 35 },
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
          model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::update,
          model::AuditEvent::SubType::read,
          model::AuditEvent::SubType::read});
}

// GEMREQ-start A_22125
TEST_P(ErpWorkflowPkvTestP, PkvChargeItemGetByIdKvnrCheck)//NOLINT(readability-function-cognitive-complexity)
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();
    const auto kvnr1 = generateNewRandomKVNR();

    // Create consent
    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(kvnr1.id(), startTime));
    ASSERT_TRUE(consent.has_value());

    // Create closed task
    std::optional<model::PrescriptionId> prescriptionId;
    std::optional<model::KbvBundle> kbvBundle;
    std::optional<model::ErxReceipt> closeReceipt;
    std::string accessCode;
    std::string secret;
    ASSERT_NO_FATAL_FAILURE(createClosedTask(prescriptionId, kbvBundle, closeReceipt, accessCode, secret, GetParam(), kvnr1.id()));
    // Create a charge item for task
    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::optional<model::ChargeItem> createdChargeItem;
    ASSERT_NO_FATAL_FAILURE(createdChargeItem = chargeItemPost(*prescriptionId, kvnr1.id(), telematicIdPharmacy, secret));

    const auto jwtInsurant1 = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr1.id());
    // Read created charge item by insurant
    closeReceipt->removeSignature();
    std::optional<model::Bundle> chargeItemsBundle;
    ASSERT_NO_FATAL_FAILURE(
        chargeItemsBundle = chargeItemGetId(jwtInsurant1, ContentMimeType::fhirJsonUtf8, createdChargeItem->id().value(),
                                            std::nullopt, kbvBundle, closeReceipt));
    const auto chargeItemFromBundle = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    ASSERT_EQ(chargeItemFromBundle.size(), 1);
    // Check that the found charge item is the created
    ASSERT_NO_FATAL_FAILURE(checkEquality(chargeItemFromBundle[0], *createdChargeItem));

    A_22125.test("kvnr check");
    const auto kvnr2 = generateNewRandomKVNR();
    const auto jwtInsurant2 = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr2.id());
    // Reading of charge item with invalid KVNR must be forbidden:
    ASSERT_NO_FATAL_FAILURE(
        chargeItemsBundle = chargeItemGetId(jwtInsurant2, ContentMimeType::fhirJsonUtf8, createdChargeItem->id().value(),
                                            std::nullopt, kbvBundle, closeReceipt,
                                            HttpStatus::Forbidden, model::OperationOutcome::Issue::Type::forbidden));
}
// GEMREQ-end A_22125

// GEMREQ-start A_22126
TEST_P(ErpWorkflowPkvTestP, PkvChargeItemGetByIdTelematikIdCheck)//NOLINT(readability-function-cognitive-complexity)
{
    if (isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();
    const std::string kvnr = generateNewRandomKVNR().id();

    // Create consent
    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(kvnr, startTime));
    ASSERT_TRUE(consent.has_value());

    // Create closed task
    std::optional<model::PrescriptionId> prescriptionId;
    std::optional<model::KbvBundle> kbvBundle;
    std::optional<model::ErxReceipt> closeReceipt;
    std::string accessCode;
    std::string secret;
    ASSERT_NO_FATAL_FAILURE(
        createClosedTask(prescriptionId, kbvBundle, closeReceipt, accessCode, secret, GetParam(), kvnr));
    closeReceipt->removeSignature();
    // Create a charge item for task
    const auto telematikIdPharmacy1 = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::optional<model::ChargeItem> postedChargeItem;
    ASSERT_NO_FATAL_FAILURE(postedChargeItem = chargeItemPost(*prescriptionId, kvnr, telematikIdPharmacy1, secret));

    std::optional<model::Bundle> chargeItemsBundle;
    std::string chargeItemAccessCode;
    {
        const auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
        ASSERT_NO_FATAL_FAILURE(
            chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8, prescriptionId.value(),
                                                std::nullopt, kbvBundle, closeReceipt));
        const auto chargeItemFromBundle = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
        ASSERT_EQ(chargeItemFromBundle.size(), 1);
        ASSERT_TRUE(chargeItemFromBundle[0].accessCode().has_value());
        chargeItemAccessCode = chargeItemFromBundle[0].accessCode().value();
    }

    const auto jwtPharmacy1 = JwtBuilder::testBuilder().makeJwtApotheke(telematikIdPharmacy1);
    // Read created charge item by pharmacy
    ASSERT_NO_FATAL_FAILURE(
        chargeItemsBundle = chargeItemGetId(jwtPharmacy1, ContentMimeType::fhirJsonUtf8, prescriptionId.value(),
                                            chargeItemAccessCode, kbvBundle, closeReceipt));
    const auto chargeItemFromBundle = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
    ASSERT_EQ(chargeItemFromBundle.size(), 1);
    // Check that the found charge item is the created
    ASSERT_NO_FATAL_FAILURE(checkEquality(chargeItemFromBundle[0], *postedChargeItem));

    A_22126.test("Telematik-ID check");
    const auto telematikIdPharmacy2 = telematikIdPharmacy1 + "_noaccess";
    const auto jwtPharmacy2 = JwtBuilder::testBuilder().makeJwtApotheke(telematikIdPharmacy2);
    // Reading of charge item with invalid KVNR must be forbidden:
    ASSERT_NO_FATAL_FAILURE(
        chargeItemsBundle = chargeItemGetId(jwtPharmacy2, ContentMimeType::fhirJsonUtf8, prescriptionId.value(),
                                            chargeItemAccessCode, kbvBundle, closeReceipt,
                                            HttpStatus::Forbidden, model::OperationOutcome::Issue::Type::forbidden));
}
// GEMREQ-end A_22126

// GEMREQ-start A_22877
TEST_P(ErpWorkflowPkvTestP, PkvChargeItemPatch)//NOLINT(readability-function-cognitive-complexity)
{
    if (isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();
    const std::string kvnr1 = generateNewRandomKVNR().id();

    // Create consent
    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(kvnr1, startTime));
    ASSERT_TRUE(consent.has_value());

    // Create closed task
    std::optional<model::PrescriptionId> prescriptionId;
    std::optional<model::KbvBundle> kbvBundle;
    std::optional<model::ErxReceipt> closeReceipt;
    std::string accessCode;
    std::string secret;
    ASSERT_NO_FATAL_FAILURE(
        createClosedTask(prescriptionId, kbvBundle, closeReceipt, accessCode, secret, GetParam(), kvnr1));
    // Create a charge item for task
    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::optional<model::ChargeItem> createdChargeItem;
    ASSERT_NO_FATAL_FAILURE(createdChargeItem = chargeItemPost(*prescriptionId, kvnr1, telematicIdPharmacy, secret));
    EXPECT_TRUE(createdChargeItem->markingFlags()->getAllMarkings().empty());

    const auto jwtInsurant1 = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr1);
    // Patch created charge item
    model::Parameters::MarkingFlag markingFlag{};
    markingFlag.taxOffice.value = true;
    std::optional<model::ChargeItem> chargeItemPatched;
    ASSERT_NO_FATAL_FAILURE(chargeItemPatched = chargeItemPatch(*createdChargeItem->id(), jwtInsurant1, markingFlag));
    EXPECT_EQ(chargeItemPatched->markingFlags()->getAllMarkings().size(), 3);
    EXPECT_TRUE(chargeItemPatched->markingFlags()->getAllMarkings().find("taxOffice")->second);
    EXPECT_FALSE(chargeItemPatched->markingFlags()->getAllMarkings().find("insuranceProvider")->second);
    EXPECT_FALSE(chargeItemPatched->markingFlags()->getAllMarkings().find("subsidy")->second);

    A_22877.test("kvnr check");
    const std::string kvnr2 = generateNewRandomKVNR().id();
    const auto jwtInsurant2 = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr2);
    // Patch of charge item with invalid KVNR must be forbidden:
    markingFlag.taxOffice.value = false;
    ASSERT_NO_FATAL_FAILURE(
        chargeItemPatched = chargeItemPatch(*createdChargeItem->id(), jwtInsurant2, markingFlag,
                                            HttpStatus::Forbidden, model::OperationOutcome::Issue::Type::forbidden));
}
// GEMREQ-end A_22877

// GEMREQ-start A_22114
TEST_P(ErpWorkflowPkvTestP, PkvChargeItemDelete)//NOLINT(readability-function-cognitive-complexity)
{
    if (isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();
    const std::string kvnr1 = generateNewRandomKVNR().id();

    // Create consent
    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(kvnr1, startTime));
    ASSERT_TRUE(consent.has_value());

    // Create 2 closed tasks
    const std::size_t numOfTasks = 2;
    std::vector<std::optional<model::PrescriptionId>> prescriptionIds(numOfTasks);
    std::vector<std::optional<model::KbvBundle>> kbvBundles(numOfTasks);
    std::vector<std::optional<model::ErxReceipt>> closeReceipts(numOfTasks);
    std::vector<std::string> accessCodes(numOfTasks);
    std::vector<std::string> secrets(numOfTasks);
    for(std::size_t i = 0; i < numOfTasks; ++i)
    {
        ASSERT_NO_FATAL_FAILURE(
            createClosedTask(prescriptionIds[i], kbvBundles[i], closeReceipts[i], accessCodes[i], secrets[i],
                             GetParam(), kvnr1));
    }
    // Create a charge item for each task
    const auto telematicIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::vector<std::optional<model::ChargeItem>> createdChargeItems(numOfTasks);
    for(std::size_t i = 0; i < numOfTasks; ++i)
    {
        ASSERT_NO_FATAL_FAILURE(
            createdChargeItems[i] = chargeItemPost(*prescriptionIds[i], kvnr1, telematicIdPharmacy, secrets[i]));
        EXPECT_FALSE(createdChargeItems[i]->containedBinary());
    }

    const auto jwtInsurant1 = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr1);
    // Delete first charge item
    ASSERT_NO_FATAL_FAILURE(chargeItemDelete(*prescriptionIds[0], jwtInsurant1));

    A_22114.test("kvnr check");
    const std::string kvnr2 = generateNewRandomKVNR().id();
    const auto jwtInsurant2 = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr2);
    // Deletion of charge item with invalid KVNR must be forbidden:
    ASSERT_NO_FATAL_FAILURE(
        chargeItemDelete(*prescriptionIds[1], jwtInsurant2,
                         HttpStatus::Forbidden, model::OperationOutcome::Issue::Type::forbidden));
}
// GEMREQ-end A_22114

// GEMREQ-start A_22117-01
TEST_P(ErpWorkflowPkvTestP, PkvChargeItemDeleteCommunications)//NOLINT(readability-function-cognitive-complexity)
{
    if (isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    const auto kvnr = generateNewRandomKVNR().id();

    // Create test data
    std::vector<std::optional<model::PrescriptionId>> prescriptionIds;  // Ids of created Tasks/ChargeItems
    std::vector<std::optional<model::Communication>> chargeItem1RelatedComm;
    std::vector<std::optional<model::Communication>> chargeItem2RelatedComm;
    std::vector<std::optional<model::Communication>> chargeItemUnrelatedComm;
    ASSERT_NO_FATAL_FAILURE(createChargeItemsWithCommunication(
        kvnr, prescriptionIds, chargeItem1RelatedComm, chargeItem2RelatedComm, chargeItemUnrelatedComm));

    const auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    // Delete first charge item
    ASSERT_NO_FATAL_FAILURE(chargeItemDelete(*prescriptionIds.at(0), jwtInsurant));

    // Try to read deleted item
    ASSERT_NO_FATAL_FAILURE(
        chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8, *prescriptionIds.at(0), std::nullopt,
                        std::nullopt, std::nullopt, HttpStatus::NotFound, model::OperationOutcome::Issue::Type::not_found));

    // Check communication, all entries related to first ChargeItem must be deleted
    std::optional<model::Bundle> commBundle;
    ASSERT_NO_FATAL_FAILURE(commBundle = communicationsGet(jwtInsurant, "_sort=sent"));
    ASSERT_EQ(commBundle->getResourceCount(), 2);
    auto communications =  commBundle->getResourcesByType<model::Communication>("Communication");
    ASSERT_EQ(communications.size(), 2);
    ASSERT_EQ(communications[0].id(), chargeItem2RelatedComm.at(0)->id());  // related to second ChargeItem;
    ASSERT_EQ(communications[1].id(), chargeItemUnrelatedComm.at(0)->id()); // not related to any ChargeItem;
}
// GEMREQ-end A_22117-01

// GEMREQ-start A_22146
// GEMREQ-start A_22215
TEST_P(ErpWorkflowPkvTestP, PkvChargeItemPut)//NOLINT(readability-function-cognitive-complexity)
{
    if (isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();
    const std::string kvnr = generateNewRandomKVNR().id();

    // Create consent
    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(kvnr, startTime));
    ASSERT_TRUE(consent.has_value());

    // Create closed task
    std::optional<model::PrescriptionId> prescriptionId;
    std::optional<model::KbvBundle> kbvBundle;
    std::optional<model::ErxReceipt> closeReceipt;
    std::string accessCode;
    std::string secret;
    ASSERT_NO_FATAL_FAILURE(
        createClosedTask(prescriptionId, kbvBundle, closeReceipt, accessCode, secret, GetParam(), kvnr));
    closeReceipt->removeSignature();
    // Create a charge item for task
    const auto telematikIdPharmacy1 = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::optional<model::ChargeItem> createdChargeItem;
    ASSERT_NO_FATAL_FAILURE(createdChargeItem = chargeItemPost(*prescriptionId, kvnr, telematikIdPharmacy1, secret));
    // if we want to replace the dispense information, remove the reference to the bundle and prepare
    // a new reference to a contained binary. The contained binary will be added on chargeItemPut()
    createdChargeItem->deleteSupportingInfoReference(model::ChargeItem::SupportingInfoType::dispenseItemBundle);

    std::string chargeItemAccessCode;
    {
        const auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
        std::optional<model::Bundle> chargeItemsBundle;
        ASSERT_NO_FATAL_FAILURE(
            chargeItemsBundle = chargeItemGetId(jwtInsurant, ContentMimeType::fhirJsonUtf8, prescriptionId.value(),
                                                std::nullopt, kbvBundle, closeReceipt));
        const auto chargeItemFromBundle = chargeItemsBundle->getResourcesByType<model::ChargeItem>("ChargeItem");
        ASSERT_EQ(chargeItemFromBundle.size(), 1);
        ASSERT_TRUE(chargeItemFromBundle[0].accessCode().has_value());
        chargeItemAccessCode = chargeItemFromBundle[0].accessCode().value();
    }
    const auto dispenseBundleString = ResourceTemplates::medicationDispenseBundleXml({.medicationDispenses = {{}}});
    auto dispenseBundle = model::Bundle::fromXmlNoValidation(dispenseBundleString);
    std::variant<model::ChargeItem, model::OperationOutcome> changedChargeItem;
    ASSERT_NO_FATAL_FAILURE(
        changedChargeItem = chargeItemPut(jwtApotheke(), ContentMimeType::fhirXmlUtf8, *createdChargeItem,
                                          dispenseBundle.serializeToXmlString(), chargeItemAccessCode));
    ASSERT_TRUE(std::holds_alternative<model::ChargeItem>(changedChargeItem));

    // Try to change with unauthorized TelematikID and validate that it is forbidden:
    A_22146.test("Telematik-ID check");
    const auto telematikIdPharmacy2 = telematikIdPharmacy1 + "_noaccess";
    const auto jwtPharmacy2 = JwtBuilder::testBuilder().makeJwtApotheke(telematikIdPharmacy2);
    ASSERT_NO_FATAL_FAILURE(
        changedChargeItem = chargeItemPut(jwtPharmacy2, ContentMimeType::fhirXmlUtf8, *createdChargeItem,
                                          dispenseBundle.serializeToXmlString(), chargeItemAccessCode,
                                          HttpStatus::Forbidden, model::OperationOutcome::Issue::Type::forbidden));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(changedChargeItem));
// GEMREQ-end A_22146

    A_22215.test("Consent check");
    // Set KVNR of ChargeItem to the KVNR of an insurant without consent:
    const std::string kvnrNoConsent = generateNewRandomKVNR().id();
    createdChargeItem->setSubjectKvnr(kvnrNoConsent);
    // Must fail because of missing consent:
    ASSERT_NO_FATAL_FAILURE(
        changedChargeItem = chargeItemPut(jwtApotheke(), ContentMimeType::fhirXmlUtf8, *createdChargeItem,
                                          dispenseBundle.serializeToXmlString(), chargeItemAccessCode,
                                          HttpStatus::Forbidden, model::OperationOutcome::Issue::Type::forbidden));
    ASSERT_TRUE(std::holds_alternative<model::OperationOutcome>(changedChargeItem));
}
// GEMREQ-end A_22215

TEST_P(ErpWorkflowPkvTestP, PkvCommunicationsChargChange)
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    std::optional<model::PrescriptionId> prescriptionId{};
    std::string accessCode{};
    ASSERT_NO_FATAL_FAILURE(checkTaskCreate(prescriptionId, accessCode, GetParam()));

    std::string qesBundle{};
    const std::string kvnr = generateNewRandomKVNR().id();
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

// GEMREQ-start A_22157
// GEMREQ-start A_22158
TEST_P(ErpWorkflowPkvTestP, PkvDeleteConsentRemovesChargeItemsAndCommunications)
{
    if(isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    const auto kvnr = generateNewRandomKVNR().id();

    // Create test data
    std::vector<std::optional<model::PrescriptionId>> prescriptionIds;  // Ids of created Tasks/ChargeItems
    std::vector<std::optional<model::Communication>> chargeItem1RelatedComm;
    std::vector<std::optional<model::Communication>> chargeItem2RelatedComm;
    std::vector<std::optional<model::Communication>> chargeItemUnrelatedComm;
    ASSERT_NO_FATAL_FAILURE(createChargeItemsWithCommunication(
        kvnr, prescriptionIds, chargeItem1RelatedComm, chargeItem2RelatedComm, chargeItemUnrelatedComm));

    // Delete consent
    ASSERT_NO_FATAL_FAILURE(consentDelete(kvnr));

    // Assure that consent does no longer exist
    A_22158.test("Consent resource for this kvnr is deleted");
    std::optional<model::Consent> consent{};
    ASSERT_NO_FATAL_FAILURE(consent = consentGet(kvnr));
    ASSERT_FALSE(consent.has_value());

    // Assure that all charging data for this kvnr are deleted
    A_22157.test("All ChargeItems for this kvnr are deleted");
    std::optional<model::Bundle> chargeItemsBundle;
    const auto jwtInsurant = JwtBuilder::testBuilder().makeJwtVersicherter(kvnr);
    ASSERT_NO_FATAL_FAILURE(chargeItemsBundle = chargeItemsGet(jwtInsurant, ContentMimeType::fhirJsonUtf8));
    EXPECT_EQ(chargeItemsBundle->getResourceCount(), 0);

    // Check communication, only info request which is not related to consent should remain
    A_22157.test("All ChargeItem related communication resources for this kvnr are deleted");
    std::optional<model::Bundle> commBundle;
    ASSERT_NO_FATAL_FAILURE(commBundle = communicationsGet(jwtInsurant));
    ASSERT_EQ(commBundle->getResourceCount(), 1);
    auto communications =  commBundle->getResourcesByType<model::Communication>("Communication");
    ASSERT_EQ(communications.size(), 1);
    ASSERT_EQ(communications.front().id(), chargeItemUnrelatedComm.at(0)->id());
}
// GEMREQ-end A_22157
// GEMREQ-end A_22158

TEST_P(ErpWorkflowPkvTestP, PkvChargeItemMultiplePostSameTask)//NOLINT(readability-function-cognitive-complexity)
{
    if (isUnsupportedFlowtype(GetParam()))
        GTEST_SKIP();

    model::Timestamp startTime = model::Timestamp::now();
    const auto kvnr = generateNewRandomKVNR().id();

    // Create consent
    std::optional<model::Consent> consent;
    ASSERT_NO_FATAL_FAILURE(consent = consentPost(kvnr, startTime));
    ASSERT_TRUE(consent.has_value());

    // Create closed task
    std::optional<model::PrescriptionId> prescriptionId;
    std::optional<model::KbvBundle> kbvBundle;
    std::optional<model::ErxReceipt> closeReceipt;
    std::string accessCode;
    std::string secret;
    ASSERT_NO_FATAL_FAILURE(
            createClosedTask(prescriptionId, kbvBundle, closeReceipt, accessCode, secret, GetParam(), kvnr));
    // Create a charge item for task
    const auto telematikIdPharmacy = jwtApotheke().stringForClaim(JWT::idNumberClaim).value();
    std::optional<model::ChargeItem> createdChargeItem;
    ASSERT_NO_FATAL_FAILURE(createdChargeItem = chargeItemPost(*prescriptionId, kvnr, telematikIdPharmacy, secret));

    // Try to create another charge item for same task -> Error 409
    ASSERT_NO_FATAL_FAILURE(
        createdChargeItem = chargeItemPost(*prescriptionId, kvnr, telematikIdPharmacy, secret,
                                           HttpStatus::Conflict, model::OperationOutcome::Issue::Type::conflict));
}

// GEMREQ-start InstantiateTest
INSTANTIATE_TEST_SUITE_P(ErpWorkflowPkvTestPInst, ErpWorkflowPkvTestP,
                         testing::Values(model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                                         model::PrescriptionType::direkteZuweisungPkv));
// GEMREQ-end InstantiateTest
