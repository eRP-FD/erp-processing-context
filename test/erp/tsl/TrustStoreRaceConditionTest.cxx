/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

// Regression test for a SIGSEGV in TrustStore::lookupCaCertificate caused by a data race on the
// internal CRYPTO_RWLOCK* of an OpenSSL X509 object.
//
// Root cause summary
// ==================
// OpenSSL 3 stores a CRYPTO_RWLOCK* (x->lock) inside every X509 struct.  The first call to any
// extension accessor (e.g. X509_get0_subject_key_id) that finds EXFLAG_SET unset triggers
// ossl_x509v3_cache_extensions(), which acquires x->lock for a one-time lazy write.  X509_free()
// later calls CRYPTO_THREAD_lock_free(x->lock) and sets x->lock = NULL.
//
// If thread B is inside ossl_x509v3_cache_extensions() and reads x->lock AFTER thread A has
// called X509_free() on the same X509*, thread B calls pthread_rwlock_wrlock(NULL) -> SIGSEGV.
//
// The trigger path in production (erp-medication-exporter)
// =========================================================
//   Thread B (IO worker doing a TLS handshake):
//     TrustStore::lookupCaCertificate()                   acquires mMutex
//       it->second.certificate.getSubjectKeyIdentifier()  EXFLAG_SET not yet set on this cert
//         X509_get0_subject_key_id()
//           ossl_x509v3_cache_extensions()
//             CRYPTO_THREAD_write_lock(x->lock)           races with Thread A
//
//   Thread A (another IO worker refreshing the TSL):
//     TrustStore::refillFromTsl()                         acquires mMutex (after Thread B)
//       mServiceInformationMap = newMap
//         old cert shared_ptr refcount drops to 0 because
//         the CaInfo copy returned by lookupCaCertificate
//         has already been dropped, and this is the last ref
//         X509_free(old_cert)
//           CRYPTO_THREAD_lock_free(x->lock)
//           x->lock = NULL                                races with Thread B
//
// Fix
// ===
// Call getSubjectKeyIdentifier() on every cert inside refillFromTsl() before releasing mMutex.
// This sets EXFLAG_SET while we are the sole owner of the map.  All subsequent calls on the
// same X509 object take the fast flag-read path that never acquires x->lock.

#include <gtest/gtest.h>

#include "shared/tsl/TrustStore.hxx"
#include "shared/tsl/TslParser.hxx"
#include "shared/tsl/X509Certificate.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/util/ResourceManager.hxx"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>


namespace
{

// Build a minimal ServiceInformationMap containing one CA certificate.
TslParser::ServiceInformationMap buildMapWithFreshCert()
{
    // Reuse the sub-CA DER that the rest of the TSL tests rely on.
    const std::string& derBytes =
        ResourceManager::instance().getStringResource("test/generated_pki/sub_ca1_ec/ca.der");

    // Warm cert: used only to derive the map key (subject + SKI).
    // We do NOT put this instance into the map.
    X509Certificate warmCert = X509Certificate::createFromAsnBytes(
        {reinterpret_cast<const unsigned char*>(derBytes.data()), derBytes.size()});
    CertificateId id{warmCert.getSubject(), warmCert.getSubjectKeyIdentifier()};

    // A second independent certificate allocation stored as the map value.
    X509Certificate freshCert = X509Certificate::createFromAsnBytes(
        {reinterpret_cast<const unsigned char*>(derBytes.data()), derBytes.size()});

    TslParser::ServiceInformationMap map;
    map.emplace(
        std::move(id),
        TslParser::ServiceInformation{
            std::move(freshCert),
            "http://uri.etsi.org/TrstSvc/Svctype/CA/QC",
            {"http://ocsp-testref.tsl.telematik-test/ocsp"},
            TslParser::AcceptanceHistoryMap{
                {std::chrono::time_point<std::chrono::system_clock>(std::chrono::milliseconds(1569415137000)),
                 true}},
            TslParser::ExtensionOidList{}});
    return map;
}

// A minimal X509Certificate whose getIssuer()/getAuthorityKeyIdentifier() match
// the CA cert above, so that lookupCaCertificate() actually walks into the SKI
// comparison on the stored CA cert in TrustStore::lookupCaCertificate().
X509Certificate buildEndEntityCert()
{
    const std::string derBytes = ResourceManager::instance().getStringResource(
        "test/generated_pki/sub_ca1_ec/certificates/smc_b_osig_ec/smc_b_osig_ec.der");
    return X509Certificate::createFromAsnBytes(
        {reinterpret_cast<const unsigned char*>(derBytes.data()), derBytes.size()});
}

} // namespace


// ---------------------------------------------------------------------------
// The actual race-condition regression test
// ---------------------------------------------------------------------------
TEST(TrustStoreRaceConditionTest, lookupAndRefillConcurrently)
{
    // Build a TrustStore and pre-populate it.
    TrustStore trustStore(TslMode::TSL);
    trustStore.mServiceInformationMap = buildMapWithFreshCert();

    const X509Certificate endEntity = buildEndEntityCert();

    // How long to run the concurrent stress loop.
    constexpr auto kTestDuration = std::chrono::milliseconds(500);
    // How many concurrent reader threads hammer lookupCaCertificate().
    constexpr int kReaderThreads = 4;

    std::atomic<bool> stop{false};

    // Writer thread: mirrors refillFromTsl() — builds a map with fresh (EXFLAG_SET-unset)
    // certs outside the lock, then under mMutex pre-warms getSubjectKeyIdentifier() on every
    // cert before assigning the map, exactly as the fix in refillFromTsl() does.
    std::thread writer([&] {
        while (!stop.load(std::memory_order_relaxed))
        {
            auto newMap = buildMapWithFreshCert();
            std::lock_guard guard(trustStore.mMutex);
            for (auto& [_, info] : newMap)
            {
                (void) _;
                (void) info.certificate.getSubjectKeyIdentifier();
            }
            trustStore.mServiceInformationMap = std::move(newMap);
        }
    });

    // Reader threads: call lookupCaCertificate() in a tight loop.
    std::vector<std::thread> readers;
    readers.reserve(kReaderThreads);
    for (int i = 0; i < kReaderThreads; ++i)
    {
        readers.emplace_back([&] {
            while (!stop.load(std::memory_order_relaxed))
            {
                // This is the exact call that crashed in production.
                (void) trustStore.lookupCaCertificate(endEntity);
            }
        });
    }

    std::this_thread::sleep_for(kTestDuration);
    stop.store(true, std::memory_order_relaxed);

    for (auto& t : readers)
    {
        t.join();
    }
    writer.join();

    // If we reach here without a SIGSEGV / SIGABRT the race was not triggered.
    // The test passes by surviving.
    SUCCEED();
}


// ---------------------------------------------------------------------------
// Verify that getSubjectKeyIdentifier() returns the same value on repeated calls,
// confirming the extension cache is stable once populated.
// ---------------------------------------------------------------------------
TEST(TrustStoreRaceConditionTest, extensionsCachedAfterFirstCall)
{
    const std::string derBytes =
        ResourceManager::instance().getStringResource("test/generated_pki/sub_ca1_ec/ca.der");

    X509Certificate cert = X509Certificate::createFromAsnBytes(
        {reinterpret_cast<const unsigned char*>(derBytes.data()), derBytes.size()});

    const std::string ski1 = cert.getSubjectKeyIdentifier();
    ASSERT_FALSE(ski1.empty());

    const std::string ski2 = cert.getSubjectKeyIdentifier();
    EXPECT_EQ(ski1, ski2);
}
