/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "test/util/ServerTestBaseAutoCleanup.hxx"

#include "erp/model/Communication.hxx"
#include "erp/util/TLog.hxx"
#include "test/mock/MockDatabase.hxx"

using namespace model;

ServerTestBaseAutoCleanup::ServerTestBaseAutoCleanup (const bool forceMockDatabase)
    : ServerTestBase(forceMockDatabase)
{
}


void ServerTestBaseAutoCleanup::TearDown (void)
{
    cleanupDatabase();
    ServerTestBase::TearDown();
}


Task ServerTestBaseAutoCleanup::createTask(const std::string& accessCode)
{
    Task task = ServerTestBase::createTask(accessCode);
    mTasksToRemove.emplace_back(task.prescriptionId());
    return task;
}

Communication ServerTestBaseAutoCleanup::addCommunicationToDatabase(const CommunicationDescriptor& descriptor)
{
    Communication communication = ServerTestBase::addCommunicationToDatabase(descriptor);
    mCommunicationsToRemove.emplace_back(communication.id().value(), model::getIdentityString(communication.sender().value()));
    return communication;
}


void ServerTestBaseAutoCleanup::cleanupDatabase (void)
{
    auto database = createDatabase();
    for (const auto& removalData : mCommunicationsToRemove)
    {
        database->deleteCommunication(removalData.first, removalData.second);
        TVLOG(1) << "deleted communication " << removalData.first.toString();
    }
    database->commitTransaction();
    for (const auto& prescriptionId : mTasksToRemove)
    {
        if (mHasPostgresSupport)
        {
            // There is no predefined method for this. Run an adhoc SQL query.
            auto transaction = createTransaction();
            transaction.exec("DELETE FROM erp.task where prescription_id = " + std::to_string(prescriptionId.toDatabaseId()));
            transaction.exec("DELETE FROM erp.task_169 where prescription_id = " + std::to_string(prescriptionId.toDatabaseId()));
            transaction.exec("DELETE FROM erp.task_200 where prescription_id = " + std::to_string(prescriptionId.toDatabaseId()));
            transaction.exec("DELETE FROM erp.task_209 where prescription_id = " + std::to_string(prescriptionId.toDatabaseId()));
            transaction.exec("DELETE FROM erp.auditevent where prescription_id = " + std::to_string(prescriptionId.toDatabaseId()));
            transaction.commit();
        }
        else
        {
            auto* mockDatabase = dynamic_cast<MockDatabase*>(database.get());
            if (mockDatabase != nullptr)
            {
                mockDatabase->deleteTask(prescriptionId);
                mockDatabase->deleteAuditEventDataForTask(prescriptionId);
            }
        }
        TVLOG(1) << "deleted task " << prescriptionId.toString();
    }
}
