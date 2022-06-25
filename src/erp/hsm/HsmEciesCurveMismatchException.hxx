/*
 * (C) Copyright IBM Deutschland GmbH 2022
 * (C) Copyright IBM Corp. 2022
 */

#ifndef ERP_PROCESSING_CONTEXT_HSM_HSM_ECIES_CURVE_MISMATCH_EXCEPTION_HXX
#define ERP_PROCESSING_CONTEXT_HSM_HSM_ECIES_CURVE_MISMATCH_EXCEPTION_HXX

#include "erp/hsm/HsmException.hxx"

class HsmEciesCurveMismatchException : public ::HsmException
{
public:
    using ::HsmException::HsmException;
};

#endif
