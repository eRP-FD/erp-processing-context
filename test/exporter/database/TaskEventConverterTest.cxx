/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "test/exporter/database/TaskEventConverterTest.hxx"
#include "exporter/database/TaskEventConverter.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "mock/hsm/HsmMockFactory.hxx"
#include "shared/compression/ZStd.hxx"
#include "shared/hsm/TeeTokenUpdater.hxx"
#include "shared/model/Binary.hxx"
#include "shared/util/Configuration.hxx"
#include "test/mock/MockBlobDatabase.hxx"
#include "test/util/CryptoHelper.hxx"
#include "test/util/JwtBuilder.hxx"
#include "test/util/ResourceTemplates.hxx"

TaskEventConverterTest::TaskEventConverterTest()
    : mHsmPool(std::make_unique<HsmPool>(
          std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(),
                                           MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm)),
          TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>()))
    , mKeyDerivation(std::make_unique<KeyDerivation>(*mHsmPool))
    , mCodec{std::make_shared<ZStd>(Configuration::instance().getStringValue(ConfigurationKey::ZSTD_DICTIONARY_DIR))}
    , kvnr{"X000000012"}
    , hashedKvnr{kvnrHashed(kvnr)}
    , prescriptionId{model::PrescriptionId::fromString("160.000.100.000.001.05")}
    , binaryProfile{std::string{model::resource::structure_definition::binary},
                    ResourceTemplates::Versions::GEM_ERP_1_3}
    , keyAndDerivationData{mKeyDerivation->initialTaskKey(prescriptionId, authoredOn)}
    , key(std::get<0>(keyAndDerivationData))
    , derivationData(std::get<1>(keyAndDerivationData))
    , salt{derivationData.salt}
    , encryptedKvnr{mCodec.encode(kvnr.id(), key, Compression::DictionaryUse::Default_json)}
    , usecase{model::TaskEvent::UseCase::providePrescription}
    , state{model::TaskEvent::State::pending}
    , prescription{::model::Bundle::fromXmlNoValidation(ResourceTemplates::kbvBundleXml())}
    , signedPrescription{::model::Binary{prescription.getId().toString(),
                                         ::CryptoHelper::toCadesBesSignature(prescription.serializeToXmlString()),
                                         binaryProfile}}
    , signedPrescriptionNoQes{::model::Binary{
          prescription.getId().toString(),
          ::CryptoHelper::toCadesBesSignature(prescription.serializeToXmlString(), "test/no-qes.pem"), binaryProfile}}
    , signedPrescriptionNoQesNoMapping{::model::Binary{
          prescription.getId().toString(),
          ::CryptoHelper::toCadesBesSignature(prescription.serializeToXmlString(), "test/no-qes-no-serno2tid.pem"), binaryProfile}}
    , signedPrescriptionNoQesNoSerno{::model::Binary{
          prescription.getId().toString(),
          ::CryptoHelper::toCadesBesSignature(prescription.serializeToXmlString(), "test/no-serno.pem"), binaryProfile}}
    , encryptedBlobPrescription{mCodec.encode(signedPrescription.serializeToJsonString(), key,
                                              Compression::DictionaryUse::Default_json)}
    , encryptedBlobPrescriptionNoQes{mCodec.encode(signedPrescriptionNoQes.serializeToJsonString(), key,
                                              Compression::DictionaryUse::Default_json)}
    , encryptedBlobPrescriptionNoQesNoMapping{mCodec.encode(signedPrescriptionNoQesNoMapping.serializeToJsonString(), key,
                                              Compression::DictionaryUse::Default_json)}
    , encryptedBlobPrescriptionNoQesNoSerno{mCodec.encode(signedPrescriptionNoQesNoSerno.serializeToJsonString(), key,
                                              Compression::DictionaryUse::Default_json)}
    , keyAndDerivationDataMedicationDispense(
          mKeyDerivation->initialMedicationDispenseKey(mKeyDerivation->hashKvnr(kvnr)))
    , keyMedicationDispense(std::get<0>(keyAndDerivationDataMedicationDispense))
    , derivationDataMedicationDispense(std::get<1>(keyAndDerivationDataMedicationDispense))
    , saltMedicationDispense(derivationDataMedicationDispense.salt)
    , medicationDispense{::model::Bundle::fromXmlNoValidation(ResourceTemplates::medicationDispenseBundleXml())}
    , signedMedicationDispensePrescription{::model::Binary{
          medicationDispense.getId().toString(),
          ::CryptoHelper::toCadesBesSignature(medicationDispense.serializeToXmlString()), binaryProfile}}
    , encryptedBlobMedicationDispense{mCodec.encode(signedMedicationDispensePrescription.serializeToJsonString(),
                                                    keyMedicationDispense, Compression::DictionaryUse::Default_json)}
    , jwtBuilder{MockCryptography::getIdpPrivateKey()}
    , telematikLookupEntries("80276883531000000202;qes-from-lookup-80276883531000000202")
    , telematikLookup(telematikLookupEntries)

{
}

TEST_F(TaskEventConverterTest, convert_ProvidePrescriptionTaskEvent)
{
    const JWT jwtDoctorIdentity = jwtBuilder.makeJwtArzt();
    encryptedBlobDoctorIdentity =
        mCodec.encode(R"({"id": ")" + jwtDoctorIdentity.stringForClaim(JWT::idNumberClaim).value() + R"(", "name": ")" +
                          jwtDoctorIdentity.stringForClaim(JWT::organizationNameClaim).value() + R"(", "oid": ")" +
                          jwtDoctorIdentity.stringForClaim(JWT::professionOIDClaim).value() + R"("})",
                      key, Compression::DictionaryUse::Default_json);
    usecase = model::TaskEvent::UseCase::providePrescription;

    const db_model::TaskEvent dbTaskEvent(
        .0, model::PrescriptionId::fromString("160.000.100.000.001.05"),
        static_cast<int16_t>(magic_enum::enum_integer(prescriptionId.type())), derivationData.blobId, salt,
        encryptedKvnr, hashedKvnr, std::string(magic_enum::enum_name(state)),
        std::string(magic_enum::enum_name(usecase)), lastModified, authoredOn, encryptedBlobPrescription,
        derivationDataMedicationDispense.blobId, saltMedicationDispense, encryptedBlobMedicationDispense,
        encryptedBlobDoctorIdentity, encryptedBlobPharmacyIdentity, 0);

    TaskEventConverter taskEventConverter(mCodec, telematikLookup);
    const auto event = taskEventConverter.convert(dbTaskEvent, key, keyMedicationDispense);

    ASSERT_TRUE(event);
    {
        EXPECT_EQ(event->getPrescriptionId(), prescriptionId);
        EXPECT_EQ(event->getPrescriptionType(), prescriptionId.type());
        EXPECT_EQ(event->getKvnr(), kvnr);
        EXPECT_EQ(event->getHashedKvnr(),
                  std::string(reinterpret_cast<const char*>(hashedKvnr.data()), hashedKvnr.size()));
        EXPECT_EQ(event->getUseCase(), usecase);
        EXPECT_EQ(event->getState(), state);
    }
    {
        EXPECT_EQ(event->getUseCase(), model::TaskEvent::UseCase::providePrescription);
        const auto& providePrescription = dynamic_cast<const model::ProvidePrescriptionTaskEvent&>(*event);
        EXPECT_EQ(providePrescription.getKbvBundle().getId(), prescription.getId());

        ASSERT_TRUE(providePrescription.getQesDoctorId());
        EXPECT_EQ(providePrescription.getQesDoctorId()->id(), "1-HBA-Testkarte-883110000129084");
        EXPECT_EQ(providePrescription.getJwtDoctorId(), "0123456789");
        EXPECT_EQ(providePrescription.getJwtDoctorOrganizationName(), "Institutions- oder Organisations-Bezeichnung");
        EXPECT_EQ(providePrescription.getJwtDoctorProfessionOid(), "1.2.276.0.76.4.30");
        EXPECT_EQ(providePrescription.getMedicationRequestAuthoredOn().toGermanDate(),
                  model::Timestamp::now().toGermanDate());
    }
}

TEST_F(TaskEventConverterTest, convert_ProvidePrescriptionTaskEventNoQes)
{
    const JWT jwtDoctorIdentity = jwtBuilder.makeJwtArzt();
    encryptedBlobDoctorIdentity =
        mCodec.encode(R"({"id": ")" + jwtDoctorIdentity.stringForClaim(JWT::idNumberClaim).value() + R"(", "name": ")" +
                          jwtDoctorIdentity.stringForClaim(JWT::organizationNameClaim).value() + R"(", "oid": ")" +
                          jwtDoctorIdentity.stringForClaim(JWT::professionOIDClaim).value() + R"("})",
                      key, Compression::DictionaryUse::Default_json);
    usecase = model::TaskEvent::UseCase::providePrescription;

    const db_model::TaskEvent dbTaskEvent(
        .0, model::PrescriptionId::fromString("160.000.100.000.001.05"),
        static_cast<int16_t>(magic_enum::enum_integer(prescriptionId.type())), derivationData.blobId, salt,
        encryptedKvnr, hashedKvnr, std::string(magic_enum::enum_name(state)),
        std::string(magic_enum::enum_name(usecase)), lastModified, authoredOn, encryptedBlobPrescriptionNoQes,
        derivationDataMedicationDispense.blobId, saltMedicationDispense, encryptedBlobMedicationDispense,
        encryptedBlobDoctorIdentity, encryptedBlobPharmacyIdentity, 0);


    TaskEventConverter taskEventConverter(mCodec, telematikLookup);
    const auto event = taskEventConverter.convert(dbTaskEvent, key, keyMedicationDispense);

    ASSERT_TRUE(event);
    {
        EXPECT_EQ(event->getPrescriptionId(), prescriptionId);
        EXPECT_EQ(event->getPrescriptionType(), prescriptionId.type());
        EXPECT_EQ(event->getKvnr(), kvnr);
        EXPECT_EQ(event->getHashedKvnr(),
                  std::string(reinterpret_cast<const char*>(hashedKvnr.data()), hashedKvnr.size()));
        EXPECT_EQ(event->getUseCase(), usecase);
        EXPECT_EQ(event->getState(), state);
    }
    {
        EXPECT_EQ(event->getUseCase(), model::TaskEvent::UseCase::providePrescription);
        const auto& providePrescription = dynamic_cast<const model::ProvidePrescriptionTaskEvent&>(*event);
        EXPECT_EQ(providePrescription.getKbvBundle().getId(), prescription.getId());

        ASSERT_TRUE(providePrescription.getQesDoctorId());
        EXPECT_EQ(providePrescription.getQesDoctorId()->id(), "qes-from-lookup-80276883531000000202");
        EXPECT_EQ(providePrescription.getJwtDoctorId(), "0123456789");
        EXPECT_EQ(providePrescription.getJwtDoctorOrganizationName(), "Institutions- oder Organisations-Bezeichnung");
        EXPECT_EQ(providePrescription.getJwtDoctorProfessionOid(), "1.2.276.0.76.4.30");
        EXPECT_EQ(providePrescription.getMedicationRequestAuthoredOn().toGermanDate(),
                  model::Timestamp::now().toGermanDate());
    }
}

TEST_F(TaskEventConverterTest, convert_ProvidePrescriptionTaskEventNoQesNoMapping)
{
    const JWT jwtDoctorIdentity = jwtBuilder.makeJwtArzt();
    encryptedBlobDoctorIdentity =
        mCodec.encode(R"({"id": ")" + jwtDoctorIdentity.stringForClaim(JWT::idNumberClaim).value() + R"(", "name": ")" +
                          jwtDoctorIdentity.stringForClaim(JWT::organizationNameClaim).value() + R"(", "oid": ")" +
                          jwtDoctorIdentity.stringForClaim(JWT::professionOIDClaim).value() + R"("})",
                      key, Compression::DictionaryUse::Default_json);
    usecase = model::TaskEvent::UseCase::providePrescription;

    const db_model::TaskEvent dbTaskEvent(
        .0, model::PrescriptionId::fromString("160.000.100.000.001.05"),
        static_cast<int16_t>(magic_enum::enum_integer(prescriptionId.type())), derivationData.blobId, salt,
        encryptedKvnr, hashedKvnr, std::string(magic_enum::enum_name(state)),
        std::string(magic_enum::enum_name(usecase)), lastModified, authoredOn, encryptedBlobPrescriptionNoQesNoMapping,
        derivationDataMedicationDispense.blobId, saltMedicationDispense, encryptedBlobMedicationDispense,
        encryptedBlobDoctorIdentity, encryptedBlobPharmacyIdentity, 0);


    TaskEventConverter taskEventConverter(mCodec, telematikLookup);
    const auto event = taskEventConverter.convert(dbTaskEvent, key, keyMedicationDispense);

    ASSERT_TRUE(event);
    {
        EXPECT_EQ(event->getPrescriptionId(), prescriptionId);
        EXPECT_EQ(event->getPrescriptionType(), prescriptionId.type());
        EXPECT_EQ(event->getKvnr(), kvnr);
        EXPECT_EQ(event->getHashedKvnr(),
                  std::string(reinterpret_cast<const char*>(hashedKvnr.data()), hashedKvnr.size()));
        EXPECT_EQ(event->getUseCase(), usecase);
        EXPECT_EQ(event->getState(), state);
    }
    {
        EXPECT_EQ(event->getUseCase(), model::TaskEvent::UseCase::providePrescription);
        const auto& providePrescription = dynamic_cast<const model::ProvidePrescriptionTaskEvent&>(*event);
        EXPECT_EQ(providePrescription.getKbvBundle().getId(), prescription.getId());

        EXPECT_FALSE(providePrescription.getQesDoctorId());
    }
}

TEST_F(TaskEventConverterTest, convert_ProvidePrescriptionTaskEventNoQesNoSerno)
{
    const JWT jwtDoctorIdentity = jwtBuilder.makeJwtArzt();
    encryptedBlobDoctorIdentity =
        mCodec.encode(R"({"id": ")" + jwtDoctorIdentity.stringForClaim(JWT::idNumberClaim).value() + R"(", "name": ")" +
                          jwtDoctorIdentity.stringForClaim(JWT::organizationNameClaim).value() + R"(", "oid": ")" +
                          jwtDoctorIdentity.stringForClaim(JWT::professionOIDClaim).value() + R"("})",
                      key, Compression::DictionaryUse::Default_json);
    usecase = model::TaskEvent::UseCase::providePrescription;

    const db_model::TaskEvent dbTaskEvent(
        .0, model::PrescriptionId::fromString("160.000.100.000.001.05"),
        static_cast<int16_t>(magic_enum::enum_integer(prescriptionId.type())), derivationData.blobId, salt,
        encryptedKvnr, hashedKvnr, std::string(magic_enum::enum_name(state)),
        std::string(magic_enum::enum_name(usecase)), lastModified, authoredOn, encryptedBlobPrescriptionNoQesNoSerno,
        derivationDataMedicationDispense.blobId, saltMedicationDispense, encryptedBlobMedicationDispense,
        encryptedBlobDoctorIdentity, encryptedBlobPharmacyIdentity, 0);

    TaskEventConverter taskEventConverter(mCodec, telematikLookup);
    const auto event = taskEventConverter.convert(dbTaskEvent, key, keyMedicationDispense);

    ASSERT_TRUE(event);
    {
        EXPECT_EQ(event->getPrescriptionId(), prescriptionId);
        EXPECT_EQ(event->getPrescriptionType(), prescriptionId.type());
        EXPECT_EQ(event->getKvnr(), kvnr);
        EXPECT_EQ(event->getHashedKvnr(),
                  std::string(reinterpret_cast<const char*>(hashedKvnr.data()), hashedKvnr.size()));
        EXPECT_EQ(event->getUseCase(), usecase);
        EXPECT_EQ(event->getState(), state);
    }
    {
        EXPECT_EQ(event->getUseCase(), model::TaskEvent::UseCase::providePrescription);
        const auto& providePrescription = dynamic_cast<const model::ProvidePrescriptionTaskEvent&>(*event);
        EXPECT_EQ(providePrescription.getKbvBundle().getId(), prescription.getId());

        EXPECT_FALSE(providePrescription.getQesDoctorId());
    }
}

TEST_F(TaskEventConverterTest, convert_CancelPrescriptionTaskEvent)
{
    encryptedBlobDoctorIdentity =
        mCodec.encode(jwtBuilder.makeJwtArzt().serialize(), key, Compression::DictionaryUse::Default_json);
    usecase = model::TaskEvent::UseCase::cancelPrescription;

    const db_model::TaskEvent dbTaskEvent(
        .0, model::PrescriptionId::fromString("160.000.100.000.001.05"),
        static_cast<int16_t>(magic_enum::enum_integer(prescriptionId.type())), derivationData.blobId, salt,
        encryptedKvnr, hashedKvnr, std::string(magic_enum::enum_name(state)),
        std::string(magic_enum::enum_name(usecase)), lastModified, authoredOn, encryptedBlobPrescription,
        derivationDataMedicationDispense.blobId, saltMedicationDispense, encryptedBlobMedicationDispense,
        encryptedBlobDoctorIdentity, encryptedBlobPharmacyIdentity, 0);

    TaskEventConverter taskEventConverter(mCodec, telematikLookup);
    const auto event = taskEventConverter.convert(dbTaskEvent, key, keyMedicationDispense);

    ASSERT_TRUE(event);
    {
        EXPECT_EQ(event->getPrescriptionId(), prescriptionId);
        EXPECT_EQ(event->getPrescriptionType(), prescriptionId.type());
        EXPECT_EQ(event->getKvnr(), kvnr);
        EXPECT_EQ(event->getHashedKvnr(),
                  std::string(reinterpret_cast<const char*>(hashedKvnr.data()), hashedKvnr.size()));
        EXPECT_EQ(event->getUseCase(), usecase);
        EXPECT_EQ(event->getState(), state);
    }
    {
        EXPECT_EQ(event->getUseCase(), model::TaskEvent::UseCase::cancelPrescription);
        const auto& cancelPrescription = dynamic_cast<const model::CancelPrescriptionTaskEvent&>(*event);
        EXPECT_EQ(cancelPrescription.getKbvBundle().getId(), prescription.getId());

        EXPECT_EQ(cancelPrescription.getMedicationRequestAuthoredOn().toGermanDate(),
                  model::Timestamp::now().toGermanDate());
    }
}

TEST_F(TaskEventConverterTest, convert_ProvideDispensationTaskEvent)
{
    const JWT jwtDoctorIdentity = jwtBuilder.makeJwtArzt();
    encryptedBlobDoctorIdentity =
        mCodec.encode(R"({"id": ")" + jwtDoctorIdentity.stringForClaim(JWT::idNumberClaim).value() + R"(", "name": ")" +
                          jwtDoctorIdentity.stringForClaim(JWT::organizationNameClaim).value() + R"(", "oid": ")" +
                          jwtDoctorIdentity.stringForClaim(JWT::professionOIDClaim).value() + R"("})",
                      key, Compression::DictionaryUse::Default_json);
    const JWT jwtPharmacyIdentity = jwtBuilder.makeJwtApotheke();
    encryptedBlobPharmacyIdentity = mCodec.encode(
        R"({"id": ")" + jwtPharmacyIdentity.stringForClaim(JWT::idNumberClaim).value() + R"(", "name": ")" +
            jwtPharmacyIdentity.stringForClaim(JWT::organizationNameClaim).value() + R"(", "oid": ")" +
            jwtPharmacyIdentity.stringForClaim(JWT::professionOIDClaim).value() + R"("})",
        key, Compression::DictionaryUse::Default_json);
    usecase = model::TaskEvent::UseCase::provideDispensation;

    const db_model::TaskEvent dbTaskEvent(
        .0, model::PrescriptionId::fromString("160.000.100.000.001.05"),
        static_cast<int16_t>(magic_enum::enum_integer(prescriptionId.type())), derivationData.blobId, salt,
        encryptedKvnr, hashedKvnr, std::string(magic_enum::enum_name(state)),
        std::string(magic_enum::enum_name(usecase)), lastModified, authoredOn, encryptedBlobPrescription,
        derivationDataMedicationDispense.blobId, saltMedicationDispense, encryptedBlobMedicationDispense,
        encryptedBlobDoctorIdentity, encryptedBlobPharmacyIdentity, 0);

    TaskEventConverter taskEventConverter(mCodec, telematikLookup);
    const auto event = taskEventConverter.convert(dbTaskEvent, key, keyMedicationDispense);

    ASSERT_TRUE(event);
    {
        EXPECT_EQ(event->getPrescriptionId(), prescriptionId);
        EXPECT_EQ(event->getPrescriptionType(), prescriptionId.type());
        EXPECT_EQ(event->getKvnr(), kvnr);
        EXPECT_EQ(event->getHashedKvnr(),
                  std::string(reinterpret_cast<const char*>(hashedKvnr.data()), hashedKvnr.size()));
        EXPECT_EQ(event->getUseCase(), usecase);
        EXPECT_EQ(event->getState(), state);
    }
    {
        EXPECT_EQ(event->getUseCase(), model::TaskEvent::UseCase::provideDispensation);
        const auto& provideDispensation = dynamic_cast<const model::ProvideDispensationTaskEvent&>(*event);
        EXPECT_EQ(provideDispensation.getKbvBundle().getId(), prescription.getId());
        EXPECT_EQ(provideDispensation.getMedicationDispenseBundle().getId(), medicationDispense.getId());

        ASSERT_TRUE(provideDispensation.getQesDoctorId());
        EXPECT_EQ(provideDispensation.getQesDoctorId()->id(), "1-HBA-Testkarte-883110000129084");
        EXPECT_EQ(provideDispensation.getJwtDoctorId(), "0123456789");
        EXPECT_EQ(provideDispensation.getJwtDoctorOrganizationName(), "Institutions- oder Organisations-Bezeichnung");
        EXPECT_EQ(provideDispensation.getJwtDoctorProfessionOid(), "1.2.276.0.76.4.30");
        EXPECT_EQ(provideDispensation.getMedicationRequestAuthoredOn().toGermanDate(),
                  model::Timestamp::now().toGermanDate());

        EXPECT_EQ(provideDispensation.getJwtPharmacyId(), "3-SMC-B-Testkarte-883110000120312");
        EXPECT_EQ(provideDispensation.getJwtPharmacyOrganizationName(), "Institutions- oder Organisations-Bezeichnung");
        EXPECT_EQ(provideDispensation.getJwtPharmacyProfessionOid(), "1.2.276.0.76.4.54");
    }
}

TEST_F(TaskEventConverterTest, convert_CancelDispensationTaskEvent)
{
    encryptedBlobDoctorIdentity =
        mCodec.encode(jwtBuilder.makeJwtArzt().serialize(), key, Compression::DictionaryUse::Default_json);
    usecase = model::TaskEvent::UseCase::cancelDispensation;

    const db_model::TaskEvent dbTaskEvent(
        .0, model::PrescriptionId::fromString("160.000.100.000.001.05"),
        static_cast<int16_t>(magic_enum::enum_integer(prescriptionId.type())), derivationData.blobId, salt,
        encryptedKvnr, hashedKvnr, std::string(magic_enum::enum_name(state)),
        std::string(magic_enum::enum_name(usecase)), lastModified, authoredOn, encryptedBlobPrescription,
        derivationDataMedicationDispense.blobId, saltMedicationDispense, encryptedBlobMedicationDispense,
        encryptedBlobDoctorIdentity, encryptedBlobPharmacyIdentity, 0);

    TaskEventConverter taskEventConverter(mCodec, telematikLookup);
    const auto event = taskEventConverter.convert(dbTaskEvent, key, keyMedicationDispense);

    ASSERT_TRUE(event);
    {
        EXPECT_EQ(event->getPrescriptionId(), prescriptionId);
        EXPECT_EQ(event->getPrescriptionType(), prescriptionId.type());
        EXPECT_EQ(event->getKvnr(), kvnr);
        EXPECT_EQ(event->getHashedKvnr(),
                  std::string(reinterpret_cast<const char*>(hashedKvnr.data()), hashedKvnr.size()));
        EXPECT_EQ(event->getUseCase(), usecase);
        EXPECT_EQ(event->getState(), state);
    }
    {
        EXPECT_EQ(event->getUseCase(), model::TaskEvent::UseCase::cancelDispensation);
        const auto& cancelDispensation = dynamic_cast<const model::CancelDispensationTaskEvent&>(*event);
        EXPECT_EQ(cancelDispensation.getKbvBundle().getId(), prescription.getId());

        EXPECT_EQ(cancelDispensation.getMedicationRequestAuthoredOn().toGermanDate(),
                  model::Timestamp::now().toGermanDate());
    }
}