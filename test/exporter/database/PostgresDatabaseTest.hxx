/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_EXPORTER_POSTGRESDATABASETEST_HXX
#define ERP_EXPORTER_POSTGRESDATABASETEST_HXX

#include "erp/database/DatabaseFrontend.hxx"
#include "erp/database/PostgresBackend.hxx"
#include "exporter/TelematikLookup.hxx"
#include "exporter/database/CommitGuard.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontend.hxx"
#include "exporter/database/MedicationExporterPostgresBackend.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/hsm/KeyDerivation.hxx"
#include "shared/model/Kvnr.hxx"
#include "shared/model/ResourceNames.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"
#include "test/util/TestConfiguration.hxx"

#include <boost/format.hpp>
#include <gtest/gtest.h>

#if defined(__GNUC__) && __GNUC__ == 12
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#include <pqxx/pqxx>
#pragma GCC diagnostic pop
#else
#include <pqxx/pqxx>
#endif


class PostgresDatabaseTest : public ::testing::Test
{
public:

    using MedicationExporterDatabaseBackendCommitGuard = CommitGuard<MedicationExporterDatabaseBackend, commit_guard_policies::ExceptionAbortStrategy>;
    using MedicationExporterDatabaseFrontendCommitGuard = CommitGuard<MedicationExporterDatabaseFrontend, commit_guard_policies::ExceptionAbortStrategy>;

    PostgresDatabaseTest();

    virtual void cleanup()
    {
        if (usePostgres())
        {
            clearTables();
            clearErpTables();
        }
    }

    void SetUp() override
    {
        if (! usePostgres())
        {
            GTEST_SKIP() << "Skipped. Postgres DB needed for this test suite";
        }
        cleanup();
    }

    void TearDown() override
    {
        EXPECT_FALSE(databaseNeedsCommit()) << "Database transaction not committed at the end of the test";
        if (mDatabase)
        {
            mDatabase.reset();
        }
        if (mErpDatabase)
        {
            mErpDatabase.reset();
        }
        cleanup();
        if (mConnection)
        {
            mConnection.reset();
        }
        if (mErpDbConnection)
        {
            mErpDbConnection.reset();
        }
    }

    MedicationExporterDatabaseBackendCommitGuard createDbBackendCommitGuard()
    {
        auto db = std::make_unique<MedicationExporterPostgresBackend>();
        return MedicationExporterDatabaseBackendCommitGuard(std::move(db));
    }

    MedicationExporterDatabaseFrontendCommitGuard createDbFrontendCommitGuard()
    {
        auto db = std::make_unique<MedicationExporterDatabaseFrontend>(std::make_unique<MedicationExporterPostgresBackend>(), *mKeyDerivation, mTelematikLookup);
        return MedicationExporterDatabaseFrontendCommitGuard(std::move(db));
    }

    MedicationExporterDatabaseFrontend& database()
    {
        if (! mDatabase || mDatabase->getBackend().isCommitted())
        {
            Expect(usePostgres(), "database support is disabled, database should not be used");
            mDatabase = std::make_unique<MedicationExporterDatabaseFrontend>( std::make_unique<MedicationExporterPostgresBackend>(), *mKeyDerivation, mTelematikLookup);
        }
        return *mDatabase;
    }

    db_model::postgres_bytea createEncryptedBlobFromXml(std::string_view xml,
                                                        const model::FhirResourceBase::Profile& profile,
                                                        const SafeString& key)
    {
        const auto prescription = ::model::Bundle::fromXmlNoValidation(xml);
        const auto signedPrescription =
            ::model::Binary{prescription.getId().toString(),
                            ::CryptoHelper::toCadesBesSignature(prescription.serializeToXmlString()), profile};
        const auto encryptedBlobPrescription =
            mCodec.encode(signedPrescription.serializeToJsonString(), key, Compression::DictionaryUse::Default_json);
        return db_model::postgres_bytea(encryptedBlobPrescription.binarystring());
    }

    void insertTaskEvent(const model::Kvnr& kvnr, std::string_view prescription_id, model::TaskEvent::UseCase usecase,
                         model::TaskEvent::State state, std::optional<std::string> healthcareProviderPrescription,
                         std::optional<std::string> medicationDispense, std::optional<std::string> doctorIdentity,
                         std::optional<std::string> pharmacyIdentity)
    {
        const fhirtools::DefinitionKey binaryProfile{std::string{model::resource::structure_definition::binary},
                                                     ResourceTemplates::Versions::GEM_ERP_1_3};
        auto&& txn = createTransaction();
        auto kvnr_hashed = kvnrHashed(kvnr);
        model::PrescriptionId prescriptionId = model::PrescriptionId::fromString(prescription_id);
        model::Timestamp authoredOn = model::Timestamp::now();
        auto [key, derivationData] = mKeyDerivation->initialTaskKey(prescriptionId, authoredOn);
        auto [keyMedicationDispense, derivationDataMedicationDispense] =
            mKeyDerivation->initialMedicationDispenseKey(mKeyDerivation->hashKvnr(kvnr));
        db_model::Blob salt{derivationData.salt};
        auto encrypedKvnr = mCodec.encode(kvnr.id(), key, Compression::DictionaryUse::Default_json);

        std::optional<db_model::postgres_bytea> optHealthcareProviderPrescription;
        if (healthcareProviderPrescription)
        {
            optHealthcareProviderPrescription =
                createEncryptedBlobFromXml(*healthcareProviderPrescription, binaryProfile, key);
        }
        std::optional<db_model::postgres_bytea> optMedicationDispense;
        std::optional<std::string> optBlobIdMedicationDispense;
        std::optional<db_model::postgres_bytea> optSaltMedicationDispense;
        if(medicationDispense)
        {
            optMedicationDispense =
                mCodec.encode(*medicationDispense, keyMedicationDispense, Compression::DictionaryUse::Default_json)
                    .binarystring();
            optBlobIdMedicationDispense = std::to_string(derivationDataMedicationDispense.blobId);
            optSaltMedicationDispense = db_model::Blob{derivationDataMedicationDispense.salt}.binarystring();
        }
        std::optional<db_model::postgres_bytea> optDoctorIdentity;
        if (doctorIdentity)
        {
            optDoctorIdentity =
                mCodec.encode(*doctorIdentity, key, Compression::DictionaryUse::Default_json).binarystring();
        }
        std::optional<db_model::postgres_bytea> optPharmacyIdentity;
        if (pharmacyIdentity)
        {
            optPharmacyIdentity =
                mCodec.encode(*pharmacyIdentity, key, Compression::DictionaryUse::Default_json).binarystring();
        }

        txn.exec_params(
            "INSERT INTO erp_event.task_event "
            "(id, prescription_type, prescription_id, kvnr, kvnr_hashed, status, usecase, state, salt, "
            "task_key_blob_id, "
            " healthcare_provider_prescription, medication_dispense_blob_id, medication_dispense_salt, "
            "medication_dispense_bundle, doctor_identity, pharmacy_identity)"
            " VALUES "
            "($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16)",
            ++mEventIdCounter, static_cast<int16_t>(magic_enum::enum_integer(prescriptionId.type())),
            prescriptionId.toDatabaseId(), encrypedKvnr.binarystring(), kvnr_hashed, 1, magic_enum::enum_name(usecase),
            magic_enum::enum_name(state), salt.binarystring(), std::to_string(derivationData.blobId),
            optHealthcareProviderPrescription, optBlobIdMedicationDispense, optSaltMedicationDispense,
            optMedicationDispense, optDoctorIdentity, optPharmacyIdentity);
        txn.commit();
    }

    void insertTaskKvnr(const model::Kvnr& kvnr, std::int32_t retry = 0)
    {
        auto kvnr_hashed = kvnrHashed(kvnr);
        auto&& txn = createTransaction();
        txn.exec_params("INSERT INTO erp_event.kvnr"
                        "(kvnr_hashed, state, retry_count)"
                        " VALUES "
                        "($1, $2, $3)",
                        kvnr_hashed, "processing", retry);
        txn.commit();
    }

    void clearTables()
    {
        auto deleteTxn = createTransaction();
        deleteTxn.exec("DELETE FROM erp_event.task_event");
        deleteTxn.exec("DELETE FROM erp_event.kvnr");
        deleteTxn.commit();
    }

    void clearErpTables()
    {
        auto deleteTxn = createErpDbTransaction();
        deleteTxn.exec("DELETE FROM erp.auditevent");
        deleteTxn.commit();
    }

    static bool usePostgres()
    {
        return TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false);
    }

    std::basic_string<std::byte> kvnrHashed(const model::Kvnr& kvnr) const
    {
        auto kvnr_hashed = mKeyDerivation->hashKvnr(kvnr);
        auto byte_view = kvnr_hashed.binarystring();
        return std::basic_string<std::byte>(byte_view.begin(), byte_view.end());
    }

protected:
    pqxx::connection& getConnection()
    {
        Expect(usePostgres(), "database support is disabled, database should not be used");
        if (! mConnection)
        {
            mConnection = std::make_unique<pqxx::connection>(MedicationExporterPostgresBackend::defaultConnectString());
        }
        return *mConnection;
    }

    bool databaseNeedsCommit()
    {
        return mDatabase && not mDatabase->getBackend().isCommitted();
    }

    pqxx::work createTransaction()
    {
        Expect3(! databaseNeedsCommit(), "previous transaction not completed", std::logic_error);
        return pqxx::work{getConnection()};
    }

    DatabaseFrontend& erpDatabase()
    {
        if (! mErpDatabase || mErpDatabase->getBackend().isCommitted())
        {
            Expect(usePostgres(), "database support is disabled, database should not be used");
            mErpDatabase = std::make_unique<DatabaseFrontend>(
                std::make_unique<PostgresBackend>(), *mHsmPool, *mKeyDerivation);
        }
        return *mErpDatabase;
    }

    pqxx::connection& getErpDbConnection()
    {
        Expect(usePostgres(), "database support is disabled, database should not be used");
        if (! mErpDbConnection)
        {
            mErpDbConnection = std::make_unique<pqxx::connection>(PostgresConnection::defaultConnectString());
        }
        return *mErpDbConnection;
    }

    bool erpDatabaseNeedsCommit()
    {
        return mErpDatabase && not mErpDatabase->getBackend().isCommitted();
    }

    pqxx::work createErpDbTransaction()
    {
        Expect3(! erpDatabaseNeedsCommit(), "previous transaction not completed", std::logic_error);
        return pqxx::work{getErpDbConnection()};
    }

    std::tuple<db_model::Blob, db_model::EncryptedBlob> prepareNewTask(const model::PrescriptionId prescriptionId,
                                                                       const model::Kvnr kvnr,
                                                                       const model::Timestamp authoredOn)
    {
        const auto [key, derivationData] = mKeyDerivation->initialTaskKey(prescriptionId, authoredOn);
        const db_model::Blob salt{derivationData.salt};
        const auto encryptedKvnr = mCodec.encode(kvnr.id(), key, Compression::DictionaryUse::Default_json);
        return {salt, encryptedKvnr};
    }

    std::unique_ptr<MedicationExporterDatabaseFrontend> mDatabase;
    std::unique_ptr<DatabaseFrontend> mErpDatabase;
    std::unique_ptr<pqxx::connection> mConnection;
    std::shared_ptr<BlobCache> mBlobCache;
    std::unique_ptr<HsmPool> mHsmPool;
    std::unique_ptr<KeyDerivation> mKeyDerivation;
    DataBaseCodec mCodec;
    JwtBuilder mJwtBuilder;

    const JWT mJwtDoctorIdentity;
    const JWT mJwtPharmacyIdentity;
    std::optional<std::string> mDoctorIdentity;
    std::optional<std::string> mPharmacyIdentity;

    std::unique_ptr<pqxx::connection> mErpDbConnection;

    std::size_t mEventIdCounter = 0;

    std::stringstream mTelematikLookupEntries;
    TelematikLookup mTelematikLookup;
};

#endif
