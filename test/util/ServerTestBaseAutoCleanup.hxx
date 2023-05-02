/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_PROCESSING_CONTEXT_TEST_UTIL_SERVERTESTBASEAUTOCLEANUP_HXX
#define ERP_PROCESSING_CONTEXT_TEST_UTIL_SERVERTESTBASEAUTOCLEANUP_HXX

#include "test/util/ServerTestBase.hxx"


class ServerTestBaseAutoCleanup : public ServerTestBase
{
public:
    explicit ServerTestBaseAutoCleanup (bool forceMockDatabase = false);

    void TearDown (void) override;

protected:
    model::Task createTask(const std::string& accessCode = ByteHelper::toHex(SecureRandomGenerator::generate(32))) override;
    model::Communication addCommunicationToDatabase(const CommunicationDescriptor& descriptor) override;

private:
    void cleanupDatabase (void);

    std::vector<model::PrescriptionId> mTasksToRemove;
    std::vector<std::pair<Uuid, std::string>> mCommunicationsToRemove;
};


#endif
