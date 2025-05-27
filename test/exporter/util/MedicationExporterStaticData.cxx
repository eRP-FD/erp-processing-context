/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "MedicationExporterStaticData.hxx"

#include "exporter/database/MainDatabaseFrontend.hxx"
#include "exporter/database/MainPostgresBackend.hxx"
#include "exporter/database/MedicationExporterPostgresBackend.hxx"
#include "exporter/database/MedicationExporterDatabaseFrontend.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "shared/util/ByteHelper.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/util/Hash.hxx"
#include "test/exporter/mock/MedicationExporterDatabaseFrontendMock.hxx"


MedicationExporterFactories MedicationExporterStaticData::makeMockMedicationExporterFactories()
{
    using exporter::MainDatabaseFrontend;
    using exporter::MainPostgresBackend;

    const auto ser2tidGivendHash =
        Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_SERNO2TID_HASH);
    const auto ser2tidFile =
        Configuration::instance().getStringValue(ConfigurationKey::MEDICATION_EXPORTER_SERNO2TID_PATH);
    const auto ser2tidContent = FileHelper::readFileAsString(ser2tidFile);
    const auto ser2tidComputedHashSum = ByteHelper::toHex(Hash::sha256(ser2tidContent));
    Expect(ser2tidComputedHashSum == ser2tidGivendHash, "Failed loading SerNo2TID.csv with hash " +
                                                            ser2tidComputedHashSum +
                                                            " - mismatched hash. Expected: " + ser2tidGivendHash);

    std::stringstream ser2tidStream(ser2tidContent);
    auto telematikLookup = std::make_shared<TelematikLookup>(ser2tidStream);

    MedicationExporterFactories factories;
    fillMockBaseFactories(factories);
    factories.exporterDatabaseFactory =
        [telematikLookup](KeyDerivation& keyDerivation,
                          TransactionMode mode) -> std::unique_ptr<MedicationExporterDatabaseFrontendInterface> {
        if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
            return std::make_unique<MedicationExporterDatabaseFrontend>(
                std::make_unique<MedicationExporterPostgresBackend>(mode), keyDerivation, *telematikLookup);
        else
            return std::make_unique<MedicationExporterDatabaseFrontendMock>();
    };
    factories.erpDatabaseFactory = [](HsmPool& hsmPool,
                                      KeyDerivation& keyDerivation) -> std::unique_ptr<MainDatabaseFrontend> {
        std::unique_ptr<MainPostgresBackend> backend;
        if (TestConfiguration::instance().getOptionalBoolValue(TestConfigurationKey::TEST_USE_POSTGRES, false))
            backend = std::make_unique<MainPostgresBackend>();
        else
            backend = std::unique_ptr<MainPostgresBackend>(nullptr);
        return std::make_unique<MainDatabaseFrontend>(std::move(backend), hsmPool, keyDerivation);
    };
    return factories;
}
