/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/enrolment/EnrolmentData.hxx"

#include "erp/common/HttpStatus.hxx"
#include "erp/hsm/BlobCache.hxx"
#include "erp/enrolment/EnrolmentServer.hxx"


EnrolmentData::EnrolmentStatus EnrolmentData::getEnclaveStatus (const BlobCache& cache)
{
    auto result = cache.hasValidBlobsOfType({
        BlobType::EndorsementKey,
        BlobType::AttestationPublicKey,
        BlobType::Quote});
    ErpExpect(result.size()==3, HttpStatus::InternalServerError, "failed to determine enclave status");

    EnrolmentStatus status = EnrolmentStatus::NotEnrolled;
    if (result[0])
        if (result[1])
            if (result[2])
                status = EnrolmentStatus::QuoteKnown;
            else
                status = EnrolmentStatus::AkKnown;
        else
            status = EnrolmentStatus::EkKnown;
    else
        status = EnrolmentStatus::NotEnrolled;

    TVLOG(1) << "getting enclave status: found "
        << (result[0] ? "at least one " : "no ") << "valid endorsement key, "
        << (result[1] ? "at least one " : "no ") << "valid attestation key, "
        << (result[2] ? "at least one " : "no ") << "valid quote"
        << " => " << magic_enum::enum_name(status);

    return status;
}
