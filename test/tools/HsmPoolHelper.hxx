/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_HSMPOOLHELPER_HXX
#define ERP_PROCESSING_CONTEXT_HSMPOOLHELPER_HXX

#include "shared/util/Expect.hxx"
#include "shared/hsm/HsmPool.hxx"


/**
 * This helper allows us to e.g. clear the HsmPool instance, while keeping it alive.
 */
class HsmPoolHelper
{
public:
    static void resetHsmPool (HsmPool& pool);
};


#endif
