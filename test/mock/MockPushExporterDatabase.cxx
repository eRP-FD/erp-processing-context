/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "test/mock/MockPushExporterDatabase.hxx"

MockPushExporterDatabase::MockPushExporterDatabase()
{
    ON_CALL(*this, isCommitted()).WillByDefault(testing::Return(true));
}
