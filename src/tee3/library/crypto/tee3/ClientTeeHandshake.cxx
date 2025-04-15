/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee3/ClientTeeHandshake.hxx"
#include "library/crypto/Signature.hxx"
#include "library/crypto/SignatureVerifier.hxx"
#include "library/crypto/tee3/Kyber768.hxx"
#include "library/crypto/tee3/Tee3Protocol.hxx"
#include "library/crypto/tee3/TeeMessage1.hxx"
#include "library/crypto/tee3/TeeMessage2.hxx"
#include "library/crypto/tee3/TeeMessage3.hxx"
#include "library/crypto/tee3/TeeMessage4.hxx"
#include "library/ocsp/OcspHelper.hxx"
#include "library/util/ByteHelper.hxx"
#include "library/util/cbor/CborDeserializer.hxx"
#include "library/util/cbor/CborSerializer.hxx"

#include "shared/crypto/EllipticCurveUtils.hxx"
#include "shared/tsl/TslManager.hxx"
#include "shared/tsl/TslService.hxx"
#include "shared/tsl/error/TslError.hxx"
#include "shared/util/Hash.hxx"

namespace epa
{

namespace
{
   // GEMREQ-start A_24624-01#verifySignature
   bool verifySignature(
       const BinaryBuffer& signature,
       const BinaryBuffer& signedData,
       EVP_PKEY* signatureKey)
   {
        try
        {
            Assert(64 == signature.size()) << "Unexpected size of signature";
            const auto ecSignature = EcSignature::fromP1363(signature);
            Assert(ecSignature != nullptr) << "no signature could be retrieved";
            ecSignature->verifyOrThrow(signedData, *signatureKey, {});

            return true;
        }
        catch (const std::runtime_error& e)
        {
            return false;
        }
   }
   // GEMREQ-end A_24624-01#verifySignature
}


ClientTeeHandshake::ClientTeeHandshake(CertificateProvider certificateProvider, TslManager& tslManager, bool isPU)
  : mState(State::NotStarted),
    mTransscript(),
    mEphemeralKeys(),
    mK2KeyConfirmation(),
    mCertificateProvider(std::move(certificateProvider)),
    mK2ApplicationData(),
    mKeyId(),
    mVauCid(),
    mTslManager{tslManager},
    mIsPU{isPU}
{
}


// GEMREQ-start A_24428#createMessage1
BinaryBuffer ClientTeeHandshake::createMessage1()
{
    Assert(mState == State::NotStarted);

    // A_24428: create an ECC (P-256) and a Kyber768 key pair.
    mEphemeralKeys = Tee3Protocol::Keypairs::generate();
    report("ephemeral ecdh public key", mEphemeralKeys.ecdh.publicKey.serialize());
    report("ephemeral ecdh private key", *mEphemeralKeys.ecdh.privateKey);
    report("ephemeral kyber768 public key", mEphemeralKeys.kyber768.publicKey);
    report("ephemeral kyber768 private key", *mEphemeralKeys.kyber768.privateKey);

    // A_24428: create M1 message from the public keys.
    const auto message1 = TeeMessage1{
        .messageType = Tee3Protocol::message1Name,
        .ecdhPublicKey = mEphemeralKeys.ecdh.publicKey,
        .kyber768PublicKey = mEphemeralKeys.kyber768.publicKey};

    // A_24428: serialize to CBOR.
    auto serialized = CborSerializer::serialize(message1);

    mTransscript = serialized;
    mState = State::Message1Created;

    return serialized;
}
// GEMREQ-end A_24428#createMessage1


void ClientTeeHandshake::setVauCid(const Tee3Protocol::VauCid& vauCid)
{
    mVauCid = vauCid;
}


// GEMREQ-start A_24623#step1
BinaryBuffer ClientTeeHandshake::createMessage3(const BinaryBuffer& serializedMessage2)
{
    Assert(mState == State::Message1Created);

    mTransscript = BinaryBuffer::concatenate(mTransscript, serializedMessage2);

    const auto message2 = CborDeserializer::deserialize<TeeMessage2>(serializedMessage2);
    report("ECDH_ct", CborSerializer::serialize(message2.ecdhCipherText));
    report("Kyber768_ct", message2.kyber768CipherText);
    report("AEAD_ct", message2.aeadCipherText);

    // A_24623: Calculate ss_e_ecdh and ss_e_kyber768 from the cipher texts in message2 and the
    // ephemeral client keys from message1.
    const auto kemResult1 // key_result_1
        = decapsulate(
            Tee3Protocol::CipherTexts{
                .ecdhCipherText = message2.ecdhCipherText,          // ECDH_ct
                .kyber768CipherText = message2.kyber768CipherText}, // Kyber768_ct
            mEphemeralKeys.privateKeys());                          // private ephemeral client key
    report("ss_e_ecdh", *kemResult1.ecdhSharedSecret);
    report("ss_e_kyber768", *kemResult1.kyber768SharedSecret);

    // A_24623: Concatenate the shared secrets ss_e = ss_e_ecdh || ss_e_kyber768
    //          and use hkdf (SHA-256, info="") to derive 2*32 bytes: K1-c2s and K1-c2s
    const auto k1 = hkdf(kemResult1);
    report("K1_c2s", *k1.clientToServer);
    report("K1_s2c", *k1.serverToClient);
    // GEMREQ-end A_24623#step1

    // GEMREQ-start A_24623#step2
    // A_24623: Use K1_s2c to decrypt ciphertext AEAD_ct.
    const auto serializedPublicServerKeys =
        aeadDecrypt(k1.serverToClient, message2.aeadCipherText, "AEAD_ct");
    // Deserialize the decrypted public keys.
    const auto publicServerKeys =
        CborDeserializer::deserialize<Tee3Protocol::SignedPublicKeys>(serializedPublicServerKeys);

    // A_24958: Retrieve certificate data, and A_24624-01: verify the public keys.
    verifyPublicKeys(publicServerKeys);
    // Deserialize the public keys.
    const auto publicKeys =
        CborDeserializer::deserialize<Tee3Protocol::VauKeys>(publicServerKeys.signedPublicKeys);

    // A_24623: Use the public keys to perform a KEM encapsulation, resulting in ss_s_ecdh and ss_s_kyber768 (in client_kem_result_2.sharedKeys).
    const auto kemResult2 // kem_result_2
        = encapsulate(publicKeys.ecdhPublicKey, publicKeys.kyber768PublicKey);
    report("ss_s_ecdh", *kemResult2.sharedKeys.ecdhSharedSecret);
    report("ss_s_kyber768", *kemResult2.sharedKeys.kyber768SharedSecret);

    // A_24623: Concatenate ss
    //     = ss_e || ss_s = ss_e_ecdh || ss_e_kyber768 || ss_s_ecdh || ss_s_kyber768.
    const auto sharedSecret = SensitiveDataGuard(BinaryBuffer::concatenate(
        *kemResult1.ecdhSharedSecret,                  // ss_e_ecdh
        *kemResult1.kyber768SharedSecret,              // ss_e_kyber768
        *kemResult2.sharedKeys.ecdhSharedSecret,       // ss_s_ecdh
        *kemResult2.sharedKeys.kyber768SharedSecret)); // ss_s_kyber768
    report("ss", *sharedSecret);
    // GEMREQ-end A_24623#step2

    // GEMREQ-start A_24623#step3
    // A_24623: Derive 4 keys and 1 KeyId, 256 Bit each = 5 * 32 Bytes = 160 Bytes.
    const auto k2 = hkdf(sharedSecret);
    mK2KeyConfirmation = k2.keyConfirmation;
    mK2ApplicationData = k2.applicationData;
    mKeyId = k2.keyId;
    report("K2_c2s_key_confirmation", *mK2KeyConfirmation.clientToServer);
    report("K2_c2s_app_data", *mK2ApplicationData.clientToServer);
    report("K2_s2c_key_confirmation", *mK2KeyConfirmation.serverToClient);
    report("K2_s2c_app_data", *mK2ApplicationData.serverToClient);
    report("KeyID", mKeyId->toHexString());
    // GEMREQ-end A_24623#step3

    // GEMREQ-start A_24623#step4

    // For message m2 the ECDH_pk field has been clarified to contain the ecdh public key as
    // nested structure. So that is what we will be using here as well.
    auto ecdhct = kemResult2.cipherTexts.ecdhCipherText;

    // A_24623: Create the inner layer of the M3 message from the cipher texts of the KEM encapsulation.
    const auto innerMessage3 = TeeMessage3InnerLayer{
        .ecdhCipherText = ecdhct,
        .kyber768CipherText = kemResult2.cipherTexts.kyber768CipherText,
        .erp = false,
        .eso = false};

    // A_24623: CBOR serialization of the inner layer, then encryption with K1_c2s.
    const auto serializedInnerMessage3 = CborSerializer::serialize(innerMessage3);
    const auto ciphertextMessage3 // ciphertext_msg_3
        = aeadEncrypt(k1.clientToServer, serializedInnerMessage3);
    report("ciphertext_msg_3", ciphertextMessage3);

    // A_24623: Concatenate M1 and M2 messages (already in mTransscript) and ciphertext_msg_3.
    const auto transscriptClientToSend // transscript_client_to_send
        = BinaryBuffer::concatenate(mTransscript, ciphertextMessage3);

    // A_24623: Compute the SHA-256 hash and encrypt the result with K2_c2s_key_confirmation.
    const auto transscriptClientHash = Hash::sha256(transscriptClientToSend.getString());
    const auto aeadCiphertextMessage3KeyConfirmation // aeadCiphertextMessage3KeyConfirmation
        = aeadEncrypt(k2.keyConfirmation.clientToServer, BinaryBuffer{transscriptClientHash});
    report("aead_ciphertext_msg_3", aeadCiphertextMessage3KeyConfirmation);

    // A_24623: Create the M3 message.
    const auto message3 = TeeMessage3{
        .messageType = Tee3Protocol::message3Name,
        .aeadCipherText = ciphertextMessage3,
        .aeadCipherTextKeyConfirmation = aeadCiphertextMessage3KeyConfirmation};

    // A_24623: return the CBOR serialization of the M3 message. It will be sent to POST <VAU-CID>
    auto serializedMessage3 = CborSerializer::serialize(message3);
    mTransscript = BinaryBuffer::concatenate(mTransscript, serializedMessage3);

    mState = State::Message3Created;

    return serializedMessage3;
}
// GEMREQ-end A_24623#step4


// GEMREQ-start A_24627#processMessage4
void ClientTeeHandshake::processMessage4(const BinaryBuffer& serializedMessage4)
{
    Assert(mState == State::Message3Created);

    // A_24627: Deserialize the M4 message.
    const auto message4 = CborDeserializer::deserialize<TeeMessage4>(serializedMessage4);

    // A_24627: Concatenate messages m1, m2 and m3 (already present in mTransscript) and
    // compute the SHA-256 hash.
    const auto expectedTransscriptHash = Hash::sha256(mTransscript.getString());

    try
    {
        // A_24627: Decrypted AEAD_ct_key_confirmation with K2_s2c_key_confirmation.
        const auto givenTransscriptHash = aeadDecrypt(
            mK2KeyConfirmation.serverToClient,      // K2_s2c_key_confirmation
            message4.aeadCipherTextKeyConfirmation, // AEAD_ct_key_confirmation
            "AEAD_ct_key_confirmation");

        // A_24627: Both transscript hashes must be identical.
        Assert(givenTransscriptHash.getString() == expectedTransscriptHash)
            << "client transscripts do not match";

        mState = State::Finished;
    }
    catch (const std::runtime_error&)
    {
        // A_24627: If the decryption failed or the hashes do not match, abort the handshake with
        // and error.

        mState = State::Error;
        throw;
    }
}
// GEMREQ-end A_24627#processMessage4


// GEMREQ-start A_24627#context
std::shared_ptr<Tee3Context> ClientTeeHandshake::context() const
{
    Assert(mState == State::Finished);
    return std::make_shared<Tee3Context>(mK2ApplicationData, mKeyId.value(), mVauCid, "", mIsPU);
}
// GEMREQ-end A_24627#context


const Tee3Protocol::VauCid& ClientTeeHandshake::vauCid() const
{
    return mVauCid;
}


void ClientTeeHandshake::verifyPublicKeys(const Tee3Protocol::SignedPublicKeys& publicKeys)
{
    bool isValid = true;

    const auto& certificate = mCertificateProvider(publicKeys.certificateHash, publicKeys.cdv);
    auto x509cert = certificate.toX509();
    const auto vauTi = X509Certificate::createFromX509Pointer(x509cert);
    const auto vauKeys =
        CborDeserializer::deserialize<Tee3Protocol::VauKeys>(publicKeys.signedPublicKeys);

    // GEMREQ-start A_24624-01#1-2
    // A_24624, 1: Verify TI certificate with the ocsp response. The OCSP response
    // must not be older than 24 hours.
    // A_24624, 2a: Check the valid time range
    // A_24624, 2b: Verify certificate chain that must be based in root certificate in TI-PKI.
    OcspCheckDescriptor ocspCheckDescriptor{
        .mode = OcspCheckDescriptor::PROVIDED_ONLY,
        .timeSettings = {.referenceTimePoint = std::nullopt, .gracePeriod = std::chrono::hours{24}},
        .providedOcspResponse = OcspHelper::binaryBufferToOcspResponse(publicKeys.ocspResponse)};

    // GEMREQ-start A_24958#tsl
    try
    {
        mTslManager.verifyCertificate(
            TslMode::TSL, vauTi, {CertificateType::C_FD_AUT}, ocspCheckDescriptor);
    }
    catch (const TslError& e)
    {
        isValid = false;
        LOG(DEBUG1) << "certificate validation with Tsl Manager failed:" << e.what();
    }
    // GEMREQ-end A_24958#tsl,A_24624-01#1-2

    // GEMREQ-start A_24624#role
    // A_24624, 3: the certificate must come from the component PKI of TI and have role
    //          "oid_epa_vau".
    if (! vauTi.checkRoles({TslService::oid_epa_vau}))
    {
        isValid = false;
        LOG(DEBUG1) << "certificate does not have role oid_epa_vau";
    }
    // GEMREQ-end A_24624#role
    auto printPubkey = [](const X509Certificate& cert) -> std::string {
        auto key = shared_EVP_PKEY::make(cert.getPublicKey());
        auto [x, y] = EllipticCurveUtils::getPublicKeyCoordinatesHex(key);
        key.release();
        return "x=" + x + ", y=" + y;
    };

    // GEMREQ-start A_24624#signature
    // A_24624, 4: Verify that "signature-ES256" must be a valid signature that matches data in
    // "signed_pub_keys".
    Assert(verifySignature(
        publicKeys.signatureES256, publicKeys.signedPublicKeys, vauTi.getPublicKey()))
        << "signature of signed_pub_keys can not be verified "
        << "(signature=" << ByteHelper::toHex(publicKeys.signatureES256) << ", "
        << "signedData=" << ByteHelper::toHex(publicKeys.signedPublicKeys) << ", "
        << "pubkey=[" << printPubkey(vauTi) << "], "
        << "certHash=" << ByteHelper::toHex(publicKeys.certificateHash) << ", "
        << "cdv=" << publicKeys.cdv << ")";
    // GEMREQ-end A_24624#signature

    // GEMREQ-start A_24624#pubkeys
    // A_24624, 5: The ECC key in "signed_pub_keys" has to be valid P-256 key and "Kyber768_PK"
    // must contain a valid Kyber768 key.
    TeeHandshakeBase::verifyVauKeys(vauKeys.ecdhPublicKey, vauKeys.kyber768PublicKey);
    // GEMREQ-end A_24624#pubkeys

    // GEMREQ-start A_24624#expiry
    // A_24624, 6: The time in "signed_pub_keys.exp" must not be expired.
    // Note that the names that are defined in gemSpec_Krypt are a bit confusing. See A_24425.
    const auto nowSinceEpoch = std::chrono::system_clock::now().time_since_epoch();
    const auto nowSecondsSinceEpoch =
        std::chrono::duration_cast<std::chrono::seconds>(nowSinceEpoch);
    Assert(vauKeys.exp > nowSecondsSinceEpoch.count())
        << "signed public keys are expired by " << nowSecondsSinceEpoch.count() - vauKeys.exp
        << " seconds";

    Assert(isValid);
    // GEMREQ-end A_24624#expiry
}


const Tee3Protocol::SymmetricKeys& ClientTeeHandshake::k2KeyConfirmation() const
{
    return mK2KeyConfirmation;
}

} // namespace epa
