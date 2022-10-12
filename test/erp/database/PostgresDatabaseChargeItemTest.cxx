/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#include "erp/ErpRequirements.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/ChargeItem.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/PrescriptionId.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Uuid.hxx"
#include "erp/util/search/UrlArguments.hxx"
#include "test/erp/database/PostgresDatabaseTest.hxx"
#include "test/mock/MockDatabase.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/ErpMacros.hxx"
#include "test/util/ResourceManager.hxx"

#include "test_config.h"

using namespace ::std::literals;

class PostgresBackendChargeItemTest : public PostgresDatabaseTest
{
public:
    void cleanup() override
    {
        if (usePostgres())
        {
            clearTables();
        }
    }

    ::std::vector<::model::ChargeInformation> setupChargeItems(::std::size_t count)
    {
        ::std::vector<::model::ChargeInformation> result;

        for (::std::size_t index = 0u; index < count; ++index)
        {
            auto& resourceManager = ::ResourceManager::instance();

            const auto insurant = "X000000100"s;

            ::model::Task task(::model::PrescriptionType::apothekenpflichtigeArzneimittelPkv,
                               ::MockDatabase::mockAccessCode);
            const auto id = database().storeTask(task);
            task.setPrescriptionId(id);
            task.setKvnr(insurant);
            task.setExpiryDate(::model::Timestamp::now());
            task.setAcceptDate(::model::Timestamp::now());
            task.setStatus(::model::Task::Status::ready);
            database().updateTaskStatusAndSecret(task);

            auto prescriptionXML =
                resourceManager.getStringResource("test/EndpointHandlerTest/kbv_pkv_bundle_template.xml");
            prescriptionXML = String::replaceAll(prescriptionXML, "##PRESCRIPTION_ID##", id.toString());
            prescriptionXML = String::replaceAll(prescriptionXML, "##KVNR##", ::std::string{InsurantA});
            auto prescription = ::model::Bundle::fromXmlNoValidation(prescriptionXML);
            auto signedPrescription =
                ::model::Binary{prescription.getId().toString(),
                                ::CryptoHelper::toCadesBesSignature(prescription.serializeToJsonString())};

            database().activateTask(task, signedPrescription);

            auto chargeItem = model::ChargeItem::fromXmlNoValidation(
                resourceManager.getStringResource("test/EndpointHandlerTest/charge_item_input.xml"));
            chargeItem.setId(id);
            chargeItem.setPrescriptionId(id);
            chargeItem.setSubjectKvnr(insurant);
            chargeItem.setEntererTelematikId("606358757");
            chargeItem.setEnteredDate(::model::Timestamp::now());
            chargeItem.setAccessCode(::MockDatabase::mockAccessCode);
            chargeItem.deleteContainedBinary();

            const auto& dispenseItemXML =
                resourceManager.getStringResource("test/EndpointHandlerTest/dispense_item.xml");
            auto medicationDispense = ::model::MedicationDispense::fromXmlNoValidation(dispenseItemXML);
            medicationDispense.setTelematicId("606358757");
            medicationDispense.setWhenHandedOver(::model::Timestamp::now());
            auto dispenseItem = ::model::Bundle::fromJsonNoValidation(medicationDispense.serializeToJsonString());
            dispenseItem.setId(::Uuid{"fe4a04af-0828-4977-a5ce-bfeed16ebf10"});
            auto signedDispenseItem =
                ::model::Binary{"fe4a04af-0828-4977-a5ce-bfeed16ebf10",
                                ::CryptoHelper::toCadesBesSignature(dispenseItem.serializeToJsonString())};

            chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem, dispenseItem);
            chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItem,
                                                  prescription);

            auto receiptJson = resourceManager.getStringResource("test/EndpointHandlerTest/receipt_template.json");
            receiptJson = ::String::replaceAll(receiptJson, "##PRESCRIPTION_ID##", id.toString());
            auto receipt = ::model::Bundle::fromJsonNoValidation(receiptJson);
            chargeItem.setSupportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt, receipt);

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
        ::model::ChargeItem::SupportingInfoType::dispenseItem));
    EXPECT_EQ(chargeInformation[0]
                  .chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem)
                  .value(),
              chargeInformationFromDatabase.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem)
                  .value());
    EXPECT_TRUE(chargeInformationFromDatabase.chargeItem.supportingInfoReference(
        ::model::ChargeItem::SupportingInfoType::prescriptionItem));
    EXPECT_EQ(chargeInformation[0]
                  .chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItem)
                  .value(),
              chargeInformationFromDatabase.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItem)
                  .value());
    EXPECT_TRUE(chargeInformationFromDatabase.chargeItem.supportingInfoReference(
        ::model::ChargeItem::SupportingInfoType::receipt));
    EXPECT_EQ(chargeInformation[0]
                  .chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt)
                  .value(),
              chargeInformationFromDatabase.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt)
                  .value());

    EXPECT_EQ(chargeInformation[0].chargeItem.isMarked(), chargeInformationFromDatabase.chargeItem.isMarked());
    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.markingFlag());
    EXPECT_EQ(chargeInformation[0].chargeItem.markingFlag()->serializeToJsonString(),
              chargeInformationFromDatabase.chargeItem.markingFlag()->serializeToJsonString());
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
    EXPECT_EQ(chargeInformation[0].dispenseItem.serializeToJsonString(),
              chargeInformationFromDatabase.dispenseItem.serializeToJsonString());
    EXPECT_EQ(chargeInformation[0].unsignedDispenseItem.serializeToJsonString(),
              chargeInformationFromDatabase.unsignedDispenseItem.serializeToJsonString());

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
        chargeInformationForUpdate.chargeItem.markingFlag()->serializeToJsonString(), "false", "true");
    chargeInformationForUpdate.chargeItem.setMarkingFlag(::model::Extension::fromJsonNoValidation(markingFlag));

    const auto dispenseItemXML = ::String::replaceAll(
        ::ResourceManager::instance().getStringResource("test/EndpointHandlerTest/dispense_item.xml"), "4.50", "5.00");
    auto dispenseItem = model::Bundle::fromXmlNoValidation(dispenseItemXML);
    chargeInformationForUpdate.dispenseItem =
        ::model::Binary{dispenseItem.getIdentifier().toString(),
                        ::CryptoHelper::toCadesBesSignature(dispenseItem.serializeToJsonString())};
    chargeInformationForUpdate.dispenseItem =
        ::model::Binary{"fe4a04af-0828-4977-a5ce-bfeed16ebf10",
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
        ::model::ChargeItem::SupportingInfoType::dispenseItem));
    EXPECT_EQ(chargeInformationForUpdate.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem)
                  .value(),
              chargeInformationFromDatabase.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::dispenseItem)
                  .value());
    EXPECT_TRUE(chargeInformationFromDatabase.chargeItem.supportingInfoReference(
        ::model::ChargeItem::SupportingInfoType::prescriptionItem));
    EXPECT_EQ(chargeInformationForUpdate.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItem)
                  .value(),
              chargeInformationFromDatabase.chargeItem
                  .supportingInfoReference(::model::ChargeItem::SupportingInfoType::prescriptionItem)
                  .value());
    EXPECT_TRUE(chargeInformationFromDatabase.chargeItem.supportingInfoReference(
        ::model::ChargeItem::SupportingInfoType::receipt));
    EXPECT_EQ(
        chargeInformationForUpdate.chargeItem.supportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt)
            .value(),
        chargeInformationFromDatabase.chargeItem
            .supportingInfoReference(::model::ChargeItem::SupportingInfoType::receipt)
            .value());

    EXPECT_NE(chargeInformation[0].chargeItem.isMarked(), chargeInformationFromDatabase.chargeItem.isMarked());
    EXPECT_EQ(chargeInformationForUpdate.chargeItem.isMarked(), chargeInformationFromDatabase.chargeItem.isMarked());
    ASSERT_TRUE(chargeInformationFromDatabase.chargeItem.markingFlag());
    EXPECT_NE(chargeInformation[0].chargeItem.markingFlag()->serializeToJsonString(),
              chargeInformationFromDatabase.chargeItem.markingFlag()->serializeToJsonString());
    EXPECT_EQ(chargeInformationForUpdate.chargeItem.markingFlag()->serializeToJsonString(),
              chargeInformationFromDatabase.chargeItem.markingFlag()->serializeToJsonString());

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
    EXPECT_NE(chargeInformation[0].dispenseItem.serializeToJsonString(),
              chargeInformationFromDatabase.dispenseItem.serializeToJsonString());
    EXPECT_EQ(chargeInformationForUpdate.dispenseItem.serializeToJsonString(),
              chargeInformationFromDatabase.dispenseItem.serializeToJsonString());
    EXPECT_NE(chargeInformation[0].unsignedDispenseItem.serializeToJsonString(),
              chargeInformationFromDatabase.unsignedDispenseItem.serializeToJsonString());
    EXPECT_EQ(chargeInformationForUpdate.unsignedDispenseItem.serializeToJsonString(),
              chargeInformationFromDatabase.unsignedDispenseItem.serializeToJsonString());

    ASSERT_TRUE(chargeInformationFromDatabase.receipt);
    EXPECT_EQ(chargeInformationForUpdate.receipt->serializeToJsonString(),
              chargeInformationFromDatabase.receipt->serializeToJsonString());
}

TEST_F(PostgresBackendChargeItemTest, DeleteChargeInformation)
{
    if (! usePostgres())
    {
        GTEST_SKIP();
    }

    ::std::vector<::model::ChargeInformation> chargeInformation = setupChargeItems(1u);
    ASSERT_TRUE(chargeInformation.size() == 1u);
    const auto prescriptionId = chargeInformation[0].chargeItem.prescriptionId();
    ASSERT_TRUE(prescriptionId);

    {
        auto checkTransaction = createTransaction();
        const auto result = checkTransaction.exec_params("SELECT FROM erp.task_200 "
                                                         "WHERE prescription_id = $1 "
                                                         "AND healthcare_provider_prescription IS NOT NULL "
                                                         "AND medication_dispense_bundle IS NOT NULL "
                                                         "AND medication_dispense_blob_id IS NOT NULL "
                                                         "AND receipt IS NOT NULL",
                                                         prescriptionId->toDatabaseId());
        checkTransaction.commit();
        EXPECT_EQ(result.size(), 1u);
    }

    EXPECT_NO_THROW(database().deleteChargeInformation(*prescriptionId));
    database().commitTransaction();
    EXPECT_ANY_THROW((void) database().retrieveChargeInformation(*prescriptionId));
    database().commitTransaction();

    A_22117_01.test("E-Rezept-Fachdienst - Abrechnungsinformation löschen - zu löschende Ressourcen");
    {
        auto checkTransaction = createTransaction();
        const auto result = checkTransaction.exec_params("SELECT FROM erp.task_200 "
                                                         "WHERE prescription_id = $1 "
                                                         "AND healthcare_provider_prescription IS NULL "
                                                         "AND medication_dispense_bundle IS NULL "
                                                         "AND medication_dispense_blob_id IS NULL "
                                                         "AND receipt IS NULL",
                                                         prescriptionId->toDatabaseId());
        checkTransaction.commit();
        EXPECT_EQ(result.size(), 1u);
    }

    database().commitTransaction();
}

TEST_F(PostgresBackendChargeItemTest, ClearAllChargeInformation)
{
    if (! usePostgres())
    {
        GTEST_SKIP();
    }

    ::std::vector<::model::ChargeInformation> chargeInformation = setupChargeItems(3u);
    ASSERT_TRUE(chargeInformation.size() == 3u);
    ASSERT_TRUE(chargeInformation[0].chargeItem.prescriptionId());
    ASSERT_TRUE(chargeInformation[1].chargeItem.prescriptionId());
    ASSERT_TRUE(chargeInformation[2].chargeItem.prescriptionId());
    ASSERT_TRUE(chargeInformation[0].chargeItem.subjectKvnr());
    const auto kvnr = ::std::string{chargeInformation[0].chargeItem.subjectKvnr().value()};
    EXPECT_EQ(database().countChargeInformationForInsurant(kvnr, {}), 3u);
    database().commitTransaction();

    const auto hashedKvnr = getKeyDerivation().hashKvnr(kvnr);
    {
        auto checkTransaction = createTransaction();
        const auto result = checkTransaction.exec_params("SELECT FROM erp.task_200 "
                                                         "WHERE kvnr_hashed = $1 "
                                                         "AND healthcare_provider_prescription IS NOT NULL "
                                                         "AND medication_dispense_bundle IS NOT NULL "
                                                         "AND medication_dispense_blob_id IS NOT NULL "
                                                         "AND receipt IS NOT NULL",
                                                         hashedKvnr.binarystring());
        checkTransaction.commit();
        EXPECT_EQ(result.size(), 3u);
    }

    EXPECT_NO_THROW(database().clearAllChargeInformation(kvnr));
    database().commitTransaction();

    EXPECT_EQ(database().countChargeInformationForInsurant(kvnr, {}), 0u);
    database().commitTransaction();

    EXPECT_ANY_THROW(
        (void) database().retrieveChargeInformation(chargeInformation[0].chargeItem.prescriptionId().value()));
    EXPECT_ANY_THROW(
        (void) database().retrieveChargeInformation(chargeInformation[1].chargeItem.prescriptionId().value()));
    EXPECT_ANY_THROW(
        (void) database().retrieveChargeInformation(chargeInformation[2].chargeItem.prescriptionId().value()));
    database().commitTransaction();

    A_22117_01.test("E-Rezept-Fachdienst - Abrechnungsinformation löschen - zu löschende Ressourcen");
    {
        auto checkTransaction = createTransaction();
        const auto result = checkTransaction.exec_params("SELECT FROM erp.task_200 "
                                                         "WHERE kvnr_hashed = $1 "
                                                         "AND healthcare_provider_prescription IS NULL "
                                                         "AND medication_dispense_bundle IS NULL "
                                                         "AND medication_dispense_blob_id IS NULL "
                                                         "AND receipt IS NULL",
                                                         hashedKvnr.binarystring());
        checkTransaction.commit();
        EXPECT_EQ(result.size(), 3u);
    }

    database().commitTransaction();
}

TEST_F(PostgresBackendChargeItemTest, AllChargeItemsForInsurant)
{
    if (! usePostgres())
    {
        GTEST_SKIP();
    }

    ::std::vector<::model::ChargeInformation> chargeInformation = setupChargeItems(5u);
    ASSERT_TRUE(chargeInformation.size() == 5u);

    ASSERT_TRUE(chargeInformation[0].chargeItem.subjectKvnr());
    const auto kvnr = ::std::string{chargeInformation[0].chargeItem.subjectKvnr().value()};
    EXPECT_EQ(database().countChargeInformationForInsurant(kvnr, {}), 5u);
    database().commitTransaction();

    const auto allChargeItems = database().retrieveAllChargeItemsForInsurant(kvnr, {});
    EXPECT_EQ(allChargeItems.size(), 5u);
    database().commitTransaction();
}
