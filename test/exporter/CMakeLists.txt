add_executable(exporter-test
        exporter-test.cxx
        Epa4AllTransformerTest.cxx
        Epa4AllTransformerGematikTestdata.cxx
        database/CommitGuardTest.cxx
        database/PostgresDatabaseTest.cxx
        database/TaskEventConverterTest.cxx
        eventprocessing/CancelPrescriptionTest.cxx
        eventprocessing/ProvideDispensationTest.cxx
        eventprocessing/ProvidePrescriptionTest.cxx
        mock/EpaAccountLookupClientMock.cxx
        mock/EpaMedicationClientMock.cxx
        model/ConsentDecisionResponseTypeTest.cxx
        EpaAccountLookupTest.cxx
        EventProcessorTest.cxx
        ExporterConfigurationTest.cxx
        VauAutTokenSignerTest.cxx
        it/EpaAccountLookupIt.cxx
        ../util/BlobDatabaseHelper.cxx
        ../mock/MockBlobDatabase.cxx
        ../util/HsmTestBase.cxx
        ../util/CryptoHelper.cxx
        ../util/JwtBuilder.cxx
        )

target_link_libraries(exporter-test erp-medication-exporter-files CONAN_PKG::gtest erp-test-util-lib
        )
