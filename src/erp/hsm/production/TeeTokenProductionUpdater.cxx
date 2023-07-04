/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/hsm/production/TeeTokenProductionUpdater.hxx"

#if ! defined(__APPLE__)
#include "erp/enrolment/EnrolmentHelper.hxx"
#include "erp/hsm/HsmPool.hxx"
#else
#include "erp/hsm/ErpTypes.hxx"
#include "mock/tpm/TpmTestData.hxx"
#endif


void TeeTokenProductionUpdater::refreshTeeToken(HsmPool& hsmPool)
{
    EnrolmentHelper::refreshTeeToken(hsmPool);
}
