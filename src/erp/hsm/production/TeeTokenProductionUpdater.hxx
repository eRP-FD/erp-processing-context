/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEETOKENPRODUCTIONUPDATER_HXX
#define ERP_PROCESSING_CONTEXT_TEETOKENPRODUCTIONUPDATER_HXX

#include "erp/hsm/TeeTokenUpdater.hxx"


class TeeTokenProductionUpdater
{
public:
    static void refreshTeeToken(HsmPool& hsmPool);
};


#endif
