/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_HSM_HSM_ECIES_CURVE_MISMATCH_EXCEPTION_HXX
#define ERP_PROCESSING_CONTEXT_HSM_HSM_ECIES_CURVE_MISMATCH_EXCEPTION_HXX

#include "shared/hsm/HsmException.hxx"

class HsmEciesCurveMismatchException : public ::HsmException
{
public:
    using ::HsmException::HsmException;
};

#endif
