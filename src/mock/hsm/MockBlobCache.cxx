/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "mock/hsm/MockBlobCache.hxx"

#include "erp/database/PostgresBackend.hxx"
#include "shared/crypto/Certificate.hxx"
#include "shared/hsm/BlobCache.hxx"
#include "shared/hsm/production/ProductionBlobDatabase.hxx"
#include "shared/util/Base64.hxx"
#include "shared/util/Configuration.hxx"
#include "shared/util/TLog.hxx"

#include "mock/crypto/MockCryptography.hxx"
#if WITH_HSM_TPM_PRODUCTION > 0
#include "mock/enrolment/MockEnrolmentManager.hxx"
#endif
#include "mock/tpm/TpmTestData.hxx"

#include "mock_config.h"


namespace
{
    bool isBlobTypeAlreadyInitialized (BlobCache& cache, const BlobType type)
    {
        const auto flags = cache.hasValidBlobsOfType({type});
        return flags.size() == 1 && flags[0];
    }
}


std::shared_ptr<BlobCache> MockBlobCache::createAndSetup (
    const MockTarget target,
    std::unique_ptr<BlobDatabase>&& blobDatabase)
{
    auto blobCache = std::make_shared<BlobCache>(std::move(blobDatabase));
    setupBlobCache(target, *blobCache);
    return blobCache;
}

void MockBlobCache::setupBlobCache (
    MockTarget target,
    BlobCache& blobCache)
{
    setupBlobCacheForAllTargets(blobCache);

    switch(target)
    {
        case MockTarget::SimulatedHsm:
            TVLOG(1) << "setting up blob cache with values for simulated HSM";
            setupBlobCacheForSimulatedHsm(blobCache);
            break;
        case MockTarget::MockedHsm:
            TVLOG(1) << "setting up blob cache with values for mocked HSM";
            setupBlobCacheForMockedHsm(blobCache);
            break;
    }
}


void MockBlobCache::setupBlobCacheForAllTargets (BlobCache& blobCache)
{
    // For key derivation. Two blobs per resource type. Due to the limited static data set we use the same key but with
    // different names.
    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::TaskKeyDerivation))
    {
        auto blob = ErpBlob::fromCDump(tpm::task_derivation_key_blob_base64);
        {
            BlobDatabase::Entry entry;
            entry.type = BlobType::TaskKeyDerivation;
            entry.name = ErpVector::create("task-1");
            entry.blob = ErpBlob(blob);
            blobCache.storeBlob(std::move(entry));
        }
        ++blob.generation; // Simulate another generation.
        {
            BlobDatabase::Entry entry;
            entry.type = BlobType::TaskKeyDerivation;
            entry.name = ErpVector::create("task-2");
            entry.blob = std::move(blob);
            blobCache.storeBlob(std::move(entry));
        }
    }

    // TODO: create static data blobs for the other two derivation key types.
    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::CommunicationKeyDerivation))
    {
        auto blob = ErpBlob::fromCDump(tpm::task_derivation_key_blob_base64);
        {
            BlobDatabase::Entry entry;
            entry.type = BlobType::CommunicationKeyDerivation;
            entry.name = ErpVector::create("communication-3");
            entry.blob = ErpBlob(blob);
            blobCache.storeBlob(std::move(entry));
        }
        ++blob.generation; // Simulate another generation.
        {
            BlobDatabase::Entry entry;
            entry.type = BlobType::CommunicationKeyDerivation;
            entry.name = ErpVector::create("communication-4");
            entry.blob = std::move(blob);
            blobCache.storeBlob(std::move(entry));
        }
    }

    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::AuditLogKeyDerivation))
    {
        auto blob = ErpBlob::fromCDump(tpm::task_derivation_key_blob_base64);
        {
            BlobDatabase::Entry entry;
            entry.type = BlobType::AuditLogKeyDerivation;
            entry.name = ErpVector::create("audit-log-7");
            entry.blob = ErpBlob(blob);
            blobCache.storeBlob(std::move(entry));
        }
        ++blob.generation; // Simulate another generation.
        {
            BlobDatabase::Entry entry;
            entry.type = BlobType::AuditLogKeyDerivation;
            entry.name = ErpVector::create("audit-log-8");
            entry.blob = std::move(blob);
            blobCache.storeBlob(std::move(entry));
        }
    }

    if (! isBlobTypeAlreadyInitialized(blobCache, ::BlobType::ChargeItemKeyDerivation))
    {
        auto blob = ErpBlob::fromCDump(tpm::task_derivation_key_blob_base64);

        {
            ::BlobDatabase::Entry entry;
            entry.type = ::BlobType::ChargeItemKeyDerivation;
            entry.name = ::ErpVector::create("charge-item-9");
            entry.blob = ::ErpBlob{blob};
            blobCache.storeBlob(::std::move(entry));
        }

        ++blob.generation;// Simulate another generation.

        {
            BlobDatabase::Entry entry;
            entry.type = ::BlobType::ChargeItemKeyDerivation;
            entry.name = ::ErpVector::create("charge-item-10");
            entry.blob = ::std::move(blob);
            blobCache.storeBlob(::std::move(entry));
        }
    }
}


void MockBlobCache::setupBlobCacheForSimulatedHsm (BlobCache& blobCache)
{
#if WITH_HSM_TPM_PRODUCTION > 0
    TpmProxyDirect tpm (blobCache);
    MockEnrolmentManager::createAndStoreAkEkAndQuoteBlob(
        tpm,
        blobCache,
        std::string(MOCK_DATA_DIR) + "/enrolment/cacertecc.crt",
        1);
#else
    Fail2("cannot use simulated HSM if #if WITH_HSM_TPM_PRODUCTION is not defined", std::logic_error);
#endif

    // Ecies key.
    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::EciesKeypair))
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::EciesKeypair;
        entry.name = ErpVector::create("ecies");
        entry.blob = ErpBlob::fromCDump(tpm::eciesKeyPair_blob_base64);
        blobCache.storeBlob(std::move(entry));
    }

    // Hash key, a 256 Bit Salt.
    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::KvnrHashKey))
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::KvnrHashKey;
        entry.name = ErpVector::create("kvnr-hash-key");
        entry.blob = ErpBlob::fromCDump(tpm::hashKey_blob_base64);
        blobCache.storeBlob(std::move(entry));
    }
    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::TelematikIdHashKey))
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::TelematikIdHashKey;
        entry.name = ErpVector::create("tid-hash-key");
        entry.blob = ErpBlob::fromCDump(tpm::hashKey_blob_base64);
        blobCache.storeBlob(std::move(entry));
    }

    // VAU SIG aka ID.FD.SIG
    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::VauSig))
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::VauSig;
        entry.name = ErpVector::create("vau-sig");
        entry.blob = ErpBlob::fromCDump(tpm::vauSigKeupair_blob_base64);
        entry.certificate = tpm::vauSigCertificate_base64;
        blobCache.storeBlob(std::move(entry));
    }

    // VAU AUT
    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::VauAut))
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::VauAut;
        entry.name = ErpVector::create("vau-aut");
        entry.blob = ErpBlob::fromCDump(tpm::vauAutKeyPair_blob_base64);
        entry.certificate = tpm::vauAutCertificate_base64;
        blobCache.storeBlob(std::move(entry));
    }
}


void MockBlobCache::setupBlobCacheForMockedHsm (BlobCache& blobCache)
{
    // For a simulated enrolment
    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::EndorsementKey))
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::EndorsementKey;
        entry.name = ErpVector::create("ek-1");
        entry.blob = ErpBlob("endorsement key", 1);
        blobCache.storeBlob(std::move(entry));
    }
    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::AttestationPublicKey))
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::AttestationPublicKey;
        entry.name = ErpVector::create(Base64::decodeToString(tpm::attestationKeyName_base64));
        entry.blob = ErpBlob::fromCDump(tpm::TrustedAk_blob_base64);
        entry.metaAkName = ErpArray<TpmObjectNameLength>::create(Base64::decodeToString(tpm::attestationKeyName_base64));
        blobCache.storeBlob(std::move(entry));
    }
    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::Quote))
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::Quote;
        entry.name = ErpVector::create("quote");
        entry.blob = ErpBlob::fromCDump(tpm::TrustedQuote_blob_base64);
        blobCache.storeBlob(std::move(entry));
    }

    // Ecies key.
    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::EciesKeypair))
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::EciesKeypair;
        entry.name = ErpVector::create("ecies");
        entry.blob = ErpBlob(MockCryptography::getEciesPrivateKeyPem(), 1);
        blobCache.storeBlob(std::move(entry));
    }

    // Hash key, a 256 Bit Salts.
    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::KvnrHashKey))
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::KvnrHashKey;
        entry.name = ErpVector::create("kvnr-hash-key");
        entry.blob = ErpBlob("< a 32 byte/256 bit KVNR salt  >", 11);
        blobCache.storeBlob(std::move(entry));
    }
    if ( ! isBlobTypeAlreadyInitialized(blobCache, BlobType::TelematikIdHashKey))
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::TelematikIdHashKey;
        entry.name = ErpVector::create("tid-hash-key");
        entry.blob = ErpBlob("< a 32 byte/256bit TID salt    >", 11);
        blobCache.storeBlob(std::move(entry));
    }

    // VAU SIG aka ID.FD.SIG
    if (! isBlobTypeAlreadyInitialized(blobCache, BlobType::VauSig))
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::VauSig;
        entry.name = ErpVector::create("vau-sig");
        entry.blob = ErpBlob(std::string_view(MockCryptography::getIdFdSigPrivateKeyPkcs8()), 11);
        entry.certificate = tpm::vauSigCertificate_base64;
        blobCache.storeBlob(std::move(entry));
    }

    // VAU AUT
    if (! isBlobTypeAlreadyInitialized(blobCache, BlobType::VauAut))
    {
        BlobDatabase::Entry entry;
        entry.type = BlobType::VauAut;
        entry.name = ErpVector::create("vau-aut");
        entry.blob = ErpBlob(std::string_view(MockCryptography::getVauAutPrivateKeyPem()), 11);
        entry.certificate = MockCryptography::getVauAutCertificate().toBase64Der();
        blobCache.storeBlob(std::move(entry));
    }
}
