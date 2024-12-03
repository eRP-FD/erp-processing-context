/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2024
 *  (C) Copyright IBM Corp. 2021, 2024
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/Signature.hxx"
#include "library/util/Assert.hxx"

#include "fhirtools/util/Gsl.hxx"
#include "shared/util/Base64.hxx"

namespace epa
{

namespace
{
    /**
     * This helper class allows parsing and creating two formats of elliptic curve signatures. In its core each
     * signature carries two 32 byte numbers, r and s.
     * The first, simpler form is the one directly provided by OpenSSL's EVP_DigestSignFinal function:
     *
     * SEQUENCE {
     *     INTEGER  // r
     *     INTEGER  // s
     * }
     *
     * The second form is the one defined by gematik. See clarification by Gematik https://dth01.ibmgcloud.net/jira/browse/EPA-10744
     * or in gemSpec_Krypt, section 5.2:
     * UPDATE: apparently this update was either wrong, misunderstood or Gematik changed their mind. Either way
     *         the following is no long actively used and will be passivly supported (read but not written) for a
     *         short time.
     *
     * SEQUENCE {
     *     OBJECT IDENTIFIER // ecdsaWithSHA256 (1 2 840 10045 4 3 2)
     * }
     * BIT STRING, encapsulates {
     *     SEQUENCE {
     *         INTEGER  // r
     *         INTEGER  // s
     *     }
     * }
     */
    class Asn1Helper
    {
    public:
        /**
         * Return either EC_SIMPLE, EC_GEMATIK or UNKNOWN. If UNKNOWN, it can still be RSA but that is not detected
         * by this function.
         */
        static Signature::Format detectEcForm(const BinaryView& data)
        {
            // See parse and parseGematikForm for a more verbose description of the individual steps.

            const auto* p = data.data();
            const auto* end = p + data.size();

            // The first tag has to be a sequence.
            long int objectLength = 0;
            int derTag = 0;
            int derClass = 0;
            const auto* p2 = p;
            const int result =
                ASN1_get_object(&p2, &objectLength, &derTag, &derClass, std::distance(p2, end));
            if ((result & 0x80) || derTag != V_ASN1_SEQUENCE)
                return Signature::Format::Unknown;

            // From this sequence, extract either the oid (gematik form) or integer sequence (r and s, simple form).
            shared_ASN1_SEQUENCE_ANY sequence;
            const auto* sequenceResult =
                d2i_ASN1_SEQUENCE_ANY(sequence.getP(), &p, std::distance(p, end));
            if (sequenceResult == nullptr)
                return Signature::Format::Unknown;
            else
                switch (sk_ASN1_TYPE_num(sequence))
                {
                    case 1:
                    {
                        // Sequence is expected to contain a single OID and the signature in Gematik
                        // form.
                        return detectEcFormFromSequenceWithOid(sequence);
                    }
                    case 2:
                    {
                        // Sequence is expected to contain two integers and the signature in Simple
                        // form.
                        return detectEcFormFromSequenceWithRAndS(sequence);
                    }
                    default:
                        // Unexpected.
                        return Signature::Format::Unknown;
                }
        }

        static Signature::Format detectEcFormFromSequenceWithOid(
            const shared_ASN1_SEQUENCE_ANY& sequence)
        {
            // Verify the oid
            const auto* oidAny = sk_ASN1_TYPE_value(sequence, 0);
            if (oidAny == nullptr || oidAny->type != V_ASN1_OBJECT)
                return Signature::Format::Unknown;
            const auto* oid = oidAny->value.object;
            if (oid == nullptr || OBJ_obj2nid(oid) != NID_ecdsa_with_SHA256)
                return Signature::Format::Unknown;
            else
                return Signature::Format::EC_GEMATIK;
        }

        static Signature::Format detectEcFormFromSequenceWithRAndS(
            const shared_ASN1_SEQUENCE_ANY& sequence)
        {
            // Extract r and s.
            const auto* rAny = sk_ASN1_TYPE_value(sequence, 0);
            Assert(rAny != nullptr) << "can not get first sequence element";
            const auto* sAny = sk_ASN1_TYPE_value(sequence, 1);
            Assert(rAny != nullptr) << "can not get second sequence element";

            if (rAny->type == V_ASN1_INTEGER && sAny->type == V_ASN1_INTEGER)
                return Signature::Format::EC_SIMPLE;
            else
                return Signature::Format::Unknown;
        }

        static std::tuple<shared_BN, shared_BN> parse(const BinaryView& data)
        {
            const auto* p = data.data();
            const auto* end = p + data.size();
            const auto* p2 = expect(V_ASN1_SEQUENCE, p, end);
            if (isTag(V_ASN1_INTEGER, p2, end))
                return parseSimpleForm(p, end);
            else
                return parseGematikForm(p, end);
        }

        static std::tuple<shared_BN, shared_BN> parseSimpleForm(
            const unsigned char* p,
            const unsigned char* end)
        {
            // Unpack the r/s sequence.
            shared_ASN1_SEQUENCE_ANY rsSequence;
            Assert(rsSequence == nullptr) << "sequence not correctly initialized";
            const auto* rsResult =
                d2i_ASN1_SEQUENCE_ANY(rsSequence.getP(), &p, std::distance(p, end));
            Assert(rsResult != nullptr) << "simple form does not contain a sequence of integers";
            Assert(sk_ASN1_TYPE_num(rsSequence) == 2)
                << "expected two integers (r and s) in sequence but got "
                << std::to_string(sk_ASN1_TYPE_num(rsSequence));

            // Extract r and s.
            const auto* rAny = sk_ASN1_TYPE_value(rsSequence, 0);
            const auto* sAny = sk_ASN1_TYPE_value(rsSequence, 1);
            return std::make_tuple(extractInteger(rAny), extractInteger(sAny));
        }

        static std::tuple<shared_BN, shared_BN> parseGematikForm(
            const unsigned char* p,
            const unsigned char* end)
        {
            // Extract the oid sequence
            shared_ASN1_SEQUENCE_ANY oidSequence;
            Assert(oidSequence == nullptr) << "sequence not correctly initialized";
            const auto* oidResult =
                d2i_ASN1_SEQUENCE_ANY(oidSequence.getP(), &p, std::distance(p, end));
            Assert(oidResult != nullptr) << "gematik form does not contain a sequence of oids";
            Assert(sk_ASN1_TYPE_num(oidSequence) == 1)
                << "expected one oid but got " << std::to_string(sk_ASN1_TYPE_num(oidSequence));

            // Verify the oid
            const auto* oidAny = sk_ASN1_TYPE_value(oidSequence, 0);
            Assert(oidAny != nullptr) << "can not extract oid";
            Assert(oidAny->type == V_ASN1_OBJECT) << "oid candidate has wrong type";
            const auto* oid = oidAny->value.object;
            Assert(oid != nullptr) << "invalid oid";
            Assert(OBJ_obj2nid(oid) == NID_ecdsa_with_SHA256)
                << "oid has wrong nid, expected " << std::to_string(NID_ecdsa_with_SHA256)
                << " (NID_ecdsa_with_SHA256) but got "
                << logging::sensitive(std::to_string(OBJ_obj2nid(oid)));

            // Unpack the r/s sequence from its enclosing bit string.
            shared_ASN1_BIT_STRING data;
            Assert(data == nullptr) << "bit string not correctly initialized";
            auto* rsOuterResult = d2i_ASN1_BIT_STRING(data.getP(), &p, std::distance(p, end));
            Assert(rsOuterResult != nullptr) << "bit string (which contains r and s) is missing";

            // Its content is identical to the simple form.
            return parseSimpleForm(data->data, data->data + data->length);
        }


        static const unsigned char* expect(
            const int expectedTag,
            const unsigned char* p,
            const unsigned char* end)
        {
            const auto [p2, objectLength, derTag] = parseObject(p, end);

            (void) objectLength;

            if (derTag != expectedTag)
                throw std::runtime_error(
                    "got wrong tag " + std::to_string(derTag) + " in EC signature, expected "
                    + std::to_string(expectedTag));
            return p2;
        }

        static std::tuple<const unsigned char*, long int, int> parseObject(
            const unsigned char* p,
            const unsigned char* end)
        {
            Assert(p != nullptr) << "start pointer must not be null";
            Assert(end != nullptr) << "end pointer must not be null";

            long int objectLength = 0;
            int derTag = 0;
            int derClass = 0;
            const int result =
                ASN1_get_object(&p, &objectLength, &derTag, &derClass, std::distance(p, end));
            if (result & 0x80)
                throw std::runtime_error("error while extracting ASN1 object");

            return std::make_tuple(p, objectLength, derTag);
        }

        static bool isTag(const int expectedTag, const unsigned char* p, const unsigned char* end)
        {
            const auto [p2, objectLength, derTag] = parseObject(p, end);
            (void) p2;
            (void) objectLength;

            return derTag == expectedTag;
        }

        static shared_BN extractInteger(const ASN1_TYPE* value)
        {
            Assert(value != nullptr) << "value is missing";
            Assert(value->type == V_ASN1_INTEGER) << "value is not an integer";
            const auto* integer = value->value.integer;
            Assert(integer != nullptr) << "integer value is missing";
            auto bn = shared_BN::make(ASN1_INTEGER_to_BN(integer, nullptr));
            Assert(bn.isSet()) << "integer conversion to BIGNUM failed";
            return bn;
        }

        static void addToSequence(
            shared_ASN1_SEQUENCE_ANY& sequence,
            const void* element,
            const int type)
        {
            Assert(sequence.isSet()) << "sequence must be set";
            Assert(element != nullptr) << "element must not be null";

            auto t = shared_ASN1_TYPE::make();
            int result = ASN1_TYPE_set1(t, type, element);
            Assert(result == 1) << "ASN1_TYPE not set correctly";
            result =
                sk_ASN1_TYPE_push(sequence, t.release()); // Ownership of t is passed to sequence.
            Expects(result > 0);
        }

        static BinaryBuffer toBufferInSimpleForm(const shared_BN& bnR, const shared_BN& bnS)
        {
            // Convert r and s to ASN1 integer objects. These have to live (a little) longer then the sequence they
            // will be added to.
            auto r = shared_ASN1_INTEGER::make(BN_to_ASN1_INTEGER(bnR, nullptr));
            auto s = shared_ASN1_INTEGER::make(BN_to_ASN1_INTEGER(bnS, nullptr));

            // Create one sequence that contains the integers r and s.
            shared_ASN1_SEQUENCE_ANY sequence = shared_ASN1_SEQUENCE_ANY::make();

            addToSequence(sequence, r, V_ASN1_INTEGER);
            addToSequence(sequence, s, V_ASN1_INTEGER);

            unsigned char* buffer = nullptr;
            const int length = i2d_ASN1_SEQUENCE_ANY(sequence, &buffer);
            sequence.reset();
            BinaryBuffer result(buffer, gsl::narrow<size_t>(length));
            OPENSSL_free(buffer);

            return result;
        }

        static BinaryBuffer toBufferInGematikForm(const shared_BN& r, const shared_BN& s)
        {
            // Create one sequence that contains the ecdsaWithSHA256 oid.
            shared_ASN1_SEQUENCE_ANY sequenceA = shared_ASN1_SEQUENCE_ANY::make();
            shared_ASN1_OBJECT oid = shared_ASN1_OBJECT::make(
                OBJ_nid2obj(NID_ecdsa_with_SHA256)); //"1 2 840 10045 4 3 2", 1);
            Assert(oid != nullptr) << "creation of oid failed";
            addToSequence(sequenceA, oid, V_ASN1_OBJECT);

            // Create a bit string that contains the simple form.
            shared_ASN1_BIT_STRING bitString = shared_ASN1_BIT_STRING::make();
            BinaryBuffer simpleForm = toBufferInSimpleForm(r, s);
            ASN1_BIT_STRING_set(bitString, simpleForm.data(), simpleForm.sizeAsInt());
            // Preserve trailing zero bits.
            bitString->flags |= ASN1_STRING_FLAG_BITS_LEFT;

            // Serialize both sequences ...
            unsigned char* bufferA = nullptr;
            const int lengthA = i2d_ASN1_SEQUENCE_ANY(sequenceA, &bufferA);
            unsigned char* bufferB = nullptr;
            int lengthB = i2d_ASN1_BIT_STRING(bitString, &bufferB);

            // ... and join the results.
            BinaryBuffer result(gsl::narrow<size_t>(lengthA + lengthB));
            std::copy(bufferA, bufferA + lengthA, result.data());
            std::copy(bufferB, bufferB + lengthB, result.data() + lengthA);

            OPENSSL_free(bufferA);
            OPENSSL_free(bufferB);

            return result;
        }
    };

    BinaryBuffer bnToBinary(
        const shared_BN& bn,
        const size_t signatureSizeBits,
        const std::string& name)
    {
        const size_t minimalSize = static_cast<size_t>(BN_num_bytes(bn));
        const size_t expectedSize = signatureSizeBits / 8 / 2;
        Assert(expectedSize >= minimalSize) << "BIGNUM " << name << " is too big";

        // in case "big number" is small let it be padded with 0 at the beginning
        BinaryBuffer binary(expectedSize);
        const size_t binaryLength =
            gsl::narrow<size_t>(BN_bn2binpad(bn, binary.data(), binary.sizeAsInt()));
        AssertOpenSsl(binary.size() == binaryLength)
            << "conversion of BIGNUM " << name << " to binary failed";
        return binary;
    }

    shared_BN binaryToBN(const BinaryView& binary, const std::string& name)
    {
        auto bn = shared_BN::make();
        auto* result = BN_bin2bn(binary.data(), sizeAsInt(binary), bn);
        AssertOpenSsl(result == bn.get()) << "conversion of " << name << " failed";

        return bn;
    }
}


std::unique_ptr<Signature> Signature::from(const BinaryView& data)
{
    switch (detectFormat(data))
    {
        case Format::EC_SIMPLE:
        case Format::EC_GEMATIK:
            return EcSignature::from(data);

        case Format::RSA:
            return RsaSignature::from(data);

        default:
            throw std::runtime_error("invalid signature format, neither EC nor RSA");
    }
}


std::unique_ptr<Signature> Signature::fromP1363(const BinaryView& data)
{
    return EcSignature::fromP1363(data);
}


Signature::Format Signature::detectFormat(const BinaryView& data)
{
    switch (Asn1Helper::detectEcForm(data))
    {
        case Format::EC_SIMPLE:
            return Format::EC_SIMPLE;
        case Format::EC_GEMATIK:
            return Format::EC_GEMATIK;
        default:
            // Otherwise, as we have no positive detection for RSA, we just assume that it has to be RSA.
            return Format::RSA;
    }
}


// GEMREQ-start A_17207#create
std::unique_ptr<Signature> Signature::create(const BinaryView& data, EVP_PKEY& privateKey)
{
    auto signingContext = shared_EVP_MD_CTX::make();
    EVP_PKEY_CTX* keyContext = nullptr; // No transfer of ownership for the keyContext.

    int status =
        EVP_DigestSignInit(signingContext, &keyContext, EVP_sha256(), nullptr, &privateKey);
    Assert(status == 1) << "initialization of digest for signing failed";
    Assert(keyContext != nullptr) << "keyContext must not be null";

    fineTuneKeyContext(*keyContext, privateKey, {});

    status = EVP_DigestSignUpdate(signingContext, data.data(), data.size());
    Assert(status == 1) << "updating digest for signing failed";

    size_t signatureLength = 0;
    status = EVP_DigestSignFinal(signingContext, nullptr, &signatureLength);
    Assert(status == 1) << "finalizing digest for signing failed";

    BinaryBuffer signature(signatureLength);
    status = EVP_DigestSignFinal(signingContext, signature.data(), &signatureLength);
    Assert(signatureLength <= signature.size()) << "invalid signature size";
    Assert(status == 1) << "obtaining signature failed";
    signature.shrinkToSize(signatureLength);

    return from(static_cast<BinaryView>(signature));
}
// GEMREQ-end A_17207#create


// GEMREQ-start A_17207#verifyOrThrow
void Signature::verifyOrThrow(
    const BinaryView& data,
    EVP_PKEY& publicKey,
    const VerificationParameters verificationParameters) const
{
    auto verificationContext = shared_EVP_MD_CTX::make();
    EVP_PKEY_CTX* keyContext = nullptr; // No transfer of ownership for the keyContext.

    int status =
        EVP_DigestVerifyInit(verificationContext, &keyContext, EVP_sha256(), nullptr, &publicKey);
    Assert(status == 1) << "failed to initialize context for signature verification";
    Assert(keyContext != nullptr) << "keyContext must not be null";

    fineTuneKeyContext(*keyContext, publicKey, verificationParameters);

    status = EVP_DigestVerifyUpdate(verificationContext, data.data(), data.size());
    Assert(status == 1) << "failed to update signature verification";

    const BinaryBuffer simpleForm = toOpenSslBuffer();
    status = EVP_DigestVerifyFinal(verificationContext, simpleForm.data(), simpleForm.size());
    Assert(status >= 0) << "failed to finalize signature verification";
    Assert(status == 1) << "signature could not be verified";
}
// GEMREQ-end A_17207#verifyOrThrow


void Signature::fineTuneKeyContext(
    EVP_PKEY_CTX& keyContext,
    EVP_PKEY& privateKey,
    const VerificationParameters verificationParameters)
{
    const auto type = EVP_PKEY_id(&privateKey);
    switch (type)
    {
        case NID_rsa:
        case NID_rsaEncryption:
        case NID_rsaSignature:
            if (verificationParameters.isMgf1SupportedForRsa)
            {
                int status = EVP_PKEY_CTX_set_rsa_padding(&keyContext, RSA_PKCS1_PSS_PADDING);
                Assert(status == 1) << "";
                status = EVP_PKEY_CTX_set_rsa_mgf1_md(&keyContext, EVP_sha256());
                Assert(status == 1) << "";
                status = EVP_PKEY_CTX_set_rsa_pss_saltlen(&keyContext, RSA_PSS_SALTLEN_DIGEST);
                Assert(status == 1) << "";
            }
            break;

        default:
            // Nothing to do for elliptic curve keys.
            break;
    }
}


// ----- EcSignature ---------------------------------------------------------

std::unique_ptr<Signature> EcSignature::from(const BinaryView& data)
{
    auto [r, s] = Asn1Helper::parse(data);
    return std::make_unique<EcSignature>(std::move(r), std::move(s));
}


std::unique_ptr<EcSignature> EcSignature::fromP1363(const BinaryView& data)
{
    const size_t halfSize = data.size() / 2;
    Assert(halfSize * 2 == data.size()) << "Signature size must be even.";

    return std::make_unique<EcSignature>(
        binaryToBN(data.first(halfSize), "R"), binaryToBN(data.last(halfSize), "S"));
}


EcSignature::EcSignature(shared_BN r, shared_BN s)
  : mR(std::move(r)),
    mS(std::move(s))
{
    Assert(mR.isSet()) << "mR must be set";
    Assert(mS.isSet()) << "mS must be set";
}


std::unique_ptr<Signature> EcSignature::create(const BinaryView& data, EC_KEY& privateKey)
{
    auto pKey = shared_EVP_PKEY::make();
    int status = EVP_PKEY_set1_EC_KEY(pKey, &privateKey);
    Assert(status == 1) << "could not convert EC key to EVP_PKEY";
    Assert(pKey != nullptr) << "created EC key must not be null";
    return Signature::create(data, *pKey);
}


BinaryBuffer EcSignature::getR() const
{
    BinaryBuffer r(static_cast<size_t>(BN_num_bytes(mR)));
    const size_t rLength = gsl::narrow<size_t>(BN_bn2bin(mR, r.data()));
    Assert(r.size() == rLength) << "conversion of BIGNUM r to binary failed";
    return r;
}


BinaryBuffer EcSignature::getS() const
{
    BinaryBuffer s(static_cast<size_t>(BN_num_bytes(mS)));
    const size_t sLength = gsl::narrow<size_t>(BN_bn2bin(mS, s.data()));
    Assert(s.size() == sLength) << "conversion of BIGNUM s to binary failed";
    return s;
}


BinaryBuffer EcSignature::toBuffer() const
{
    return toBufferInSimpleForm();
}


BinaryBuffer EcSignature::toOpenSslBuffer() const
{
    return toBufferInSimpleForm();
}


BinaryBuffer EcSignature::toBufferInSimpleForm() const
{
    return Asn1Helper::toBufferInSimpleForm(mR, mS);
}


BinaryBuffer EcSignature::toBufferInGematikForm() const
{
    return Asn1Helper::toBufferInGematikForm(mR, mS);
}


std::string EcSignature::toJwtBase64String(const size_t signatureSizeBits) const
{
    // TODO: update getR() and getS() to use padding
    // for p1363 format the R and S must be padded to provide the expected signature size
    return Base64::toBase64Url(Base64::encode(toRSBuffer(signatureSizeBits)));
}


std::string EcSignature::toNormalBase64String(const size_t signatureSizeBits) const
{
    // for p1363 format the R and S must be padded to provide the expected signature size
    return Base64::encode(toRSBuffer(signatureSizeBits));
}


BinaryBuffer EcSignature::toRSBuffer(const size_t signatureSizeBits) const
{
    return BinaryBuffer::concatenate(
        bnToBinary(mR, signatureSizeBits, "R"), bnToBinary(mS, signatureSizeBits, "S"));
}


// ----- RsaSignature --------------------------------------------------------

std::unique_ptr<Signature> RsaSignature::from(const BinaryView& data)
{
    return std::make_unique<RsaSignature>(BinaryBuffer(data));
}


RsaSignature::RsaSignature(BinaryBuffer data)
  : mData(std::move(data))
{
}


BinaryBuffer RsaSignature::toBuffer() const
{
    return mData;
}


BinaryBuffer RsaSignature::toOpenSslBuffer() const
{
    return mData;
}


std::string RsaSignature::toJwtBase64String(const size_t) const
{
    return Base64::toBase64Url(Base64::encode(mData));
}


std::string RsaSignature::toNormalBase64String(const size_t) const
{
    return Base64::encode(mData);
}

} // namespace epa
