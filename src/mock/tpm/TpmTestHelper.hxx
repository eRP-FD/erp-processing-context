/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_TPMTESTHELPER_HXX
#define ERP_PROCESSING_CONTEXT_TPMTESTHELPER_HXX

#include "shared/tpm/Tpm.hxx"

#include <functional>
#include <memory>


#if WITH_HSM_MOCK != 1
#error TpmTestHelper.hxx included but WITH_HSM_MOCK not enabled
#endif
/**
 * The setup for TPM tests can no longer rely solely on static data. For the production TPM that communicates
 * with a simulator or later with a hardware TPM, we have to provide "fresh" data that is generated by the TPM.
 * Therefore we need different implementations that provides test data for mock TPM and production TPM.
 */
class TpmTestHelper
{
public:
    /**
     * Return true only if
     * - the platform suppports the tpm client library
     * - the MOCK_USE_MOCK_TPM configuration value is set to a negative value.
     */
    static bool isProductionTpmSupportedAndConfigured (void);


    /**
     * Create the production TPM if supported by the platform and not explicitly configured out.
     * If that is not possible return a mock TPM instance.
     */
    static Tpm::Factory createTpmFactory (void);

    static Tpm::Factory createMockTpmFactory (void);
    static Tpm::Factory createProductionTpmFactory (void);

    static std::unique_ptr<Tpm> createMockTpm (void);
    static std::unique_ptr<Tpm> createProductionTpm (BlobCache& blobCache);

    /**
     * Always returns a TpmTestHelper object that can be used with the mock TPM.
     */
    static std::unique_ptr<TpmTestHelper> createTestHelperForMock (void);

    /**
     * Returns a TpmTestHelper object that provides test data suitable for a production TPM if `isProductionTpmSupportedAndConfigured`
     * returns `true`.
     * Otherwise returns an empty pointer.
     */
    static std::unique_ptr<TpmTestHelper> createTestHelperForProduction (void);

    virtual ~TpmTestHelper (void) = default;

    virtual Tpm::AuthenticateCredentialInput getAuthenticateCredentialInput (BlobCache& blobCache) = 0;
};


#endif
