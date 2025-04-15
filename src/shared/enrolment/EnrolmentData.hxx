/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_ENROLMENTDATA_HXX
#define ERP_PROCESSING_CONTEXT_ENROLMENTDATA_HXX


#include "shared/util/SafeString.hxx"
#include "shared/util/Uuid.hxx"


class BlobCache;


class EnrolmentData
{
public:
    enum class EnrolmentStatus
    {
        NotEnrolled,
        EkKnown,
        AkKnown,
        QuoteKnown
    };
    struct TpmEndorsementKey
    {
        SafeString certificate;
        std::string ekName;
    };
    struct TpmAttestationKey
    {
        SafeString publicKey;
        std::string akName;
    };


    Uuid enclaveId;

    static EnrolmentStatus getEnclaveStatus (const BlobCache& cache);
};


#endif
