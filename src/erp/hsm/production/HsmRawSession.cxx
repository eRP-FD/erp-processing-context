/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/production/HsmProductionClient.hxx"
#include "erp/hsm/production/HsmRawSession.hxx"


HsmRawSession::HsmRawSession (void)
    : rawSession{0, 0, 0, hsmclient::HSMSessionStatus::HSMUninitialised, 0, 0, 0},
      identity(HsmIdentity::Identity::Work)
{
}


HsmRawSession::HsmRawSession (HsmRawSession&& other) noexcept
    : rawSession(other.rawSession),
      identity(other.identity)
{
    other.rawSession = hsmclient::HSMSession{0, 0, 0, hsmclient::HSMSessionStatus::HSMUninitialised, 0, 0, 0};
}


HsmRawSession::~HsmRawSession (void)
{
    HsmProductionClient::disconnect(*this);
}


HsmRawSession& HsmRawSession::operator= (HsmRawSession&& other) noexcept
{
    if (this != &other)
    {
        HsmProductionClient::disconnect(*this);

        rawSession = other.rawSession;

        other.rawSession = hsmclient::HSMSession{0, 0, 0, hsmclient::HSMSessionStatus::HSMUninitialised, 0, 0, 0};
    }
    return *this;
}
