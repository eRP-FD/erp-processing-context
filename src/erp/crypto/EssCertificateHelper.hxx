/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_ERP_CRYPTO_ESSCERTIFICATEHELPER_HXX
#define ERP_PROCESSING_CONTEXT_SRC_ERP_CRYPTO_ESSCERTIFICATEHELPER_HXX

#include "erp/crypto/OpenSslHelper.hxx"

/**
 * This helper wraps OpenSsl based implementation inserting/retrieving a signing certificate into/from signed data
 * of CMS signer info.
 */
class EssCertificateHelper
{
public:
    static void setSigningCertificateInSignedData(CMS_SignerInfo& signerInfo,
                                                  X509& signingCertificate);

    static void verifySigningCertificateFromSignedData(CMS_SignerInfo& signerInfo,
                                                       X509& signingCertificate);
};


#endif
