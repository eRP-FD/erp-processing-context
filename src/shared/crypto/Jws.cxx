/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/crypto/Jws.hxx"
#include "shared/util/Expect.hxx"

#include <magic_enum/magic_enum.hpp>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <utility>

#include "shared/crypto/Sha256.hxx"
#include "shared/util/Base64.hxx"
#include "fhirtools/util/Gsl.hxx"
#include "shared/util/String.hxx"


JoseHeader::JoseHeader(JoseHeader::Algorithm algorithm)
    : algorithm(algorithm)
{
    mAlgorithm.value = magic_enum::enum_name(algorithm);
}
JoseHeader& JoseHeader::setJwkSetUrl(const std::string_view& jwkSetUrl)
{
    mJwkSetUrl.value = jwkSetUrl;
    return *this;
}
JoseHeader& JoseHeader::setJsonWebKey(const std::string_view& jsonWebKey)
{
    mJsonWebKey.value = jsonWebKey;
    return *this;
}
JoseHeader& JoseHeader::setKeyId(const std::string_view& keyId)
{
    mKeyId.value = keyId;
    return *this;
}
JoseHeader& JoseHeader::setX509Url(const std::string_view& x509Url)
{
    mX509Url.value = x509Url;
    return *this;
}
JoseHeader& JoseHeader::setX509Certificate(const Certificate& x509Certificate, bool withThumbprint)
{
    for (const Certificate* cert = &x509Certificate; cert != nullptr; cert = cert->getNextCertificate())
    {
        mX509Certificate.value.push_back(cert->toBase64Der());
    }
    if (withThumbprint)
    {
        mX509CertificateSha265Thumbprint.value =
            Base64::toBase64Url(Base64::encode(Sha256::fromBin(x509Certificate.toBinaryDer())));
    }
    return *this;
}
JoseHeader& JoseHeader::setType(const MimeType& type)
{
    mType.value = type;
    return *this;
}

JoseHeader& JoseHeader::setType(const std::string& type)
{
    mType.value = type;
    return *this;
}

JoseHeader& JoseHeader::setContentType(const MimeType& contentType)
{
    mContentType.value = contentType;
    return *this;
}
JoseHeader& JoseHeader::setCritical(const std::initializer_list<std::string>& critical)
{
    mCritical.value.assign(critical);
    return *this;
}

std::string JoseHeader::serializeToBase64Url() const
{
    Expect3(mAlgorithm.value, "algorithm header value missing", std::logic_error);

    rapidjson::Document document;
    document.SetObject();

    for (const auto& field :
         {&mAlgorithm, &mJwkSetUrl, &mJsonWebKey, &mKeyId, &mX509Url, &mX509CertificateSha1Thumbprint,
          &mX509CertificateSha265Thumbprint, &mType, &mContentType})
    {
        if (field->value)
        {
            field->key.Set(document, *field->value, document.GetAllocator());
        }
    }
    for (const auto& arrayField : {&mX509Certificate, &mCritical})
    {
        if (! arrayField->value.empty())
        {
            auto& rjArray = arrayField->key.Create(document).SetArray();
            for (const auto& arrayEntry : arrayField->value)
            {
                rjArray.PushBack(
                    rapidjson::Value(arrayEntry.c_str(), gsl::narrow_cast<rapidjson::SizeType>(arrayEntry.size())),
                    document.GetAllocator());
            }
        }
    }

    rapidjson::StringBuffer buffer;
    rapidjson::Writer writer(buffer);
    document.Accept(writer);
    return Base64::toBase64Url(Base64::encode(buffer.GetString()));
}


Jws::Jws(const JoseHeader& header, std::string_view payload, const shared_EVP_PKEY& privateKey)
{
    static constexpr size_t BnSize = 32;

    mSerializedHeader = header.serializeToBase64Url();
    mSerializedPayload = Base64::toBase64Url(Base64::encode(payload));
    auto mdctx = shared_EVP_MD_CTX::make();
    OpenSslExpect(mdctx, "Failed to make EVP_MD_CTX");
    EVP_PKEY_CTX* pkeyctx = nullptr;
    switch (header.algorithm)
    {
        case JoseHeader::Algorithm::ES256:
        case JoseHeader::Algorithm::BP256R1:
            OpenSslExpect(EVP_DigestSignInit(mdctx, &pkeyctx, EVP_sha256(), nullptr, privateKey.removeConst()) == 1,
                          "Initialization of Signing Algorithm failed.");
            break;
    }
    OpenSslExpect(pkeyctx, "Failed to initialize DigestSignInit");
    OpenSslExpect(EVP_DigestSignUpdate(mdctx, mSerializedHeader.data(), mSerializedHeader.size()) == 1,
                  "Processing of JWS data failed.");
    OpenSslExpect(EVP_DigestSignUpdate(mdctx, ".", 1) == 1, "Processing of JWS data failed.");
    OpenSslExpect(EVP_DigestSignUpdate(mdctx, mSerializedPayload.data(), mSerializedPayload.size()) == 1,
                  "Processing of JWS data failed.");

    size_t siglen = 0;
    OpenSslExpect(EVP_DigestSignFinal(mdctx, nullptr, &siglen) == 1, "Failed to get signature length.");
    auto sig = std::make_unique<unsigned char[]>(siglen);
    OpenSslExpect(EVP_DigestSignFinal(mdctx, sig.get(), &siglen) == 1, "Signature calculation failed");
    const unsigned char* sigptr = sig.get();
    EcdsaSignaturePtr ecdsaSig{d2i_ECDSA_SIG(nullptr, &sigptr, static_cast<int>(siglen))};
    OpenSslExpect(ecdsaSig != nullptr, "Failed to read generated signature.");

    const BIGNUM* r;// NOLINT(cppcoreguidelines-init-variables)
    const BIGNUM* s;// NOLINT(cppcoreguidelines-init-variables)
    ECDSA_SIG_get0(ecdsaSig.get(), &r, &s);
    std::string rsBin(2 * BnSize, '\0');
    OpenSslExpect(BN_bn2binpad(r, reinterpret_cast<unsigned char*>(rsBin.data()), BnSize) == BnSize,
                  "Failed to get binary R.");
    OpenSslExpect(BN_bn2binpad(s, reinterpret_cast<unsigned char*>(rsBin.data() + BnSize), BnSize) == BnSize,
                  "Failed to get binary S.");
    mSerializedSignature = Base64::toBase64Url(Base64::encode(rsBin));
}

std::string Jws::compactSerialized() const
{
    return mSerializedHeader + '.' + mSerializedPayload + '.' + mSerializedSignature;
}

std::string Jws::compactDetachedSerialized() const
{
    return mSerializedHeader + ".." + mSerializedSignature;
}
