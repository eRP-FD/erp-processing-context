/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/TRezeptEventProcessor.hxx"
#include "exporter/BdeMessage.hxx"
#include "exporter/ExporterRequirements.hxx"
#include "exporter/MedicationExporterMain.hxx"
#include "exporter/RunLoopScheduler.hxx"
#include "exporter/database/CommitGuard.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontend.hxx"
#include "exporter/database/TaskEventConverter.hxx"
#include "exporter/model/EventKvnr.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "mock/EpaAccountLookupMock.hxx"
#include "mock/crypto/MockCryptography.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/util/Configuration.hxx"
#include "test/exporter/database/PostgresDatabaseTest.hxx"
#include "test/exporter/util/MedicationExporterStaticData.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"

#include <boost/beast/core/error.hpp>
#include <gtest/gtest.h>
#include <memory>


class TRezeptEventProcessorTest : public PostgresDatabaseTest
{
public:
    const std::string healthcareProviderPrescription = ResourceTemplates::kbvBundleXml();

    const model::Kvnr kvnr{"X000000012"};

    const ResourceTemplates::Versions::GEM_ERP gemVer = ResourceTemplates::Versions::GEM_ERP_current();

    // ids with `3-01.2.2023001.16.101.*` return OK according to lzconfig.yaml of bfarm-mock:
    ResourceTemplates::MedicationDispenseBundleOptions options{
        .gematikVersion = gemVer,
        .medicationDispenses =
            {
                {
                    .telematikId = "3-01.2.2023001.16.101.1",
                    .whenHandedOver = model::Timestamp::now().toGermanDate(),
                    .gematikVersion = gemVer,
                    .medication = ResourceTemplates::MedicationOptions{.version = gemVer},
                },
                {
                    .telematikId = "3-01.2.2023001.16.101.1",
                    .whenHandedOver = model::Timestamp::now().toGermanDate(),
                    .gematikVersion = gemVer,
                    .medication = ResourceTemplates::MedicationOptions{.version = gemVer},
                },
            },
        .prescriptionId = model::PrescriptionId::fromString("160.123.456.789.123.58"),
    };

    const model::MedicationDispenseBundle medicationDispenseBundle =
        ResourceTemplates::internal_type::medicationDispenseBundle(options);

    // ids with `3-02.2.2023001.16.101.*` return NotFound according to lzconfig.yaml of bfarm-mock:
    ResourceTemplates::MedicationDispenseBundleOptions options1{
        .gematikVersion = gemVer,
        .medicationDispenses =
            {
                {
                    .telematikId = "3-02.2.2023001.16.101.1",
                    .whenHandedOver = model::Timestamp::now().toGermanDate(),
                    .gematikVersion = gemVer,
                    .medication = ResourceTemplates::MedicationOptions{.version = gemVer},
                },
                {
                    .telematikId = "3-02.2.2023001.16.101.1",
                    .whenHandedOver = model::Timestamp::now().toGermanDate(),
                    .gematikVersion = gemVer,
                    .medication = ResourceTemplates::MedicationOptions{.version = gemVer},
                },
            },
        .prescriptionId = model::PrescriptionId::fromString("160.123.456.789.123.58"),
    };

    const model::MedicationDispenseBundle medicationDispenseBundle1 =
        ResourceTemplates::internal_type::medicationDispenseBundle(options1);

    JwtBuilder jwtBuilder = JwtBuilder(MockCryptography::getIdpPrivateKey());

    void SetUp() override
    {
        PostgresDatabaseTest::SetUp();
    }

    static inline ThreadPool ioThreadPool;
    static inline std::shared_ptr<MedicationExporterServiceContext> serviceContext;
    static inline RunLoopScheduler runLoop;

    static const inline model::PrescriptionId prescriptionId =
        model::PrescriptionId::fromString("166.000.100.000.001.75");


    static void SetUpTestSuite()
    {

        if (! usePostgres())
        {
            GTEST_SKIP() << "Skipped. Postgres DB needed for this test suite";
        }
        ioThreadPool.setUp(1, "medication-exporter-io");
        serviceContext = std::make_shared<MedicationExporterServiceContext>(
            ioThreadPool.ioContext(), Configuration::instance(),
            MedicationExporterStaticData::makeMockMedicationExporterFactories());
        runLoop.getThreadPool().setUp(1, "threadpool runner");
        co_spawn(runLoop.getThreadPool().ioContext(), setupEpaClientPool(serviceContext), boost::asio::use_future)
            .get();
    }

    static void TearDownTestSuite()
    {
        if (usePostgres())
        {
            runLoop.shutDown();
            serviceContext.reset();
            ioThreadPool.shutDown();
        }
    }

    void cleanup() override
    {
        clearTrezeptEventTable();
    }

    testutils::ShiftFhirResourceViewsGuard shift{"erp.t-prescription-1.1",
                                                 date::floor<date::days>(model::Timestamp::now().toChronoTimePoint())};

    const std::string pharmacyIdentity =
        R"({"id": ")" + mJwtPharmacyIdentity.stringForClaim(JWT::idNumberClaim).value() + R"(", "name": ")" +
        mJwtPharmacyIdentity.stringForClaim(JWT::organizationNameClaim).value() + R"(", "oid": ")" +
        mJwtPharmacyIdentity.stringForClaim(JWT::professionOIDClaim).value() + R"("})";
};


TEST_F(TRezeptEventProcessorTest, noEventsNoFail)
{
    // No events
    {
        TRezeptEventProcessor processor(serviceContext);
        EXPECT_TRUE(processor.process() == TRezeptEventProcessor::ResultType::Idle);
    }
}

TEST_F(TRezeptEventProcessorTest, oneEventNoFail)
{
    // One event to be processed with no failure
    {
        insertTRezeptEvent(kvnr, prescriptionId.toString(), model::TaskEvent::UseCase::provideDispensation,
                           model::TaskEvent::State::pending, healthcareProviderPrescription,
                           medicationDispenseBundle.serializeToJsonString(), std::nullopt, pharmacyIdentity);
        TRezeptEventProcessor processor(serviceContext);
        EXPECT_EQ(processor.process(), TRezeptEventProcessor::ResultType::Success);
    }
}

TEST_F(TRezeptEventProcessorTest, oneEventNoFail208)
{
    // One event to be processed with no failure
    {
        // KVNR X000000013 configured to return 208 in docker-compose/bfarm/lz-config.json
        insertTRezeptEvent(model::Kvnr{"X000000013"}, prescriptionId.toString(), model::TaskEvent::UseCase::provideDispensation,
                           model::TaskEvent::State::pending, healthcareProviderPrescription,
                           medicationDispenseBundle.serializeToJsonString(), std::nullopt, pharmacyIdentity);
        TRezeptEventProcessor processor(serviceContext);
        EXPECT_EQ(processor.process(), TRezeptEventProcessor::ResultType::Success);
    }
}

TEST_F(TRezeptEventProcessorTest, searchNotFoundRetry)
{
    // One event with a single retry
    {
        // medicationDispenseBundle1 is using 3-02.2.2023001.16.101.1 which is configured for 404 in lz-config.json.
        insertTRezeptEvent(kvnr, prescriptionId.toString(), model::TaskEvent::UseCase::provideDispensation,
                           model::TaskEvent::State::pending, healthcareProviderPrescription,
                           medicationDispenseBundle1.serializeToJsonString(), std::nullopt, pharmacyIdentity, 0);
        TRezeptEventProcessor processor(serviceContext);
        EXPECT_EQ(processor.process(), TRezeptEventProcessor::ResultType::Retry);
    }
}

TEST_F(TRezeptEventProcessorTest, missingPharmacy)
{
    // One event with a single retry
    {
        // medicationDispenseBundle1 is using 3-02.2.2023001.16.101.1 which is configured for 404 in lz-config.json.
        insertTRezeptEvent(kvnr, prescriptionId.toString(), model::TaskEvent::UseCase::provideDispensation,
                           model::TaskEvent::State::pending, healthcareProviderPrescription,
                           medicationDispenseBundle1.serializeToJsonString(), std::nullopt, std::nullopt, 0);
        TRezeptEventProcessor processor(serviceContext);
        EXPECT_EQ(processor.process(), TRezeptEventProcessor::ResultType::FailureOther);
    }
}

TEST_F(TRezeptEventProcessorTest, retryLimitExceeded)
{
    // One event with retry count exceeding the limit, results into DLQ state.
    {
        const auto max_retries =
            Configuration::instance().getIntValue(ConfigurationKey::MEDICATION_EXPORTER_BFARM_CLIENT_NUM_RETRIES) + 1;
        insertTRezeptEvent(kvnr, prescriptionId.toString(), model::TaskEvent::UseCase::provideDispensation,
                           model::TaskEvent::State::pending, healthcareProviderPrescription,
                           medicationDispenseBundle.serializeToJsonString(), std::nullopt, pharmacyIdentity,
                           max_retries);
        TRezeptEventProcessor processor(serviceContext);
        EXPECT_TRUE(processor.process() == TRezeptEventProcessor::ResultType::Success);
        EXPECT_EQ("deadLetterQueue", getTRezeptEventState());
    }
}
