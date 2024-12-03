/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_MOCKENROLMENTMANAGER_HXX
#define ERP_PROCESSING_CONTEXT_MOCKENROLMENTMANAGER_HXX

#include "shared/enrolment/EnrolmentHelper.hxx"
#include "shared/hsm/ErpTypes.hxx"

class BlobCache;


/**
 * The purpose of this class is to simulate a minimal enrolment manager that is able to run the attestation sequence
 * with the goal to store the blobs necessary for a regular operation of the processing context.
 *
 * Based on TEST_F(erpAttestationIntegrationTestFixture, AttestationSequence) in repository vau-its.
 *
 * Note that the implementation will directly access the hsm client without its HsmSession wrapper because most/all
 * of the necessary calls are not part of the operational part of the HSM interface and therefore are not included
 * in the interface of HsmSession.
 * Note also that this class is only required and can only work with a simulated or real HSM and a simulated or real
 * TPM.  It is a "Mock" only because most of its functionality is not part of the regular operations of the processing
 * context but is only used in a setup of the processing context in a non-production environment.
 */
class MockEnrolmentManager
{
public:
    static void createAndStoreAkEkAndQuoteBlob (
        TpmProxy& tpm,
        BlobCache& blobCache,
        const int32_t logLevel = 1);
    static void createAndStoreAkEkAndQuoteBlob (
        TpmProxy& tpm,
        BlobCache& blobCache,
        const std::string& certificateFilename,
        const int32_t logLevel = 1);
    static EnrolmentHelper::Blobs createAndReturnAkEkAndQuoteBlob (
        BlobCache& blobCache,
        const std::string& certificateFilename,
        const int32_t logLevel);
};


#endif
