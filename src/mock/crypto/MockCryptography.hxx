#ifndef ERP_PROCESSING_CONTEXT_MOCKCRYPTOGRAPHY_HXX
#define ERP_PROCESSING_CONTEXT_MOCKCRYPTOGRAPHY_HXX

#include "erp/crypto/OpenSslHelper.hxx"

#if WITH_HSM_MOCK  != 1
#error MockCryptography.hxx included but WITH_HSM_MOCK not enabled
#endif

class Certificate;
class SafeString;


/**
 * Provide mock values for certain cryptographic values (private or public keys, certificates) that are not all directly
 * accessible in production but are required for tests so that e.g. JWTs can be created and signed or VAU
 * requests can be encrypted.
 *
 * Note that some of the values, like the IDP public key are accessible in production but some tests require access
 * without having access to the PcProcessingContext. Therefore, when you can access a value in a way that is valid in
 * production then do so. Otherwise use this class.
 */
class MockCryptography
{
public:
    static shared_EVP_PKEY getIdpPublicKey (void);
    static const Certificate& getIdpPublicKeyCertificate (void);
    static shared_EVP_PKEY getIdpPrivateKey (void);

    static shared_EVP_PKEY getEciesPublicKey (void);
    static const SafeString& getEciesPublicKeyPem (void);
    static const Certificate& getEciesPublicKeyCertificate (void);
    static const SafeString& getEciesPrivateKeyPem (void);
    static shared_EVP_PKEY getEciesPrivateKey (void);

    static const SafeString& getIdFdSigPrivateKeyPem (void);
    static const SafeString& getIdFdSigPrivateKeyPkcs8 (void);
};


#endif
