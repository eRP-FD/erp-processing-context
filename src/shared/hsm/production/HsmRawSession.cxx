/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/hsm/production/HsmProductionClient.hxx"
#include "shared/hsm/production/HsmRawSession.hxx"


HsmRawSession::HsmRawSession (void)
    : rawSession{0, 0, 0, hsmclient::HSMSessionStatus::HSMUninitialised, 0, 0, 0}
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
