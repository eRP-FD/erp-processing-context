/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_EXPORTER_TASKEVENTEXPORTERTEST_HXX
#define ERP_EXPORTER_TASKEVENTEXPORTERTEST_HXX

#include "erp/model/Binary.hxx"
#include "exporter/TelematikLookup.hxx"
#include "exporter/database/ExporterDatabaseModel.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "shared/hsm/HsmPool.hxx"
#include "shared/hsm/KeyDerivation.hxx"
#include "test/util/JwtBuilder.hxx"

#include <gtest/gtest.h>

class TaskEventConverterTest : public ::testing::Test
{
public:
    TaskEventConverterTest();

    virtual void cleanup()
    {
    }

    void SetUp() override
    {
    }

    void TearDown() override
    {
    }

    std::basic_string<std::byte> kvnrHashed(const model::Kvnr& kvnr) const
    {
        auto kvnr_hashed = mKeyDerivation->hashKvnr(kvnr);
        auto byte_view = kvnr_hashed.binarystring();
        return std::basic_string<std::byte>(byte_view.begin(), byte_view.end());
    }

protected:
    std::unique_ptr<HsmPool> mHsmPool;
    std::unique_ptr<KeyDerivation> mKeyDerivation;
    DataBaseCodec mCodec;

    model::Kvnr kvnr;
    db_model::HashedKvnr hashedKvnr;
    model::PrescriptionId prescriptionId;
    const fhirtools::DefinitionKey binaryProfile;
    model::Timestamp lastModified = model::Timestamp::now();
    model::Timestamp authoredOn = model::Timestamp::now();
    std::tuple<SafeString, OptionalDeriveKeyData> keyAndDerivationData;
    SafeString& key;
    OptionalDeriveKeyData& derivationData;
    db_model::Blob salt;
    db_model::EncryptedBlob encryptedKvnr;
    model::TaskEvent::UseCase usecase;
    model::TaskEvent::State state;

    model::Bundle prescription;
    model::Binary signedPrescription;
    model::Binary signedPrescriptionNoQes;
    model::Binary signedPrescriptionNoQesNoMapping;
    model::Binary signedPrescriptionNoQesNoSerno;
    db_model::EncryptedBlob encryptedBlobPrescription;
    db_model::EncryptedBlob encryptedBlobPrescriptionNoQes;
    db_model::EncryptedBlob encryptedBlobPrescriptionNoQesNoMapping;
    db_model::EncryptedBlob encryptedBlobPrescriptionNoQesNoSerno;

    std::tuple<SafeString, OptionalDeriveKeyData> keyAndDerivationDataMedicationDispense;
    SafeString& keyMedicationDispense;
    OptionalDeriveKeyData& derivationDataMedicationDispense;
    db_model::Blob saltMedicationDispense;
    model::Bundle medicationDispense;
    model::Binary signedMedicationDispensePrescription;
    db_model::EncryptedBlob encryptedBlobMedicationDispense;

    JwtBuilder jwtBuilder;

    std::optional<db_model::EncryptedBlob> encryptedBlobDoctorIdentity;
    std::optional<db_model::EncryptedBlob> encryptedBlobPharmacyIdentity;

    std::stringstream telematikLookupEntries;
    TelematikLookup telematikLookup;
};

#endif
