/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_ERP_DATABASE_POSTGRESDATABASECOMMUNICATIONTEST_HXX
#define ERP_PROCESSING_CONTEXT_TEST_ERP_DATABASE_POSTGRESDATABASECOMMUNICATIONTEST_HXX

#include "test/erp/database/PostgresDatabaseTest.hxx"


class PostgresDatabaseCommunicationTest : public PostgresDatabaseTest, public testing::WithParamInterface<model::PrescriptionType>
{
public:
    PostgresDatabaseCommunicationTest();
    void cleanup() override;
protected:
    model::PrescriptionId insertTask(model::Task& task);
    std::optional<Uuid> insertCommunication(model::Communication& communication);
    void deleteCommunication(const Uuid& communicationId, const std::string& sender);
    uint64_t countCommunications();
    /**
     * Retrieve the 'communication' object from the database with the given id.
     */
    std::optional<model::Communication>
    retrieveCommunication(const Uuid& communicationId, const std::string_view& sender);
    /**
     * Make sure that there are no left overs from previous tests.
     */
    void verifyDatabaseIsTidy() override;

    UrlArguments searchForSent (const std::string& sentString);
    UrlArguments searchForReceived (const std::string& receivedString);

    std::string taskFile() const;
};

#endif //ERP_PROCESSING_CONTEXT_TEST_ERP_DATABASE_POSTGRESDATABASECOMMUNICATIONTEST_HXX
