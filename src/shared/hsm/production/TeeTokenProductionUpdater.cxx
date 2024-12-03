/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/hsm/production/TeeTokenProductionUpdater.hxx"

#if ! defined(__APPLE__)
#include "shared/enrolment/EnrolmentHelper.hxx"
#include "shared/hsm/HsmPool.hxx"
#else
#include "shared/hsm/ErpTypes.hxx"
#include "mock/tpm/TpmTestData.hxx"
#endif


void TeeTokenProductionUpdater::refreshTeeToken(HsmPool& hsmPool)
{
    EnrolmentHelper::refreshTeeToken(hsmPool);
}
