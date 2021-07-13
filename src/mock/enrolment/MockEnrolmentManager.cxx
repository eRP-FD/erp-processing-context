#include "mock/enrolment/MockEnrolmentManager.hxx"

#include "erp/hsm/HsmIdentity.hxx"
#include "erp/util/Hash.hxx"

#include "mock_config.h"

void MockEnrolmentManager::createAndStoreAkEkAndQuoteBlob (
    TpmProxy& tpm,
    BlobCache& blobCache,
    const size_t logLevel)
{
    createAndStoreAkEkAndQuoteBlob(tpm, blobCache, std::string(MOCK_DATA_DIR) + "/enrolment/cacertecc.crt", logLevel);
}


void MockEnrolmentManager::createAndStoreAkEkAndQuoteBlob (
    TpmProxy& tpm,
    BlobCache& blobCache,
    const std::string& certificateFilename,
    const size_t logLevel)
{
    auto blobs = EnrolmentHelper(HsmIdentity::getSetupIdentity(), certificateFilename, logLevel)
        .createBlobs(tpm);

    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::AttestationPublicKey;
        entry.name = blobs.akName;
        entry.metaAkName = blobs.akName.toArray<TpmObjectNameLength>();
        entry.blob = std::move(blobs.trustedAk);
        blobCache.storeBlob(std::move(entry));
    }
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::EndorsementKey;
        entry.name = EnrolmentHelper::getBlobName(blobs.trustedEk);
        entry.blob = std::move(blobs.trustedEk);
        blobCache.storeBlob(std::move(entry));
    }
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::Quote;
        entry.name = EnrolmentHelper::getBlobName(blobs.trustedQuote);
        entry.blob = std::move(blobs.trustedQuote);
        entry.metaPcrSet = blobs.pcrSet;
        blobCache.storeBlob(std::move(entry));
    }
}


EnrolmentHelper::Blobs MockEnrolmentManager::createAndReturnAkEkAndQuoteBlob (
    BlobCache& blobCache,
    const std::string& certificateFilename,
    const size_t logLevel)
{
    (void)blobCache; // Used only when TpmProxyDirect is used.
    TpmProxyApi tpm;

    return EnrolmentHelper(HsmIdentity::getSetupIdentity(), certificateFilename, logLevel)
        .createBlobs(tpm);
}
