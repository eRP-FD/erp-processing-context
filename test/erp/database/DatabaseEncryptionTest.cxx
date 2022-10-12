/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/ErpRequirements.hxx"
#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/DatabaseModel.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "erp/hsm/HsmPool.hxx"
#include "erp/hsm/KeyDerivation.hxx"
#include "erp/model/AuditData.hxx"
#include "erp/model/Binary.hxx"
#include "erp/model/ErxReceipt.hxx"
#include "erp/model/MedicationDispense.hxx"
#include "erp/model/Task.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "test/erp/database/PostgresDatabaseTest.hxx"
#include "test/util/JsonTestUtils.hxx"

#include <erp/compression/ZStd.hxx>
#include <gtest/gtest.h>

#include "test/util/ResourceManager.hxx"

class DatabaseEncryptionTest : public PostgresDatabaseTest
{
protected:
    static constexpr std::string_view accessCode = "777bea0e13cc9c42ceec14aec3ddee2263325dc2c6c699db115f58fe423607ea";
    static constexpr std::string_view secret = "000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f";
    static constexpr std::string_view binId = "someId";
    static constexpr std::string_view binData = "SOME++DATA==";
    static constexpr std::string_view erxBundleResource = "test/fhir/conversion/erx_bundle.json";
    static constexpr std::string_view medicationDispenseResource = "test/fhir/conversion/medication_dispense.json";

    void cleanup() override
    {
        if (usePostgres())
        {
            clearTables();
        }
    }

    std::tuple<model::Task, model::ErxReceipt, model::MedicationDispense> createTask()
    {
        auto& db = database();
        model::Task task{model::PrescriptionType::apothekenpflichigeArzneimittel, accessCode};
        task.setAcceptDate(model::Timestamp{1.0});
        task.setExpiryDate(model::Timestamp{2.0});
        task.setKvnr(InsurantA);
        task.updateLastUpdate(model::Timestamp{3.0});
        task.setStatus(model::Task::Status::completed);
        task.setSecret(secret);
        model::PrescriptionId prescriptionId = db.storeTask(task);
        task.setPrescriptionId(prescriptionId);
        db.activateTask(task, model::Binary{binId, binData});
        auto erxReceipt =
            model::ErxReceipt::fromJsonNoValidation(ResourceManager::instance().getStringResource(erxBundleResource));
        std::vector<model::MedicationDispense> medicationDispenses;
        medicationDispenses.emplace_back(model::MedicationDispense::fromJsonNoValidation(
            ResourceManager::instance().getStringResource(medicationDispenseResource)));
        db.updateTaskMedicationDispenseReceipt(task, medicationDispenses, erxReceipt);
        db.updateTaskStatusAndSecret(task);
        db.commitTransaction();
        return std::make_tuple(std::move(task), std::move(erxReceipt), std::move(medicationDispenses.at(0)));
    }

    db_model::Blob accountSalt(const db_model::HashedId& identityHashed, db_model::MasterKeyType keyType, BlobId blobId)
    {
        auto txn = createTransaction();
        auto medicationDispenseAccountRow = txn.exec_params1(
            "SELECT salt FROM erp.account WHERE account_id = $1 AND master_key_type = $2 AND blob_id = $3",
            identityHashed.binarystring(), gsl::narrow<int>(keyType), blobId);
        txn.commit();
        return db_model::Blob{medicationDispenseAccountRow[0].as<db_model::postgres_bytea>()};
    }

    SafeString medicationDispenseKey(const db_model::HashedKvnr& kvnrHashed, BlobId blobId)
    {
        db_model::Blob salt{accountSalt(kvnrHashed, db_model::MasterKeyType::medicationDispense, blobId)};
        return getKeyDerivation().medicationDispenseKey(kvnrHashed, blobId, salt);
    }

    SafeString communicationKey(const std::string_view& identity, const db_model::HashedId& identityHashed,
                                BlobId blobId)
    {
        db_model::Blob salt{accountSalt(identityHashed, db_model::MasterKeyType::communication, blobId)};
        return getKeyDerivation().communicationKey(identity, identityHashed, blobId, salt);
    }

    SafeString auditeventKey(const db_model::HashedKvnr& kvnrHashed, BlobId blobId)
    {
        db_model::Blob salt{accountSalt(kvnrHashed, db_model::MasterKeyType::auditevent, blobId)};
        return getKeyDerivation().auditEventKey(kvnrHashed, blobId, salt);
    }
};


TEST_F(DatabaseEncryptionTest, TableTask)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    A_19688.test("no unencrypted personal data in table erp.task.");
    struct col {// no enum class to allow inplicit cast to int
        enum index
        {
            prescription_id,
            kvnr,
            kvnr_hashed,
            last_modified,
            authored_on,
            expiry_date,
            accept_date,
            status,
            task_key_blob_id,
            salt,
            access_code,
            secret,
            healthcare_provider_prescription,
            receipt,
            when_handed_over,
            when_prepared,
            performer,
            medication_dispense_blob_id,
            medication_dispense_bundle,
            COUNT
        };
    };
    const auto& [task, erxReceipt, medicationDispense] = createTask();
    auto prescriptionId = task.prescriptionId();
    auto txn = createTransaction();
    // intentionally uses '*' to get all columns of the table - so we will not forget to adapt this test if we add a row
    auto row = txn.exec_params1("SELECT * FROM erp.task WHERE prescription_id = $1", prescriptionId.toDatabaseId());
    ASSERT_EQ(row.size(), col::index::COUNT) << "Expected table `erp.task` to have 18 columns.";
    txn.commit();
    db_model::Blob salt{row[col::salt].as<db_model::postgres_bytea>()};
    BlobId blobId = gsl::narrow<BlobId>(row[col::task_key_blob_id].as<int32_t>());
    auto taskKey = getKeyDerivation().taskKey(prescriptionId, task.authoredOn(), blobId, salt);
    auto kvnrHashed = getKeyDerivation().hashKvnr(*task.kvnr());
    BlobId medicationDispenseBlobId = gsl::narrow<BlobId>(row[col::medication_dispense_blob_id].as<int32_t>());

    //  0: prescription_id
    //     not encrypted
    //  1: kvnr
    //     encrypted kvnr
    db_model::EncryptedBlob encryptedKvnr{row[col::kvnr].as<db_model::postgres_bytea>()};
    SafeString decryptedKvnr;
    ASSERT_NO_THROW(decryptedKvnr = getDBCodec().decode(encryptedKvnr, taskKey));
    EXPECT_EQ(decryptedKvnr, task.kvnr());
    //  2: kvnr_hashed bytea,
    EXPECT_EQ(kvnrHashed, db_model::EncryptedBlob{row[col::kvnr_hashed].as<db_model::postgres_bytea>()});
    //  3: last_modified timestamp with time zone DEFAULT CURRENT_TIMESTAMP::timestamp with time zone NOT NULL,
    //     not encrypted
    //  4: authored_on timestamp with time zone DEFAULT CURRENT_TIMESTAMP::timestamp with time zone NOT NULL,
    //     not encrypted
    //  5: expiry_date date,
    //     not encrypted
    //  6: accept_date date,
    //     not encrypted
    //  7: status smallint NOT NULL,
    //     not encrypted
    //  8: task_key_blob_id integer,
    //     not encrypted
    //  9: salt bytea,
    //     not encrypted
    // 10: access_code bytea,
    db_model::EncryptedBlob encryptedAccessCode{row[col::access_code].as<db_model::postgres_bytea>()};
    SafeString decryptedAccessCode;
    ASSERT_NO_THROW(decryptedAccessCode = getDBCodec().decode(encryptedAccessCode, taskKey));
    EXPECT_EQ(decryptedAccessCode, accessCode);
    // 11: secret bytea,
    db_model::EncryptedBlob encryptedSecret{row[col::secret].as<db_model::postgres_bytea>()};
    SafeString decryptedSecret;
    ASSERT_NO_THROW(decryptedSecret = getDBCodec().decode(encryptedSecret, taskKey));
    EXPECT_EQ(decryptedSecret, secret);
    // 12: healthcare_provider_prescription bytea,
    db_model::EncryptedBlob encryptedPrescription{
        row[col::healthcare_provider_prescription].as<db_model::postgres_bytea>()};
    std::optional<model::Binary> decryptedPrescription;
    ASSERT_NO_THROW(decryptedPrescription =
                        model::Binary::fromJsonNoValidation(getDBCodec().decode(encryptedPrescription, taskKey)));
    EXPECT_EQ(decryptedPrescription->id(), binId);
    EXPECT_EQ(decryptedPrescription->data(), binData);
    // 13: receipt bytea,
    db_model::EncryptedBlob encryptedReceipt{row[col::receipt].as<db_model::postgres_bytea>()};
    std::optional<model::ErxReceipt> decryptedReceipt;
    ASSERT_NO_THROW(decryptedReceipt =
                        model::ErxReceipt::fromJsonNoValidation(getDBCodec().decode(encryptedReceipt, taskKey)));
    EXPECT_EQ(decryptedReceipt->prescriptionId(), erxReceipt.prescriptionId());
    // 14: when_handed_over timestamp with time zone,
    //     not encrypted
    // 15: when_prepared timestamp with time zone,
    //     not encrypted
    // 16: performer bytea,
    auto telematikId = medicationDispense.telematikId();
    EXPECT_EQ(getKeyDerivation().hashTelematikId(telematikId),
              db_model::EncryptedBlob{row[col::performer].as<db_model::postgres_bytea>()});
    // 17: medication_dispense_blob_id integer,
    //     not encrypted
    // 18: medication_dispense_bundle bytea
    db_model::EncryptedBlob encryptedMedicationDispense{
        row[col::medication_dispense_bundle].as<db_model::postgres_bytea>()};
    std::optional<model::Bundle> decryptedMedicationDispenseBundle;
    {
        auto key = medicationDispenseKey(kvnrHashed, medicationDispenseBlobId);
        ASSERT_NO_THROW(decryptedMedicationDispenseBundle = model::Bundle::fromJsonNoValidation(
                            getDBCodec().decode(encryptedMedicationDispense, key)));
        auto decryptedMedicationDispenses =
            decryptedMedicationDispenseBundle->getResourcesByType<model::MedicationDispense>();
        ASSERT_FALSE(decryptedMedicationDispenses.empty());
        EXPECT_EQ(decryptedMedicationDispenses[0].kvnr(), medicationDispense.kvnr());
    }
    A_19688.finish();
}

TEST_F(DatabaseEncryptionTest, TableCommunication)//NOLINT(readability-function-cognitive-complexity)
{
    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    A_19688.test("no unencrypted personal data in table erp.communication.");
    struct col {// no enum class to allow inplicit cast to int
        enum index
        {
            id,
            sender,
            recipient,
            message_type,
            received,
            prescription_id,
            sender_blob_id,
            message_for_sender,
            recipient_blob_id,
            message_for_recipient,
            prescription_type,
        };
    };
    auto& db = database();
    // TODO: The question https://dth01.ibmgcloud.net/jira/browse/ERP-5579 to Gematik is pending.
    // If the format of the prescription id in the reference implementation of the communication
    // resources has been updated the builder can be replaced again by the example as provided
    // by Gematik with "test/fhir/conversion/communication_info_req.json".
    auto builder = CommunicationJsonStringBuilder(model::Communication::MessageType::InfoReq);
    builder.setPrescriptionId(
        model::PrescriptionId::fromDatabaseId(model::PrescriptionType::apothekenpflichigeArzneimittel,815).toString());
    builder.setPayload("Hallo, ich wollte gern fragen, ob das Medikament bei Ihnen vorraetig ist.");
    builder.setAbout("#5fe6e06c-8725-46d5-aecd-e65e041ca3de");
    auto communication = model::Communication::fromJsonNoValidation(builder.createJsonString());
    communication.setSender(InsurantA);
    communication.setRecipient(mPharmacy);
    communication.setTimeSent(model::Timestamp{(int64_t)1612134000});
    auto id = db.insertCommunication(communication);
    ASSERT_TRUE(id.has_value());
    db.commitTransaction();
    auto txn = createTransaction();
    // intentionally uses '*' to get all columns of the table - so we will not forget to adapt this test if we add a row
    auto row = txn.exec_params1("SELECT * FROM erp.communication WHERE id = $1", id->toString());
    txn.commit();
    ASSERT_EQ(row.size(), 11) << "Expected table `erp.communication` to have 11 columns.";

    //  0: id uuid DEFAULT erp.gen_suuid(CURRENT_TIMESTAMP) NOT NULL,
    //     not encrypted
    //  1: sender bytea NOT NULL,
    auto senderHashed = getKeyDerivation().hashIdentity(communication.sender().value());
    EXPECT_EQ(db_model::EncryptedBlob{row[col::sender].as<db_model::postgres_bytea>()}, senderHashed);
    //  2: recipient bytea NOT NULL,
    auto recipientHashed = getKeyDerivation().hashIdentity(communication.recipient().value());
    EXPECT_EQ(db_model::EncryptedBlob{row[col::recipient].as<db_model::postgres_bytea>()}, recipientHashed);
    //  3: message_type smallint NOT NULL,
    //     not encrypted
    //  4: received timestamp with time zone,
    //     not encrypted
    //  5: prescription_id bigint,
    //     not encrypted
    //  6: prescription_type smallint,
    //     not encrypted
    //  7: sender_blob_id integer NOT NULL,
    //     not encrypted
    //  8: message_for_sender bytea NOT NULL,
    auto senderBlobId = gsl::narrow<BlobId>(row[col::sender_blob_id].as<int32_t>());
    {
        auto key = communicationKey(communication.sender().value(), senderHashed, senderBlobId);
        db_model::EncryptedBlob encryptedMessage{row[col::message_for_sender].as<db_model::postgres_bytea>()};
        std::optional<model::Communication> decryptedMessage;
        ASSERT_NO_THROW(decryptedMessage =
                            model::Communication::fromJsonNoValidation(getDBCodec().decode(encryptedMessage, key)));
        ASSERT_EQ(decryptedMessage->sender(), communication.sender());
        ASSERT_EQ(decryptedMessage->recipient(), communication.recipient());
    }
    //  8: recipient_blob_id integer NOT NULL,
    //     not encrypted
    //  9: message_for_recipient bytea NOT NULL,
    auto recipientBlobId = gsl::narrow<BlobId>(row[col::recipient_blob_id].as<int32_t>());
    {
        auto key = communicationKey(communication.recipient().value(), recipientHashed, recipientBlobId);
        db_model::EncryptedBlob encryptedMessage{row[col::message_for_recipient].as<db_model::postgres_bytea>()};
        std::optional<model::Communication> decryptedMessage;
        ASSERT_NO_THROW(decryptedMessage =
                            model::Communication::fromJsonNoValidation(getDBCodec().decode(encryptedMessage, key)));
        ASSERT_EQ(decryptedMessage->sender(), communication.sender());
        ASSERT_EQ(decryptedMessage->recipient(), communication.recipient());
    }
    A_19688.finish();
}

TEST_F(DatabaseEncryptionTest, TableAuditEvent)//NOLINT(readability-function-cognitive-complexity)
{
    static constexpr std::string_view agentName{"Max Mustermann"};
    static constexpr std::string_view pnwPzNumber{"ODAyNzY4ODEwMjU1NDg0MzEzMDEwMDAwMDAwMDA2Mzg2ODc4MjAyMjA4MzEwODA3MzY="};

    if (!usePostgres())
    {
        GTEST_SKIP();
    }
    A_19688.test("no unencrypted personal data in table erp.auditevent.");
    struct col { // no enum class to allow implicit cast to int
        enum index
        {
            id,
            kvnr_hashed,
            event_id,
            action,
            agent_type,
            observer,
            prescription_id,
            metadata,
            blob_id,
            prescription_type,
        };
    };
    model::AuditData auditData{model::AuditEventId::GET_Task,
                               model::AuditMetaData{agentName, InsurantA, pnwPzNumber},
                               model::AuditEvent::Action::read,
                               model::AuditEvent::AgentType::human,
                               std::string{InsurantA},
                               4711,
                               std::nullopt, std::nullopt};
    auto& db = database();
    auto id = db.storeAuditEventData(auditData);
    db.commitTransaction();
    auto txn = createTransaction();
    // intentionally uses '*' to get all columns of the table - so we will not forget to adapt this test if we add a row
    auto row = txn.exec_params1("SELECT * FROM erp.auditevent WHERE id = $1", id);
    txn.commit();
    ASSERT_EQ(row.size(), 10) << "Expected table `erp.auditevent` to have 10 columns.";
    auto kvnrHashed = getKeyDerivation().hashKvnr(InsurantA);
    auto blobId = gsl::narrow<BlobId>(row[col::blob_id].as<int32_t>());
    const auto& key = auditeventKey(kvnrHashed, blobId);
    //  0: id uuid DEFAULT erp.gen_suuid(CURRENT_TIMESTAMP) NOT NULL,
    //     not encrypted
    //  1: kvnr_hashed bytea NOT NULL,
    EXPECT_EQ(db_model::EncryptedBlob{row[col::kvnr_hashed].as<db_model::postgres_bytea>()}, kvnrHashed);
    //  2: event_id smallint NOT NULL,
    //     not encrypted
    //  3: action character(1) NOT NULL,
    //     not encrypted
    //  4: agent_type smallint NOT NULL,
    //     not encrypted
    //  5: observer smallint,
    //     not encrypted
    //  6: prescription_id bigint,
    //     not encrypted
    //  7: prescription_type smallint,
    //     not encrypted
    //  8: metadata bytea,
    db_model::EncryptedBlob encryptedMetaData{row[col::metadata].as<db_model::postgres_bytea>()};
    std::optional<model::AuditMetaData> decryptedMetaData;
    ASSERT_NO_THROW(decryptedMetaData =
                        model::AuditMetaData::fromJsonNoValidation(getDBCodec().decode(encryptedMetaData, key)));
    EXPECT_EQ(decryptedMetaData->agentWho(), InsurantA);
    EXPECT_EQ(decryptedMetaData->agentName(), agentName);
    EXPECT_EQ(decryptedMetaData->pnwPzNumber(), pnwPzNumber);
    //  8: blob_id integer NOT NULL
    //     not encrypted
    A_19688.finish();
}
