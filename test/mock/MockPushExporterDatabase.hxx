/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once

class HsmPool;

class MockPushExporterDatabase
{
 public:
    virtual ~MockPushExporterDatabase() = default;
    virtual void healthCheck();
    virtual void commitTransaction();
    virtual void closeConnection();
    virtual bool isCommitted() const;
};
