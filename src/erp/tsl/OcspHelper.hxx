/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_TSL_OCSPHELPER_HXX
#define ERP_TSL_OCSPHELPER_HXX

#include "erp/crypto/OpenSslHelper.hxx"
#include "erp/tsl/TslMode.hxx"

#include <memory>


class OcspHelper
{
public:
    struct Asn1SequenceDeleter
    {
        void operator() (ASN1_SEQUENCE_ANY* asn1SequenceAny);
    };

    using Asn1SequencePtr = std::unique_ptr<ASN1_SEQUENCE_ANY, Asn1SequenceDeleter>;

    static const EVP_MD* getDigestFromExtension (ASN1_SEQUENCE_ANY* extensionData);

    static const ASN1_OCTET_STRING*
    getCertHashValueFromExtension (ASN1_SEQUENCE_ANY* extensionData);

    static std::string ocspResponseToString(OCSP_RESPONSE& ocspResponse);

    static OcspResponsePtr stringToOcspResponse(const std::string& responseString);

    static std::chrono::system_clock::duration getOcspGracePeriod(TslMode tslMode);
};

#endif
