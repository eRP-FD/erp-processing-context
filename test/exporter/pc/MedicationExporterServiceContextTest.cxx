/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "exporter/MedicationExporterMain.hxx"
#include "exporter/pc/MedicationExporterFactories.hxx"
#include "exporter/pc/MedicationExporterServiceContext.hxx"
#include "shared/util/Configuration.hxx"
#include "test/exporter/mock/MedicationExporterDatabaseFrontendMock.hxx"
#include "test/exporter/util/MedicationExporterStaticData.hxx"
#include "test/util/EnvironmentVariableGuard.hxx"
#include "test/util/StaticData.hxx"

#include <boost/asio/io_context.hpp>
#include <pqxx/connection>

class MedicationExporterServiceContextTest : public testing::Test
{
};


TEST_F(MedicationExporterServiceContextTest, transaction)
{
    EnvironmentVariableGuard noPostgres(TestConfigurationKey::TEST_USE_POSTGRES, "false");
    testing::NiceMock<MedicationExporterDatabaseFrontendMock> frontendMock;

    boost::asio::io_context ioContext;
    MedicationExporterFactories factories = MedicationExporterStaticData::makeMockMedicationExporterFactories();
    factories.exporterDatabaseFactory =
        [&](KeyDerivation&, TransactionMode) -> std::unique_ptr<MedicationExporterDatabaseFrontendInterface> {
        return std::make_unique<MedicationExporterDatabaseFrontendProxy>(&frontendMock);
    };


    EXPECT_CALL(frontendMock, healthCheck)
        .WillOnce(::testing::Throw(pqxx::broken_connection{"down"}))
        .WillOnce(::testing::Return());

    MedicationExporterServiceContext serviceContext{ioContext, Configuration::instance(), factories};
    serviceContext.transaction(TransactionMode::autocommit, [](auto& db) {
        db.healthCheck();
    });
}
