/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/exporter/database/PostgresDatabaseTest.hxx"
#include "exporter/model/EventKvnr.hxx"
#include "exporter/model/TaskEvent.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "shared/compression/ZStd.hxx"
#include "test/util/ResourceTemplates.hxx"

// using namespace model;

std::shared_ptr<Compression> compressionInstance()
{
    static auto theCompressionInstance =
        std::make_shared<ZStd>(Configuration::instance().getStringValue(ConfigurationKey::ZSTD_DICTIONARY_DIR));
    return theCompressionInstance;
}

PostgresDatabaseTest::PostgresDatabaseTest()
    : mDatabase(nullptr)
    , mConnection(nullptr)
    , mCodec{compressionInstance()}
    , mJwtBuilder(MockCryptography::getIdpPrivateKey())
    , mJwtDoctorIdentity(mJwtBuilder.makeJwtArzt())
    , mJwtPharmacyIdentity(mJwtBuilder.makeJwtApotheke())
    , mDoctorIdentity(R"({"id": ")" + mJwtDoctorIdentity.stringForClaim(JWT::idNumberClaim).value() +
                      R"(", "name": ")" + mJwtDoctorIdentity.stringForClaim(JWT::organizationNameClaim).value() +
                      R"(", "oid": ")" + mJwtDoctorIdentity.stringForClaim(JWT::professionOIDClaim).value() + R"("})")
    , mPharmacyIdentity(R"({"id": ")" + mJwtPharmacyIdentity.stringForClaim(JWT::idNumberClaim).value() +
                        R"(", "name": ")" + mJwtPharmacyIdentity.stringForClaim(JWT::organizationNameClaim).value() +
                        R"(", "oid": ")" + mJwtPharmacyIdentity.stringForClaim(JWT::professionOIDClaim).value() +
                        R"("})")
{
    mBlobCache = MockBlobDatabase::createBlobCache(MockBlobCache::MockTarget::MockedHsm);
    mHsmPool = std::make_unique<HsmPool>(
        std::make_unique<HsmMockFactory>(std::make_unique<HsmMockClient>(), std::move(mBlobCache)),
        TeeTokenUpdater::createMockTeeTokenUpdaterFactory(), std::make_shared<Timer>());
    mKeyDerivation = std::make_unique<KeyDerivation>(*mHsmPool);
}

std::string printBasicString(const std::basic_string<std::byte>& byteString)
{
    std::ostringstream os;
    for (const auto& byte : byteString)
    {
        os << std::hex << std::to_integer<int>(byte) << " ";// Hexadezimale Ausgabe
    }
    return os.str();
}

TEST_F(PostgresDatabaseTest, getAllEventsForKvnr)//NOLINT(readability-function-cognitive-complexity)
{
    model::Kvnr kvnr{"X000000012"};
    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, "160.000.100.000.001.05", model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    std::nullopt);

    insertTaskEvent(kvnr, "160.000.100.000.001.05", model::TaskEvent::UseCase::cancelPrescription,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    mPharmacyIdentity);

    const auto medicationDispense = ResourceTemplates::medicationDispenseBundleXml();
    const auto medicationDispenseBundle = ::model::Bundle::fromXmlNoValidation(medicationDispense);
    insertTaskEvent(kvnr, "160.000.100.000.001.05", model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(),
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, mPharmacyIdentity);

    insertTaskEvent(kvnr, "160.000.100.000.001.05", model::TaskEvent::UseCase::cancelDispensation,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    mPharmacyIdentity);

    model::EventKvnr eventKvnr{kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::pending, 0};
    const auto events = database().getAllEventsForKvnr(eventKvnr);

    const auto prescription = ResourceTemplates::kbvBundleXml();
    const auto prescriptionBundle = model::Bundle::fromXmlNoValidation(prescription);

    ASSERT_EQ(events.size(), 4);
    {
        const auto& event = *events.at(0);
        ASSERT_TRUE(event.getKvnr() == kvnr);
        EXPECT_EQ(event.getUseCase(), model::TaskEvent::UseCase::providePrescription);

        const auto& providePrescription = dynamic_cast<const model::ProvidePrescriptionTaskEvent&>(event);
        EXPECT_EQ(providePrescription.getKbvBundle().getId(), prescriptionBundle.getId());
        EXPECT_EQ(providePrescription.getMedicationRequestAuthoredOn().toGermanDate(),
                  model::Timestamp::now().toGermanDate());

        ASSERT_TRUE(providePrescription.getQesDoctorId());
        EXPECT_EQ(providePrescription.getQesDoctorId()->id(), "1-HBA-Testkarte-883110000129084");
        EXPECT_EQ(providePrescription.getJwtDoctorId(), "0123456789");
        EXPECT_EQ(providePrescription.getJwtDoctorOrganizationName(), "Institutions- oder Organisations-Bezeichnung");
        EXPECT_EQ(providePrescription.getJwtDoctorProfessionOid(), "1.2.276.0.76.4.30");
    }
    {
        const auto& event = *events.at(1);
        ASSERT_TRUE(event.getKvnr() == kvnr);
        EXPECT_EQ(event.getUseCase(), model::TaskEvent::UseCase::cancelPrescription);

        const auto& cancelPrescription = dynamic_cast<const model::CancelPrescriptionTaskEvent&>(event);
        EXPECT_EQ(cancelPrescription.getKbvBundle().getId(), prescriptionBundle.getId());
        EXPECT_EQ(cancelPrescription.getMedicationRequestAuthoredOn().toGermanDate(),
                  model::Timestamp::now().toGermanDate());
    }
    {
        const auto& event = *events.at(2);
        ASSERT_TRUE(event.getKvnr() == kvnr);
        EXPECT_EQ(event.getUseCase(), model::TaskEvent::UseCase::provideDispensation);

        const auto& provideDispensation = dynamic_cast<const model::ProvideDispensationTaskEvent&>(event);
        EXPECT_EQ(provideDispensation.getKbvBundle().getId(), prescriptionBundle.getId());
        EXPECT_EQ(provideDispensation.getMedicationRequestAuthoredOn().toGermanDate(),
                  model::Timestamp::now().toGermanDate());

        EXPECT_EQ(provideDispensation.getMedicationDispenseBundle().getId(), medicationDispenseBundle.getId());
    }
    {
        const auto& event = *events.at(3);
        ASSERT_TRUE(event.getKvnr() == kvnr);
        EXPECT_EQ(event.getUseCase(), model::TaskEvent::UseCase::cancelDispensation);

        const auto& cancelDispensation = dynamic_cast<const model::CancelDispensationTaskEvent&>(event);
        EXPECT_EQ(cancelDispensation.getKbvBundle().getId(), prescriptionBundle.getId());
        EXPECT_EQ(cancelDispensation.getMedicationRequestAuthoredOn().toGermanDate(),
                  model::Timestamp::now().toGermanDate());
    }

    database().commitTransaction();
}

TEST_F(PostgresDatabaseTest,
       getAllEventsForKvnr_noHealthcareProviderPrescription)//NOLINT(readability-function-cognitive-complexity)
{
    model::Kvnr kvnr{"X000000012"};
    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, "160.000.100.000.001.05", model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, std::nullopt, std::nullopt, mDoctorIdentity, std::nullopt);

    model::EventKvnr eventKvnr{kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::pending, 0};
    MedicationExporterDatabaseFrontendInterface::taskevents_t events;
    EXPECT_THROW(database().getAllEventsForKvnr(eventKvnr), std::exception);
    database().commitTransaction();
}

TEST_F(PostgresDatabaseTest, getAllEventsForKvnr_medicationDispense)//NOLINT(readability-function-cognitive-complexity)
{
    model::Kvnr kvnr{"X000000012"};
    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, "160.000.100.000.001.05", model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    std::nullopt);

    model::EventKvnr eventKvnr{kvnrHashed(kvnr), std::nullopt, std::nullopt, model::EventKvnr::State::pending, 0};
    EXPECT_THROW(database().getAllEventsForKvnr(eventKvnr), std::exception);
    database().commitTransaction();
}

TEST_F(PostgresDatabaseTest, delayProcessing)//NOLINT(readability-function-cognitive-complexity)
{
    model::Kvnr kvnr1{"X000000012"};
    model::EventKvnr eventKvnr1{kvnrHashed(kvnr1), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0};
    insertTaskKvnr(kvnr1);
    insertTaskEvent(kvnr1, "160.000.100.000.001.05", model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    std::nullopt);

    model::Kvnr kvnr2{"X000000022"};
    model::EventKvnr eventKvnr2{kvnrHashed(kvnr2), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0};
    insertTaskKvnr(kvnr2);
    insertTaskEvent(kvnr2, "160.000.100.000.002.02", model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    std::nullopt);

    struct StateAndNextExport {
        model::EventKvnr::State state;
        model::Timestamp nextExport;
    };

    std::map<db_model::postgres_bytea, StateAndNextExport> before_eventKvnrs;
    {
        auto transaction = pqxx::work{getConnection()};
        auto r =
            transaction.exec_params("SELECT kvnr_hashed, state, EXTRACT(EPOCH FROM next_export) FROM erp_event.kvnr ");
        before_eventKvnrs.try_emplace(
            r.at(0, 0).as<db_model::postgres_bytea>(),
            magic_enum::enum_cast<model::EventKvnr::State>(r.at(0, 1).as<std::string>()).value(),
            model::Timestamp(r.at(0, 2).as<double>()));
        before_eventKvnrs.try_emplace(
            r.at(1, 0).as<db_model::postgres_bytea>(),
            magic_enum::enum_cast<model::EventKvnr::State>(r.at(1, 1).as<std::string>()).value(),
            model::Timestamp(r.at(1, 2).as<double>()));
        transaction.commit();
    }

    database().updateProcessingDelay(0, 5min, eventKvnr1);
    database().commitTransaction();

    std::map<db_model::postgres_bytea, StateAndNextExport> after_eventKvnrs;
    {
        auto transaction = pqxx::work{getConnection()};
        auto r =
            transaction.exec_params("SELECT kvnr_hashed, state, EXTRACT(EPOCH FROM next_export) FROM erp_event.kvnr ");
        after_eventKvnrs.try_emplace(
            r.at(0, 0).as<db_model::postgres_bytea>(),
            magic_enum::enum_cast<model::EventKvnr::State>(r.at(0, 1).as<std::string>()).value(),
            model::Timestamp(r.at(0, 2).as<double>()));
        after_eventKvnrs.try_emplace(
            r.at(1, 0).as<db_model::postgres_bytea>(),
            magic_enum::enum_cast<model::EventKvnr::State>(r.at(1, 1).as<std::string>()).value(),
            model::Timestamp(r.at(1, 2).as<double>()));
        transaction.commit();
    }

    using namespace std::chrono_literals;
    {
        EXPECT_EQ(before_eventKvnrs.at(eventKvnr1.kvnrHashed()).state, model::EventKvnr::State::processing);
        EXPECT_EQ(before_eventKvnrs.at(eventKvnr2.kvnrHashed()).state, model::EventKvnr::State::processing);

        EXPECT_EQ(after_eventKvnrs.at(eventKvnr1.kvnrHashed()).state, model::EventKvnr::State::pending);
        EXPECT_EQ(after_eventKvnrs.at(eventKvnr2.kvnrHashed()).state, model::EventKvnr::State::processing);

        const auto delay = 5min;
        const auto maxInaccurancy = 1min;
        EXPECT_TRUE(before_eventKvnrs.at(eventKvnr1.kvnrHashed()).nextExport + delay <=
                        after_eventKvnrs.at(eventKvnr1.kvnrHashed()).nextExport &&
                    after_eventKvnrs.at(eventKvnr1.kvnrHashed()).nextExport <=
                        before_eventKvnrs.at(eventKvnr1.kvnrHashed()).nextExport + delay + maxInaccurancy);
    }
}


TEST_F(PostgresDatabaseTest, processNexteventKvnr)//NOLINT(readability-function-cognitive-complexity)
{
    const model::PrescriptionId prescriptionId = model::PrescriptionId::fromString("160.000.100.000.001.05");
    const std::string kvnr_str{"X000000012"};
    const model::Kvnr kvnr{kvnr_str};
    const auto hashed_kvnr = kvnrHashed(kvnr);

    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId.toString(), model::TaskEvent::UseCase::cancelPrescription,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    std::nullopt);

    model::EventKvnr eventKvnr{hashed_kvnr, std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0};
    const auto events = database().getAllEventsForKvnr(eventKvnr);
    EXPECT_EQ(events.size(), 1);

    std::optional<model::EventKvnr> event_kvnr = database().processNextKvnr();
    ASSERT_TRUE(event_kvnr.has_value());
    EXPECT_EQ(event_kvnr.value().kvnrHashed(), hashed_kvnr);

    database().deleteAllEventsForKvnr(event_kvnr.value());

    database().commitTransaction();
}

TEST_F(PostgresDatabaseTest, deleteOneEventForKvnr)//NOLINT(readability-function-cognitive-complexity)
{
    model::Kvnr kvnr1{"X000000012"};
    model::EventKvnr eventKvnr1{kvnrHashed(kvnr1), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0};

    const auto medicationDispense = ResourceTemplates::medicationDispenseBundleXml();
    const auto medicationDispenseBundle = ::model::Bundle::fromXmlNoValidation(medicationDispense);

    insertTaskKvnr(kvnr1);
    insertTaskEvent(kvnr1, "160.000.100.000.001.05", model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    std::nullopt);
    insertTaskEvent(kvnr1, "160.000.100.000.001.05", model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(),
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, mPharmacyIdentity);

    model::Kvnr kvnr2{"X000000022"};
    model::EventKvnr eventKvnr2{kvnrHashed(kvnr2), std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0};
    insertTaskKvnr(kvnr2);
    insertTaskEvent(kvnr2, "160.000.100.000.002.02", model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    std::nullopt);
    insertTaskEvent(kvnr2, "160.000.100.000.002.02", model::TaskEvent::UseCase::provideDispensation,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(),
                    medicationDispenseBundle.serializeToJsonString(), mDoctorIdentity, mPharmacyIdentity);


    const auto events = database().getAllEventsForKvnr(eventKvnr2);
    ASSERT_EQ(events.size(), 2);

    const auto& event_kvnr2_provide = events[0];
    database().deleteOneEventForKvnr(eventKvnr2, event_kvnr2_provide->getId());
    ASSERT_EQ(database().getAllEventsForKvnr(eventKvnr2).size(), 1);

    database().commitTransaction();
}


TEST_F(PostgresDatabaseTest, isDeadletter_no)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string kvnr_str{"X000000012"};
    const model::Kvnr kvnr{kvnr_str};
    const auto hashedKvnr = kvnrHashed(kvnr);
    model::EventKvnr eventKvnr{hashedKvnr, std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0};

    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, "160.000.100.000.001.05", model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    std::nullopt);
    const auto events = database().getAllEventsForKvnr(eventKvnr);
    ASSERT_EQ(events.size(), 1);
    const auto& event = *events.at(0);

    EXPECT_EQ(database().isDeadLetter(eventKvnr, event.getPrescriptionId(), event.getPrescriptionType()), false);
    database().commitTransaction();
}

TEST_F(PostgresDatabaseTest, isDeadletter_yes)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string kvnr_str{"X000000012"};
    const model::Kvnr kvnr{kvnr_str};
    const auto hashedKvnr = kvnrHashed(kvnr);
    model::EventKvnr eventKvnr{hashedKvnr, std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0};

    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, "160.000.100.000.001.05", model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::deadLetterQueue, ResourceTemplates::kbvBundleXml(), std::nullopt,
                    mDoctorIdentity, std::nullopt);

    auto transaction = createTransaction();
    const auto results = transaction.exec_params("SELECT prescription_id, prescription_type FROM erp_event.task_event");
    ASSERT_EQ(results.size(), 1);
    transaction.commit();
    auto prescription_id = results[0].at(0).as<std::int64_t>();
    auto prescription_type = magic_enum::enum_cast<model::PrescriptionType>(
        gsl::narrow_cast<std::uint8_t>(results[0].at(1).as<std::int16_t>()));

    EXPECT_EQ(database().isDeadLetter(eventKvnr,
                                      model::PrescriptionId::fromDatabaseId(prescription_type.value(), prescription_id),
                                      prescription_type.value()),
              true);
    database().commitTransaction();
}

TEST_F(PostgresDatabaseTest, markDeadletter)//NOLINT(readability-function-cognitive-complexity)
{
    const std::string kvnr_str{"X000000012"};
    const model::Kvnr kvnr{kvnr_str};
    const auto hashedKvnr = kvnrHashed(kvnr);
    model::EventKvnr eventKvnr{hashedKvnr, std::nullopt, std::nullopt, model::EventKvnr::State::processing, 0};


    const auto prescriptionId = model::PrescriptionId::fromString("160.000.100.000.001.05");

    insertTaskKvnr(kvnr);
    insertTaskEvent(kvnr, prescriptionId.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::pending, ResourceTemplates::kbvBundleXml(), std::nullopt, mDoctorIdentity,
                    std::nullopt);
    insertTaskEvent(kvnr, prescriptionId.toString(), model::TaskEvent::UseCase::providePrescription,
                    model::TaskEvent::State::deadLetterQueue, ResourceTemplates::kbvBundleXml(), std::nullopt,
                    mDoctorIdentity, std::nullopt);

    EXPECT_EQ(database().isDeadLetter(eventKvnr, prescriptionId, prescriptionId.type()), true);
    mDatabase.reset();
    {
        auto transaction = createTransaction();
        const auto results =
            transaction.exec_params("SELECT state FROM erp_event.task_event WHERE prescription_id = "
                                    "$1 AND prescription_type = $2 ORDER BY last_modified ASC",
                                    prescriptionId.toDatabaseId(),
                                    gsl::narrow_cast<std::int16_t>(magic_enum::enum_integer(prescriptionId.type())));
        ASSERT_EQ(results.size(), 2);
        EXPECT_EQ("pending", results[0].at(0).as<std::string>());
        EXPECT_EQ("deadLetterQueue", results[1].at(0).as<std::string>());
        transaction.commit();
    }

    EXPECT_EQ(database().isDeadLetter(eventKvnr, prescriptionId, prescriptionId.type()), true);

    const auto marked = database().markDeadLetter(eventKvnr, prescriptionId, prescriptionId.type());
    EXPECT_EQ(marked, 1);
    database().commitTransaction();

    {
        auto transaction = createTransaction();
        const auto results =
            transaction.exec_params("SELECT state FROM erp_event.task_event ORDER BY last_modified ASC");
        ASSERT_EQ(results.size(), 2);
        EXPECT_EQ("deadLetterQueue", results[0].at(0).as<std::string>());
        EXPECT_EQ("deadLetterQueue", results[1].at(0).as<std::string>());
        transaction.commit();
    }
}
