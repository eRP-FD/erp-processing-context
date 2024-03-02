/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_TSL_X509STORE_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_TSL_X509STORE_HXX

#include "erp/tsl/X509Certificate.hxx"

#include <memory>

class X509Store
{
public:
    /**
     * Creates an empty object.
     */
    X509Store();

    /**
     * Creates X509Store object filled with provided trusted certificates.
     */
    explicit X509Store(std::vector<X509Certificate> certificates);

    /**
     * Provides access to the trusted certificates in storage.
     */
    const std::vector<X509Certificate>& getStoredCertificates() const;

    /**
     * Provides access to X509_STORE object filled with trusted certificates
     * or null if the store is empty.
     */
    X509_STORE* getStore() const;

private:
    using X509StorePtr = std::shared_ptr<X509_STORE>;
    X509StorePtr mX509Store;
    std::vector<X509Certificate> mStoredCertificates;
};


#endif
