/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/ErpRequirements.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/Kvnr.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "test_config.h"
#include "test/erp/database/PostgresDatabaseTest.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/ResourceManager.hxx"
#include "test/util/ResourceTemplates.hxx"

using namespace ::std::literals;

class PostgresBackendChargeItemTest : public PostgresDatabaseTest
{
public:
    void SetUp() override
    {
        if (model::ResourceVersion::deprecatedProfile(
                model::ResourceVersion::current<model::ResourceVersion::DeGematikErezeptWorkflowR4>()))
        {
            GTEST_SKIP();
        }
        ASSERT_NO_FATAL_FAILURE(PostgresDatabaseTest::SetUp());
    }

    void cleanup() override
    {
        if (usePostgres())
        {
            clearTables();
        }
    }

    ::std::vector<::model::ChargeInformation> setupChargeItems(::std::size_t count, const std::string_view& kvnr = InsurantA)
    {
        ::std::vector<::model::ChargeInformation> result;

        for (::std::size_t index = 0u; index < count; ++index)
        {
            auto& resourceManager = ::ResourceManager::instance();

            ::model::Task task(::model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                               ::MockDatabase::mockAccessCode);
            const auto id = database().storeTask(task);
            task.setPrescriptionId(id);
            const auto type = id.isPkv() ? model::Kvnr::Type::pkv : model::Kvnr::Type::gkv;
            task.setKvnr(model::Kvnr{kvnr, type});
            task.setExpiryDate(::model::Timestamp::now());
            task.setAcceptDate(::model::Timestamp::now());
            task.setStatus(::model::Task::Status::ready);
            database().updateTaskStatusAndSecret(task);

            auto prescriptionXML = ResourceTemplates::kbvBundlePkvXml({id, model::Kvnr(kvnr)});
            auto prescription = ::model::Bundle::fromXmlNoValidation(prescriptionXML);
            auto signedPrescription =
                ::model::Binary{prescription.getId().toString(),
                                ::CryptoHelper::toCadesBesSignature(prescription.serializeToJsonString())};

            database().activateTask(task, signedPrescription);

            auto chargeItem = model::ChargeItem::fromXmlNoValidation(
                resourceManager.getStringResource("test/EndpointHandlerTest/charge_item_input.xml"));
            chargeItem.setId(id);
            chargeItem.setPrescriptionId(id);
            chargeItem.setSubjectKvnr(kvnr);
            chargeItem.setEntererTelematikId("606358757");
            chargeItem.setEnteredDate(::model::Timestamp::now());
            chargeItem.setAccessCode(::MockDatabase::mockAccessCode);
            chargeItem.deleteContainedBinary();

            const auto& dispenseItemXML =
                resourceManager.getStringResource("test/EndpointHandlerTest/dispense_item.xml");
            auto medicationDispense = ::model::MedicationDispense::fromXmlNoValidation(dispenseItemXML);
            medicationDispense.setTelematicId(model::TelematikId("606358757"));
            medicationDispense.setWhenHandedOver(::model::Timestamp::now());
            auto dispenseItem = ::model::AbgabedatenPkvBundle::fromJsonNoValidation(medicationDispense.serializeToJsonString());
            dispenseItem.setId(::Uuid{"fe4a04af-0828-4977-a5ce-bfeed16ebf10"});
            auto signedDispenseItem =
                ::model::Binary{"fe4a04af-0828-4977-a5ce-bfeed16ebf10",
                                ::CryptoHelper::toCadesBesSignature(dispenseItem.serializeToJsonString())};

            chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItemBundle);
            chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItemBundle);

            auto receiptJson = resourceManager.getStringResource("test/EndpointHandlerTest/receipt_template.json");
            receiptJson = ::String::replaceAll(receiptJson, "##PRESCRIPTION_ID##", id.toString());
            auto receipt = ::model::Bundle::fromJsonNoValidation(receiptJson);
            chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::receiptBundle);

            ::std::vector<::model::MedicationDispense> medicationDispenses;
            medicationDispenses.emplace_back(
                ::model::MedicationDispense::fromJsonNoValidation(dispenseItem.serializeToJsonString()));

            database().updateTaskMedicationDispenseReceipt(task, medicationDispenses,
                                                           ::model::ErxReceipt::fromJsonNoValidation(receiptJson));

            result.emplace_back(::model::ChargeInformation{::std::move(chargeItem), ::std::move(signedPrescription),
                                                           ::std::move(prescription), ::std::move(signedDispenseItem),
                                                           ::std::move(dispenseItem), ::std::move(receipt)});

            database().storeChargeInformation(result.back());
        }
        database().commitTransaction();

        return result;
    }
};

TEST_F(PostgresBackendChargeItemTest, storeChargeInformation)//NOLINT(readability-function-cognitive-complexity)
{
    if (! usePostgres())
    {
        GTEST_SKIP();
    }

    ::std::vector<::model::ChargeInformation> chargeInformation = setupChargeItems(1u);
    ASSERT_TRUE(chargeInformation.size() == 1u);
    ASSERT_TRUE(chargeInformation[0].chargeItem.prescriptionId());

    const auto chargeInformationFromDatabase =
        database().retrieveChargeInformation(chargeInformation[0].chargeItem.prescriptionId().value());
    database().commitTransaction();

    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.id());
    EXPECT_EQ(chargeInformation[0].chargeItem.id()->toString(),
              chargeInformationFromDatabase.chargeItem.id()->toString());
    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.prescriptionId());
    EXPECT_EQ(chargeInformation[0].chargeItem.prescriptionId()->toString(),
              chargeInformationFromDatabase.chargeItem.prescriptionId()->toString());

    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.subjectKvnr());
    EXPECT_EQ(chargeInformation[0].chargeItem.subjectKvnr().value(),
              chargeInformationFromDatabase.chargeItem.subjectKvnr().value());
    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.entererTelematikId());
    EXPECT_EQ(chargeInformation[0].chargeItem.entererTelematikId().value(),
              chargeInformationFromDatabase.chargeItem.entererTelematikId().value());
    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.enteredDate());
    EXPECT_EQ(chargeInformation[0].chargeItem.enteredDate().value(),
              chargeInformationFromDatabase.chargeItem.enteredDate().value());

    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.supportingInfoReference(
        ::model::ChargeItem::SupportingInfoType::dispenseItemBundle));
    EXPECT_EQ(chargeInformation[0]
                  .chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItemBundle)
                  .value(),
              chargeInformationFromDatabase.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItemBundle)
                  .value());
    EXPECT_TRUE(chargeInformationFromDatabase.chargeItem.supportingInfoReference(
        ::model::ChargeItem::SupportingInfoType::prescriptionItemBundle));
    EXPECT_EQ(chargeInformation[0]
                  .chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItemBundle)
                  .value(),
              chargeInformationFromDatabase.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItemBundle)
                  .value());
    EXPECT_TRUE(chargeInformationFromDatabase.chargeItem.supportingInfoReference(
        ::model::ChargeItem::SupportingInfoType::receiptBundle));
    EXPECT_EQ(chargeInformation[0]
                  .chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::receiptBundle)
                  .value(),
              chargeInformationFromDatabase.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::receiptBundle)
                  .value());

    EXPECT_EQ(chargeInformation[0].chargeItem.isMarked(), chargeInformationFromDatabase.chargeItem.isMarked());
    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.markingFlags());
    EXPECT_EQ(chargeInformation[0].chargeItem.markingFlags()->serializeToJsonString(),
              chargeInformationFromDatabase.chargeItem.markingFlags()->serializeToJsonString());
    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.accessCode());
    EXPECT_EQ(chargeInformation[0].chargeItem.accessCode().value(),
              chargeInformationFromDatabase.chargeItem.accessCode().value());

    EXPECT_FALSE(chargeInformationFromDatabase.chargeItem.containedBinary());

    ASSERT_TRUE(chargeInformationFromDatabase.prescription);
    EXPECT_EQ(chargeInformation[0].prescription->serializeToJsonString(),
              chargeInformationFromDatabase.prescription->serializeToJsonString());
    ASSERT_TRUE(chargeInformationFromDatabase.unsignedPrescription);
    EXPECT_EQ(chargeInformation[0].unsignedPrescription->serializeToJsonString(),
              chargeInformationFromDatabase.unsignedPrescription->serializeToJsonString());
    EXPECT_EQ(chargeInformation[0].dispenseItem->serializeToJsonString(),
              chargeInformationFromDatabase.dispenseItem->serializeToJsonString());
    EXPECT_EQ(chargeInformation[0].unsignedDispenseItem->serializeToJsonString(),
              chargeInformationFromDatabase.unsignedDispenseItem->serializeToJsonString());

    ASSERT_TRUE(chargeInformationFromDatabase.receipt);
    EXPECT_EQ(chargeInformation[0].receipt->serializeToJsonString(),
              chargeInformationFromDatabase.receipt->serializeToJsonString());
}

TEST_F(PostgresBackendChargeItemTest, UpdateChargeInformation)//NOLINT(readability-function-cognitive-complexity)
{
    if (! usePostgres())
    {
        GTEST_SKIP();
    }

    ::std::vector<::model::ChargeInformation> chargeInformation = setupChargeItems(1u);
    ASSERT_TRUE(chargeInformation.size() == 1u);
    ASSERT_TRUE(chargeInformation[0].chargeItem.prescriptionId());

    auto [chargeInformationForUpdate, blobId, salt] =
        database().retrieveChargeInformationForUpdate(chargeInformation[0].chargeItem.prescriptionId().value());
    database().commitTransaction();

    const auto markingFlag = ::String::replaceAll(
        chargeInformationForUpdate.chargeItem.markingFlags()->serializeToJsonString(), "false", "true");
    chargeInformationForUpdate.chargeItem.setMarkingFlags(::model::Extension::fromJsonNoValidation(markingFlag));

    const auto dispenseItemXML = ::String::replaceAll(
        ::ResourceManager::instance().getStringResource("test/EndpointHandlerTest/dispense_item.xml"), "4.50", "5.00");
    auto dispenseItem = model::AbgabedatenPkvBundle::fromXmlNoValidation(dispenseItemXML);
    dispenseItem.setId(Uuid{"fe4a04af-0828-4977-a5ce-bfeed16ebf10"});
    chargeInformationForUpdate.dispenseItem =
        ::model::Binary{dispenseItem.getId().toString(),
                        ::CryptoHelper::toCadesBesSignature(dispenseItem.serializeToJsonString())};
    chargeInformationForUpdate.unsignedDispenseItem = ::std::move(dispenseItem);

    database().updateChargeInformation(chargeInformationForUpdate, blobId, salt);
    database().commitTransaction();

    const auto chargeInformationFromDatabase =
        database().retrieveChargeInformation(chargeInformation[0].chargeItem.prescriptionId().value());
    database().commitTransaction();

    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.id());
    EXPECT_EQ(chargeInformationForUpdate.chargeItem.id()->toString(),
              chargeInformationFromDatabase.chargeItem.id()->toString());
    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.prescriptionId());
    EXPECT_EQ(chargeInformationForUpdate.chargeItem.prescriptionId()->toString(),
              chargeInformationFromDatabase.chargeItem.prescriptionId()->toString());

    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.subjectKvnr());
    EXPECT_EQ(chargeInformationForUpdate.chargeItem.subjectKvnr().value(),
              chargeInformationFromDatabase.chargeItem.subjectKvnr().value());
    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.entererTelematikId());
    EXPECT_EQ(chargeInformationForUpdate.chargeItem.entererTelematikId().value(),
              chargeInformationFromDatabase.chargeItem.entererTelematikId().value());
    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.enteredDate());
    EXPECT_EQ(chargeInformationForUpdate.chargeItem.enteredDate().value(),
              chargeInformationFromDatabase.chargeItem.enteredDate().value());

    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.supportingInfoReference(
        ::model::ChargeItem::SupportingInfoType::dispenseItemBundle));
    EXPECT_EQ(chargeInformationForUpdate.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItemBundle)
                  .value(),
              chargeInformationFromDatabase.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItemBundle)
                  .value());
    EXPECT_TRUE(chargeInformationFromDatabase.chargeItem.supportingInfoReference(
        ::model::ChargeItem::SupportingInfoType::prescriptionItemBundle));
    EXPECT_EQ(chargeInformationForUpdate.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItemBundle)
                  .value(),
              chargeInformationFromDatabase.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItemBundle)
                  .value());
    EXPECT_TRUE(chargeInformationFromDatabase.chargeItem.supportingInfoReference(
        ::model::ChargeItem::SupportingInfoType::receiptBundle));
    EXPECT_EQ(
        chargeInformationForUpdate.chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::receiptBundle)
            .value(),
        chargeInformationFromDatabase.chargeItem
            .supportingInfoReference(::model::ChargeItem::SupportingInfoType::receiptBundle)
            .value());

    EXPECT_NE(chargeInformation[0].chargeItem.isMarked(), chargeInformationFromDatabase.chargeItem.isMarked());
    EXPECT_EQ(chargeInformationForUpdate.chargeItem.isMarked(), chargeInformationFromDatabase.chargeItem.isMarked());
    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.markingFlags());
    EXPECT_NE(chargeInformation[0].chargeItem.markingFlags()->serializeToJsonString(),
              chargeInformationFromDatabase.chargeItem.markingFlags()->serializeToJsonString());
    EXPECT_EQ(chargeInformationForUpdate.chargeItem.markingFlags()->serializeToJsonString(),
              chargeInformationFromDatabase.chargeItem.markingFlags()->serializeToJsonString());

    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.accessCode());
    EXPECT_EQ(chargeInformationForUpdate.chargeItem.accessCode().value(),
              chargeInformationFromDatabase.chargeItem.accessCode().value());

    EXPECT_FALSE(chargeInformationFromDatabase.chargeItem.containedBinary());

    ASSERT_TRUE(chargeInformationFromDatabase.prescription);
    EXPECT_EQ(chargeInformationForUpdate.prescription->serializeToJsonString(),
              chargeInformationFromDatabase.prescription->serializeToJsonString());
    ASSERT_TRUE(chargeInformationFromDatabase.unsignedPrescription);
    EXPECT_EQ(chargeInformationForUpdate.unsignedPrescription->serializeToJsonString(),
              chargeInformationFromDatabase.unsignedPrescription->serializeToJsonString());
    EXPECT_NE(chargeInformation[0].dispenseItem->serializeToJsonString(),
              chargeInformationFromDatabase.dispenseItem->serializeToJsonString());
    EXPECT_EQ(chargeInformationForUpdate.dispenseItem->serializeToJsonString(),
              chargeInformationFromDatabase.dispenseItem->serializeToJsonString());
    EXPECT_NE(chargeInformation[0].unsignedDispenseItem->serializeToJsonString(),
              chargeInformationFromDatabase.unsignedDispenseItem->serializeToJsonString());
    EXPECT_EQ(chargeInformationForUpdate.unsignedDispenseItem->serializeToJsonString(),
              chargeInformationFromDatabase.unsignedDispenseItem->serializeToJsonString());

    ASSERT_TRUE(chargeInformationFromDatabase.receipt);
    EXPECT_EQ(chargeInformationForUpdate.receipt->serializeToJsonString(),
              chargeInformationFromDatabase.receipt->serializeToJsonString());
}

// GEMREQ-start A_22117-01
TEST_F(PostgresBackendChargeItemTest, DeleteChargeInformation)
{
    if (! usePostgres())
    {
        GTEST_SKIP();
    }

    ::std::vector<::model::ChargeInformation> chargeInformation1 = setupChargeItems(1u);
    ASSERT_TRUE(chargeInformation1.size() == 1u);
    const auto prescriptionId1 = chargeInformation1[0].chargeItem.prescriptionId();
    ASSERT_TRUE(prescriptionId1);

    ::std::vector<::model::ChargeInformation> chargeInformation2 = setupChargeItems(1u);
    ASSERT_TRUE(chargeInformation1.size() == 1u);
    const auto prescriptionId2 = chargeInformation2[0].chargeItem.prescriptionId();
    ASSERT_TRUE(prescriptionId2);

    EXPECT_NO_THROW(database().deleteChargeInformation(*prescriptionId1));
    database().commitTransaction();
    EXPECT_ERP_EXCEPTION((void) database().retrieveChargeInformation(*prescriptionId1), HttpStatus::NotFound);
    EXPECT_NO_THROW((void)database().retrieveChargeInformation(*prescriptionId2));
    database().commitTransaction();
}
// GEMREQ-end A_22117-01

// GEMREQ-start A_22157
TEST_F(PostgresBackendChargeItemTest, ClearAllChargeInformation)
{
    if (! usePostgres())
    {
        GTEST_SKIP();
    }

    const auto chargeInformation1 = setupChargeItems(3u, InsurantA);
    ASSERT_TRUE(chargeInformation1.size() == 3u);
    ASSERT_TRUE(chargeInformation1[0].chargeItem.prescriptionId());
    ASSERT_TRUE(chargeInformation1[1].chargeItem.prescriptionId());
    ASSERT_TRUE(chargeInformation1[2].chargeItem.prescriptionId());
    ASSERT_TRUE(chargeInformation1[0].chargeItem.subjectKvnr());
    const auto kvnr1 = chargeInformation1[0].chargeItem.subjectKvnr().value();
    EXPECT_EQ(kvnr1, InsurantA);
    EXPECT_EQ(database().countChargeInformationForInsurant(kvnr1, {}), 3u);

    const auto chargeInformation2 = setupChargeItems(1u, InsurantB);
    ASSERT_TRUE(chargeInformation2.size() == 1u);
    ASSERT_TRUE(chargeInformation2[0].chargeItem.prescriptionId());
    const auto kvnr2 = chargeInformation2[0].chargeItem.subjectKvnr().value();
    EXPECT_EQ(kvnr2, InsurantB);
    EXPECT_EQ(database().countChargeInformationForInsurant(kvnr2, {}), 1u);

    // Deletion of entries from table charge_item for kvnr1
    EXPECT_NO_THROW(database().clearAllChargeInformation(kvnr1));
    database().commitTransaction();

    EXPECT_EQ(database().countChargeInformationForInsurant(kvnr1, {}), 0u);
    EXPECT_EQ(database().countChargeInformationForInsurant(kvnr2, {}), 1u);

    EXPECT_ERP_EXCEPTION(
        (void) database().retrieveChargeInformation(chargeInformation1[0].chargeItem.prescriptionId().value()), HttpStatus::NotFound);
    EXPECT_ERP_EXCEPTION(
        (void) database().retrieveChargeInformation(chargeInformation1[1].chargeItem.prescriptionId().value()), HttpStatus::NotFound);
    EXPECT_ERP_EXCEPTION(
        (void) database().retrieveChargeInformation(chargeInformation1[2].chargeItem.prescriptionId().value()), HttpStatus::NotFound);
    EXPECT_NO_THROW(
        (void) database().retrieveChargeInformation(chargeInformation2[0].chargeItem.prescriptionId().value()));
    database().commitTransaction();
}
// GEMREQ-end A_22157

TEST_F(PostgresBackendChargeItemTest, AllChargeItemsForInsurant)
{
    if (! usePostgres())
    {
        GTEST_SKIP();
    }

    ::std::vector<::model::ChargeInformation> chargeInformation = setupChargeItems(5u);
    ASSERT_TRUE(chargeInformation.size() == 5u);

    ASSERT_TRUE(chargeInformation[0].chargeItem.subjectKvnr());
    const auto kvnr = chargeInformation[0].chargeItem.subjectKvnr().value();
    EXPECT_EQ(database().countChargeInformationForInsurant(kvnr, {}), 5u);
    database().commitTransaction();

    const auto allChargeItems = database().retrieveAllChargeItemsForInsurant(kvnr, {});
    EXPECT_EQ(allChargeItems.size(), 5u);
    database().commitTransaction();
}

TEST_F(PostgresBackendChargeItemTest, AllChargeItemsForInsurant_AbsentAccessCodePerItem_NoException)
{
    if (! usePostgres())
    {
        GTEST_SKIP();
    }

    const size_t numberOfItems = 5u;

    ::std::vector<::model::ChargeInformation> chargeInformation = setupChargeItems(numberOfItems);
    ASSERT_TRUE(chargeInformation.size() == numberOfItems);

    ASSERT_TRUE(chargeInformation[0].chargeItem.subjectKvnr());
    const auto kvnr = chargeInformation[0].chargeItem.subjectKvnr().value();
    EXPECT_EQ(database().countChargeInformationForInsurant(kvnr, {}), numberOfItems);
    database().commitTransaction();

    // Query for all charge items must not populate the access code field.
    const auto allChargeItems = database().retrieveAllChargeItemsForInsurant(kvnr, {});
    EXPECT_EQ(allChargeItems.size(), numberOfItems);

    // Test for empty access code in charge items.
    for (const auto& chargeItem : allChargeItems) {
        EXPECT_FALSE(chargeItem.accessCode().has_value());
    }

    database().commitTransaction();
}

TEST_F(PostgresBackendChargeItemTest, ChargeItemForInsurant_PresentAccessCode_NoException)
{
    if (! usePostgres())
    {
        GTEST_SKIP();
    }

    const size_t numberOfItems = 2u;

    ::std::vector<::model::ChargeInformation> chargeInformation = setupChargeItems(numberOfItems);
    ASSERT_TRUE(chargeInformation.size() == numberOfItems);

    ASSERT_TRUE(chargeInformation[0].chargeItem.subjectKvnr());
    const auto kvnr = chargeInformation[0].chargeItem.subjectKvnr().value();
    EXPECT_EQ(database().countChargeInformationForInsurant(kvnr, {}), numberOfItems);
    database().commitTransaction();

    for (const auto & info : chargeInformation) {
        // Test for present access code field when querying single charge items.
        const auto item = database().retrieveChargeInformation(info.chargeItem.prescriptionId().value());
        EXPECT_TRUE(item.chargeItem.accessCode().has_value());
    }

    database().commitTransaction();
}
