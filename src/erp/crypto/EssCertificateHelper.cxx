/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/crypto/EssCertificateHelper.hxx"

#include "erp/util/Gsl.hxx"

#include <functional>

/**
 * The following OpenSsl based implementation
 *
 * implements SigningCertificate and SigningCertificateV2 attributes according to
 * https://tools.ietf.org/html/rfc2634
 * https://tools.ietf.org/html/rfc5035
 *
 * and is inspired by the implementation in
 * https://github.com/openssl/openssl/pull/7893/files
 *
 * It has to be implemented explicitly ourself because the related OpenSsl implementation
 * will only be available for OpenSsl 3.0 that is still not released.
 *
 * After the main implementation is switched to OpenSsl 3.0 this implementation can be removed
 * and replaced by usage of CMS_sign()/CMS_verify() with CMS_CADES flag set.
 */

class EssIssuerSerial
{
public:
    STACK_OF(GENERAL_NAME)* issuer;
    ASN1_INTEGER* serial;
};
using EssIssuerSerialPtr = std::unique_ptr<EssIssuerSerial, std::function<void(EssIssuerSerial*)>>;
DEFINE_STACK_OF(EssIssuerSerial)
ASN1_SEQUENCE(EssIssuerSerial) = {
    ASN1_SEQUENCE_OF(EssIssuerSerial, issuer, GENERAL_NAME),
    ASN1_SIMPLE(EssIssuerSerial, serial, ASN1_INTEGER)
} ASN1_SEQUENCE_END(EssIssuerSerial)
IMPLEMENT_ASN1_FUNCTIONS(EssIssuerSerial)


class EssCertificateId
{
public:
    ASN1_OCTET_STRING* hash;
    EssIssuerSerial* essIssuerSerial;
};
using EssCertificateIdPtr = std::unique_ptr<EssCertificateId, std::function<void(EssCertificateId*)>>;
DEFINE_STACK_OF(EssCertificateId)
ASN1_SEQUENCE(EssCertificateId) = {
    ASN1_SIMPLE(EssCertificateId, hash, ASN1_OCTET_STRING),
    ASN1_OPT(EssCertificateId, essIssuerSerial, EssIssuerSerial)
} ASN1_SEQUENCE_END(EssCertificateId)
IMPLEMENT_ASN1_FUNCTIONS(EssCertificateId)


class EssSigningCertificate
{
public:
    STACK_OF(EssCertificateId)* essCertificateIds;
    STACK_OF(POLICYINFO)* policyInfos;
};
using EssSigningCertificatePtr = std::unique_ptr<EssSigningCertificate, std::function<void(EssSigningCertificate*)>>;
DEFINE_STACK_OF(EssSigningCertificate)
ASN1_SEQUENCE(EssSigningCertificate) = {
    ASN1_SEQUENCE_OF(EssSigningCertificate, essCertificateIds, EssCertificateId),
    ASN1_SEQUENCE_OF_OPT(EssSigningCertificate, policyInfos, POLICYINFO)
} ASN1_SEQUENCE_END(EssSigningCertificate)
IMPLEMENT_ASN1_FUNCTIONS(EssSigningCertificate)


struct EssCertificateIdV2 {
    X509_ALGOR* hashAlgorithm;
    ASN1_OCTET_STRING* hash;
    EssIssuerSerial* essIssuerSerial;
};
using EssCertificateIdV2Ptr = std::unique_ptr<EssCertificateIdV2, std::function<void(EssCertificateIdV2*)>>;
DEFINE_STACK_OF(EssCertificateIdV2)
ASN1_SEQUENCE(EssCertificateIdV2) = {
    ASN1_OPT(EssCertificateIdV2, hashAlgorithm, X509_ALGOR),
    ASN1_SIMPLE(EssCertificateIdV2, hash, ASN1_OCTET_STRING),
    ASN1_OPT(EssCertificateIdV2, essIssuerSerial, EssIssuerSerial)
} ASN1_SEQUENCE_END(EssCertificateIdV2)
IMPLEMENT_ASN1_FUNCTIONS(EssCertificateIdV2)


struct EssSigningCertificateV2 {
    STACK_OF(EssCertificateIdV2)* essCertificateIds;
    STACK_OF(POLICYINFO)* policyInfos;
};
using EssSigningCertificateV2Ptr = std::unique_ptr<EssSigningCertificateV2, std::function<void(EssSigningCertificateV2*)>>;
DEFINE_STACK_OF(EssSigningCertificateV2)
ASN1_SEQUENCE(EssSigningCertificateV2) = {
    ASN1_SEQUENCE_OF(EssSigningCertificateV2, essCertificateIds, EssCertificateIdV2),
    ASN1_SEQUENCE_OF_OPT(EssSigningCertificateV2, policyInfos, POLICYINFO)
} ASN1_SEQUENCE_END(EssSigningCertificateV2)
IMPLEMENT_ASN1_FUNCTIONS(EssSigningCertificateV2)


namespace
{
    std::vector<unsigned char> calculateCertificateHash(X509& certificate, const EVP_MD& hashAlgorithm)
    {
        std::vector<unsigned char> hash(EVP_MAX_MD_SIZE, '\0');
        unsigned int hashSize = gsl::narrow_cast<unsigned int>(hash.size());
        OpenSslExpect(1 == X509_digest(&certificate, &hashAlgorithm, hash.data(), &hashSize),
                      "Can not create digest");
        hash.resize(hashSize);

        return hash;
    }


    template <class CertificateId>
    void setCertificateIdHash(CertificateId& certificateId, X509& certificate, const EVP_MD& hashAlgorithm)
    {
        std::vector<unsigned char> hash = calculateCertificateHash(certificate, hashAlgorithm);
        OpenSslExpect(1 == ASN1_OCTET_STRING_set(certificateId.hash, hash.data(), gsl::narrow_cast<int>(hash.size())),
                      "Can not set hash");
    }


    template <class CertificateId>
    void setCertificateIdIssuerSerial(CertificateId& certificateId, X509& certificate)
    {
        certificateId.essIssuerSerial = EssIssuerSerial_new();
        OpenSslExpect(certificateId.essIssuerSerial != nullptr, "Can not allocate issuer serial");

        std::unique_ptr<GENERAL_NAME, std::function<void(GENERAL_NAME*)>> generalName(
            GENERAL_NAME_new(), GENERAL_NAME_free);
        OpenSslExpect(generalName != nullptr, "Can not allocate GENERAL_NAME");
        generalName->type = GEN_DIRNAME;
        generalName->d.dirn = X509_NAME_dup(X509_get_issuer_name(&certificate));
        OpenSslExpect(generalName->d.dirn != nullptr, "Can not duplicate certificate issuer name");
        OpenSslExpect(1 == sk_GENERAL_NAME_push(certificateId.essIssuerSerial->issuer, generalName.get()),
                      "Can not set general name to certificate id");
        generalName.release(); // NOLINT(bugprone-unused-return-value)

        ASN1_INTEGER_free(certificateId.essIssuerSerial->serial);
        certificateId.essIssuerSerial->serial = ASN1_INTEGER_dup(X509_get_serialNumber(&certificate));
        OpenSslExpect(certificateId.essIssuerSerial->serial != nullptr, "Can not duplicate certificate serial number");
    }


    EssCertificateIdPtr createEssCertificateId(
        X509& certificate)
    {
        EssCertificateIdPtr certificateId(EssCertificateId_new(), EssCertificateId_free);
        OpenSslExpect(certificateId != nullptr, "Can not allocate EssCertificateId.");

        setCertificateIdHash(*certificateId, certificate, *EVP_sha1());

        setCertificateIdIssuerSerial(*certificateId, certificate);

        return certificateId;
    }


    EssSigningCertificatePtr createEssSigningCertificate(
        X509& certificate)
    {
        EssSigningCertificatePtr essCertificate(EssSigningCertificate_new(), EssSigningCertificate_free);
        OpenSslExpect(essCertificate != nullptr, "Can not create EssSigningCertificate");

        EssCertificateIdPtr certificateId = createEssCertificateId(certificate);
        OpenSslExpect(sk_EssCertificateId_push(essCertificate->essCertificateIds, certificateId.get()),
                      "Can not push certificate id to EssSigningCertificate");
        certificateId.release(); // NOLINT(bugprone-unused-return-value)

        return essCertificate;
    }


    void addSigningCert(CMS_SignerInfo &signerInfo, X509& signingCert)
    {
        EssSigningCertificatePtr essCertificate = createEssSigningCertificate(signingCert);
        OpenSslExpect(essCertificate != nullptr, "Can not create EssSigningCertificateV2");

        int length = i2d_EssSigningCertificate(essCertificate.get(), nullptr);
        OpenSslExpect(length > 0,
                      "Failed to get certificate length.");
        std::vector<unsigned char> certificateBytes(gsl::narrow<size_t>(length), 0u);
        unsigned char* rawptr = certificateBytes.data();
        OpenSslExpect(i2d_EssSigningCertificate(essCertificate.get(), &rawptr) == length,
                      "Failed call i2d_EssSigningCertificate()");

        std::unique_ptr<ASN1_STRING, std::function<void(ASN1_STRING*)>> certificateString(
            ASN1_STRING_new(), ASN1_STRING_free);
        OpenSslExpect(certificateString != nullptr, "Can not create ASN1_STRING");
        OpenSslExpect(1 == ASN1_STRING_set(certificateString.get(), certificateBytes.data(), length),
                      "Can not set ASN1_STRING");

        OpenSslExpect(CMS_signed_add1_attr_by_NID(
                          &signerInfo,
                          NID_id_smime_aa_signingCertificate,
                          V_ASN1_SEQUENCE,
                          certificateString.get(),
                          -1),
                      "Can not add certificate to signed data");
    }


    EssCertificateIdV2Ptr createEssCertificateIdV2(
        X509& certificate,
        const EVP_MD& hashAlgorithm)
    {
        EssCertificateIdV2Ptr certificateId(EssCertificateIdV2_new(), EssCertificateIdV2_free);
        OpenSslExpect(certificateId != nullptr, "Can not allocate EssCertificateIdV2.");

        if (&hashAlgorithm == EVP_sha256())
        {
            certificateId->hashAlgorithm = nullptr;
        }
        else
        {
            std::unique_ptr<X509_ALGOR, std::function<void(X509_ALGOR*)>> algorithm(X509_ALGOR_new(), X509_ALGOR_free);
            OpenSslExpect(algorithm != nullptr, "Can not allocate X509_ALGOR");
            X509_ALGOR_set_md(algorithm.get(), &hashAlgorithm);
            OpenSslExpect(algorithm->algorithm != nullptr, "Can not set algorithm");
            certificateId->hashAlgorithm = algorithm.release();
        }

        setCertificateIdHash(*certificateId, certificate, hashAlgorithm);

        setCertificateIdIssuerSerial(*certificateId, certificate);

        return certificateId;
    }


    EssSigningCertificateV2Ptr createEssSigningCertificateV2(
        X509& certificate,
        const EVP_MD& hashAlgorithm)
    {
        EssSigningCertificateV2Ptr essCertificate(EssSigningCertificateV2_new(), EssSigningCertificateV2_free);
        OpenSslExpect(essCertificate != nullptr, "Can not create EssSigningCertificateV2");

        EssCertificateIdV2Ptr certificateId = createEssCertificateIdV2(certificate, hashAlgorithm);
        OpenSslExpect(sk_EssCertificateIdV2_push(essCertificate->essCertificateIds, certificateId.get()),
                      "Can not push certificate id to EssSigningCertificateV2");
        certificateId.release(); // NOLINT(bugprone-unused-return-value)

        return essCertificate;
    }


    void addSigningCertV2(CMS_SignerInfo &signerInfo,
                          X509& signingCert,
                          const EVP_MD& hashAlgorithm)
    {
        EssSigningCertificateV2Ptr essCertificate = createEssSigningCertificateV2(signingCert, hashAlgorithm);
        OpenSslExpect(essCertificate != nullptr, "Can not create EssSigningCertificateV2");

        int length = i2d_EssSigningCertificateV2(essCertificate.get(), nullptr);
        OpenSslExpect(length > 0,
                      "Failed to get certificate length.");
        std::vector<unsigned char> certificateBytes(gsl::narrow<size_t>(length), 0u);
        unsigned char* rawptr = certificateBytes.data();
        OpenSslExpect(i2d_EssSigningCertificateV2(essCertificate.get(), &rawptr) == length,
                      "Failed call i2d_EssSigningCertificateV2()");

        std::unique_ptr<ASN1_STRING, std::function<void(ASN1_STRING*)>> certificateString(
            ASN1_STRING_new(), ASN1_STRING_free);
        OpenSslExpect(certificateString != nullptr, "Can not create ASN1_STRING");
        OpenSslExpect(1 == ASN1_STRING_set(certificateString.get(), certificateBytes.data(), length),
                      "Can not set ASN1_STRING");

        OpenSslExpect(CMS_signed_add1_attr_by_NID(
                          &signerInfo,
                          NID_id_smime_aa_signingCertificateV2,
                          V_ASN1_SEQUENCE,
                          certificateString.get(),
                          -1),
                      "Can not add certificate to signed data");
    }


    ASN1_STRING* getAttributeAsn1String(CMS_SignerInfo& signerInfo, const int nid)
    {
        ASN1_OBJECT* object = OBJ_nid2obj(nid);
        OpenSslExpect(object != nullptr, "Can not create SigningCertificate attribute object.");

        return reinterpret_cast<ASN1_STRING*>(
            CMS_signed_get0_data_by_OBJ(&signerInfo, object, -1, V_ASN1_SEQUENCE));
    }


    const EVP_MD* getHashAlgorithm(EssCertificateId&)
    {
        return EVP_sha1();
    }


    const EVP_MD* getHashAlgorithm(EssCertificateIdV2& certificateId)
    {
        if (certificateId.hashAlgorithm == nullptr)
        {
            return EVP_sha256();
        }

        return EVP_get_digestbyobj(certificateId.hashAlgorithm->algorithm);
    }


    template<class SigningCertificate, class CertificateId, class CertificateIdStack>
    void verifySigningCertificateAttributeContainsHash(
        SigningCertificate& essSigningCertificate,
        X509& signingCertificate,
        std::function<int(CertificateIdStack*)> getNum,
        std::function<CertificateId*(CertificateIdStack*, int)> getValue)
    {
        const int idCount = getNum(essSigningCertificate.essCertificateIds);
        OpenSslExpect(idCount > 0, "At least one certificate is expected in the signed data.");
        for (int ind = 0; ind < idCount; ind++)
        {
            CertificateId* certificateId = getValue(essSigningCertificate.essCertificateIds, ind);
            OpenSslExpect(certificateId != nullptr, "Can not get certificate id.");

            const EVP_MD* hashAlgorithm = getHashAlgorithm(*certificateId);
            OpenSslExpect(hashAlgorithm != nullptr, "Can not get hash algorithm.");

            std::vector<unsigned char> hash = calculateCertificateHash(signingCertificate, *hashAlgorithm);
            std::unique_ptr<ASN1_OCTET_STRING, std::function<void(ASN1_OCTET_STRING*)>> expectedHash(
                ASN1_OCTET_STRING_new(), ASN1_OCTET_STRING_free);
            OpenSslExpect(expectedHash != nullptr, "Can not create ASN1_OCTET_STRING");
            OpenSslExpect(1 == ASN1_OCTET_STRING_set(expectedHash.get(), hash.data(), gsl::narrow_cast<int>(hash.size())),
                          "Can not set ASN1_OCTET_STRING");

            OpenSslExpect(certificateId->hash != nullptr, "Can not get certificate id hash.");
            if (ASN1_OCTET_STRING_cmp(expectedHash.get(), certificateId->hash) == 0)
            {
                return;
            }
        }

        Fail("The CMS signing certificate hash comparing with signed attributes failed.");
    }


    void verifySigningCertificate(
        const ASN1_STRING& signingCertificateAttributeString,
        X509& signingCertificate)
    {
        EssSigningCertificatePtr essSigningCertificate(
            reinterpret_cast<EssSigningCertificate*>(
                ASN1_item_unpack(&signingCertificateAttributeString, ASN1_ITEM_rptr(EssSigningCertificate))),
            EssSigningCertificate_free);
        OpenSslExpect(essSigningCertificate != nullptr, "Can not get signing certificate from ASN1 string.");

        verifySigningCertificateAttributeContainsHash
            <EssSigningCertificate, EssCertificateId, STACK_OF(EssCertificateId)>
            (*essSigningCertificate, signingCertificate, sk_EssCertificateId_num, sk_EssCertificateId_value);
    }


    void verifySigningCertificateV2(
        const ASN1_STRING& signingCertificateAttributeString,
        X509& signingCertificate)
    {
        EssSigningCertificateV2Ptr essSigningCertificateV2(
            reinterpret_cast<EssSigningCertificateV2*>(
                ASN1_item_unpack(&signingCertificateAttributeString, ASN1_ITEM_rptr(EssSigningCertificateV2))),
            EssSigningCertificateV2_free);
        OpenSslExpect(essSigningCertificateV2 != nullptr, "Can not get signing certificate from ASN1 string.");

        verifySigningCertificateAttributeContainsHash
            <EssSigningCertificateV2, EssCertificateIdV2, STACK_OF(EssCertificateIdV2)>
            (*essSigningCertificateV2, signingCertificate, sk_EssCertificateIdV2_num, sk_EssCertificateIdV2_value);
    }
}


void EssCertificateHelper::setSigningCertificateInSignedData(
    CMS_SignerInfo& signerInfo,
    X509& signingCertificate)
{
    X509_ALGOR* algorithm = nullptr;
    CMS_SignerInfo_get0_algs(&signerInfo, nullptr, nullptr, &algorithm, nullptr);
    int nid = OBJ_obj2nid(algorithm->algorithm);
    OpenSslExpect(algorithm != nullptr, "Can not get digest algorithm from signed info.");
    if (nid == EVP_MD_nid(EVP_sha1()))
    {
        addSigningCert(signerInfo, signingCertificate);
    }
    else
    {
        const EVP_MD* digest = EVP_get_digestbynid(nid);
        OpenSslExpect(digest != nullptr, "Can not get digest from NID");
        addSigningCertV2(signerInfo, signingCertificate, *digest);
    }
}


void EssCertificateHelper::verifySigningCertificateFromSignedData(
    CMS_SignerInfo& signerInfo,
    X509& signingCertificate)
{
    ASN1_STRING* signingCertificateAttributeString =
        getAttributeAsn1String(signerInfo, NID_id_smime_aa_signingCertificate);
    ASN1_STRING* signingCertificateV2AttributeString =
        getAttributeAsn1String(signerInfo, NID_id_smime_aa_signingCertificateV2);
    OpenSslExpect(signingCertificateAttributeString != nullptr || signingCertificateV2AttributeString != nullptr,
                  "No signing certificate attribute in signed data.");

    if (signingCertificateAttributeString != nullptr)
    {
        verifySigningCertificate(*signingCertificateAttributeString, signingCertificate);
    }
    if (signingCertificateV2AttributeString != nullptr)
    {
        verifySigningCertificateV2(*signingCertificateV2AttributeString, signingCertificate);
    }
}
