/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef ERP_TSL_OCSPHELPER_HXX
#define ERP_TSL_OCSPHELPER_HXX

#include "erp/crypto/OpenSslHelper.hxx"

#include <memory>


class OcspHelper
{
public:
    struct Asn1SequenceDeleter
    {
        void operator() (ASN1_SEQUENCE_ANY* asn1SequenceAny);
    };

    static constexpr int DEFAULT_OCSP_GRACEPERIOD_SECONDS = 600;

    using Asn1SequencePtr = std::unique_ptr<ASN1_SEQUENCE_ANY, Asn1SequenceDeleter>;

    static const EVP_MD* getDigestFromExtension (ASN1_SEQUENCE_ANY* extensionData);

    static const ASN1_OCTET_STRING*
    getCertHashValueFromExtension (ASN1_SEQUENCE_ANY* extensionData);

    static std::string ocspResponseToString(OCSP_RESPONSE& ocspResponse);

    static OcspResponsePtr stringToOcspResponse(const std::string& responseString);
};

#endif
