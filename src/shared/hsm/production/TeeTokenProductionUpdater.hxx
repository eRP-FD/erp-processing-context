/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TEETOKENPRODUCTIONUPDATER_HXX
#define ERP_PROCESSING_CONTEXT_TEETOKENPRODUCTIONUPDATER_HXX

#include "shared/hsm/TeeTokenUpdater.hxx"


class TeeTokenProductionUpdater
{
public:
    static void refreshTeeToken(HsmPool& hsmPool);
};


#endif
