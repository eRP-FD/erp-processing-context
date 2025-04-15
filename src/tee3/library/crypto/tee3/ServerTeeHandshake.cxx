/*
 *  (C) Copyright IBM Deutschland GmbH 2021, 2025
 *  (C) Copyright IBM Corp. 2021, 2025
 *  non-exclusively licensed to gematik GmbH
 */

#include "library/crypto/tee3/ServerTeeHandshake.hxx"
#include "library/crypto/tee3/Kyber768.hxx"
#include "library/crypto/tee3/TeeMessage1.hxx"
#include "library/crypto/tee3/TeeMessage2.hxx"
#include "library/crypto/tee3/TeeMessage3.hxx"
#include "library/crypto/tee3/TeeMessage4.hxx"
#include "library/crypto/tee3/TeeServerKeys.hxx"
#include "library/util/ByteHelper.hxx"
#include "library/util/Random.hxx"
#include "library/util/VauCidEncoder.hxx"
#include "library/util/cbor/CborDeserializer.hxx"
#include "library/util/cbor/CborSerializer.hxx"

#include "shared/network/message/Header.hxx"
#include "shared/util/Hash.hxx"

namespace epa
{

namespace
{
    Tee3Protocol::SignedPublicKeys signedPublicServerKeys(const TeeServerKeys& serverKeys)
    {
        return serverKeys.signedPublicKeys();
    }


    template<class MessageType>
    MessageType deserializeMessage(const BinaryBuffer& serializedM1)
    {
        try
        {
            return CborDeserializer::deserialize<MessageType>(serializedM1);
        }
        catch (const CborError& e)
        {
            LOG(ERROR) << "caught CBOR error while deserializing M1";
            // Translate the CborError into a TeeError.
            if (e.code() == CborError::Code::DeserializationMissingItem)
                throw TeeError(TeeError::Code::MissingParameters, e.what());
            else if (e.code() == CborError::Code::DeserializationError)
                throw TeeError(TeeError::Code::DecodingError, e.what());
            else
                throw TeeError(TeeError::Code::InternalServerError, e.what());
        }
        catch (const std::runtime_error& e)
        {
            LOG(ERROR) << "caught runtime error while deserializing M1";
            // Interpret errors that are not explicitly reported as CBOR error as
            // decoding error rather than internal server error under the assumption
            // that it is more likely that the error is caused by faulty input data than
            // an implementation error.
            throw TeeError(TeeError::Code::DecodingError, e.what());
        }
    }
}


ServerTeeHandshake::ServerTeeHandshake(const TeeServerKeys& serverKeys, bool isPU)
  : mServerKeys(serverKeys),
    mTransscript(),
    mK1(),
    mKemResult1(),
    mK2ApplicationData(),
    mKeyId(),
    mVauCid(),
    mChannelId(),
    mIsPU{isPU}
{
}


void ServerTeeHandshake::generateAndSetVauCid(const Header& header)
{
    const auto& clusterAddress = header.header(Header::Epa::ClusterAddress);
    Assert(clusterAddress.has_value())
        << "cluster address header is missing"
        << assertion::exceptionType<TeeError>(TeeError::Code::MissingParameters);
    const auto& podAddress = header.header(Header::Epa::PodAddress);
    Assert(podAddress.has_value())
        << "pod address header is missing"
        << assertion::exceptionType<TeeError>(TeeError::Code::MissingParameters);

    // Initialize the VAU-CID value. It will be returned in the HTTP response header.
    // A_24608: The returned value must be a valid URL and not be longer than 200 characters.
    // A_24622: The returned value must only contains characters a-zA-Z0-9-/. The first
    //          character must be a slash.
    // TODO: specify and implement a schema.
    // The current schema starts with /VAU/v1. Later versions can increase v1 to higher numbers.
    // The following parts are:
    // - cluster address ( provided by TEE Router )
    // - pod address ( provided by TEE Router )
    // - a 32 byte number that identifies the channel.
    // The channel identifier is used to connect the first with the second handshake request.
    // All the three parts are hex-encoded as the supported character set only contains
    // 63 characters ('/' is used as path separator) and we don't have (yet) a base 63 encoding.

    std::string channelId =
        ByteHelper::toHex(static_cast<BinaryView>(Random::randomBinaryData(32)));
    const std::string clusterAddressEncoded = VauCidEncoder::encode(*clusterAddress);
    const std::string podAddressEncoded = VauCidEncoder::encode(*podAddress);
    Tee3Protocol::VauCid vauCid(
        "/VAU/v1/" + clusterAddressEncoded + "/" + podAddressEncoded + "/" + channelId);
    vauCid.verify();

    setChannelId(std::move(channelId));
    setVauCid(std::move(vauCid));
}


BinaryBuffer ServerTeeHandshake::createMessage2(const BinaryBuffer& serializedMessage1)
{
    const auto vauServerSignedPublicKeys // vau_server_signed_pub_keys
        = signedPublicServerKeys(mServerKeys);

    // A_24429: deserialize the M1 message.
    const auto message1 = deserializeMessage<TeeMessage1>(serializedMessage1);

    // A_24608: Verify public keys in m1.
    verifyVauKeys(message1.ecdhPublicKey, message1.kyber768PublicKey);

    // A_24608: Apply matching KEM encapsulate functions to both public keys. The returned object
    // contains two shared keys and two cipher texts.
    mKemResult1 = encapsulate(message1.ecdhPublicKey, message1.kyber768PublicKey);
    report("ECDH_ct", CborSerializer::serialize(mKemResult1.cipherTexts.ecdhCipherText));
    report("Kyber768_ct", mKemResult1.cipherTexts.kyber768CipherText);
    report("ss_e_ecdh", *mKemResult1.sharedKeys.ecdhSharedSecret);
    report("ss_e_kyber768", *mKemResult1.sharedKeys.kyber768SharedSecret);

    // A_24608: key derivation of server side keys K1_c2s and K1_s2c.
    mK1 = hkdf(mKemResult1.sharedKeys);
    report("K1_c2s", *mK1.clientToServer);
    report("K1_s2c", *mK1.serverToClient);

    // A_24608: encrypt the signed public keys with key K1_s2c.
    const auto aeadCiphertextMessage2 // aead_ciphertext_msg_2
        = aeadEncrypt(mK1.serverToClient, CborSerializer::serialize(vauServerSignedPublicKeys));

    // A_24608: create M2 message and serialize with CBOR.
    const auto m2 = TeeMessage2{
        .messageType = Tee3Protocol::message2Name,
        .ecdhCipherText = mKemResult1.cipherTexts.ecdhCipherText,
        .kyber768CipherText = mKemResult1.cipherTexts.kyber768CipherText,
        .aeadCipherText = aeadCiphertextMessage2};
    auto serializedM2 = CborSerializer::serialize(m2);

    // Remember the serialized messages M1 and M2. They will be used when message M3 is processed.
    mTransscript = BinaryBuffer::concatenate(serializedMessage1, serializedM2);

    return serializedM2;
}


BinaryBuffer ServerTeeHandshake::createMessage4(const BinaryBuffer& serializedMessage3)
{
    // A24624: Deserialize message M3.
    const auto m3 = deserializeMessage<TeeMessage3>(serializedMessage3);

    // A_24626: As preparation for the KEM decapsulation we decrypt and deserialize the AEAD_ct field.
    BinaryBuffer kemClientToServerCbor; // aka kem_cts_cbor
    kemClientToServerCbor =
        aeadDecrypt(mK1.clientToServer, m3.aeadCipherText, "decryption of M3/AEAD_ct");
    const auto kemCipherTexts // kem_cts
        = CborDeserializer::deserialize<TeeMessage3InnerLayer>(kemClientToServerCbor);

    // The ECDH_ct field for message m2 has been clarified to contain a EcdhPublicKey structure.
    // So that is what we are using here as well.
    auto ecdhct = kemCipherTexts.ecdhCipherText;

    // A_24626 (A_24623): KEM decapsulation as first step of key derivation.
    //          This produces ss_s_ecdh and ss_s_kyber768.
    const auto kemResult2 = decapsulate(
        Tee3Protocol::CipherTexts{
            .ecdhCipherText = ecdhct, .kyber768CipherText = kemCipherTexts.kyber768CipherText},
        mServerKeys.privateKeys());
    report("ss_s_ecdh", *kemResult2.ecdhSharedSecret);
    report("ss_s_kyber768", *kemResult2.kyber768SharedSecret);

    // A_24626 (A_24623): Concatenate ss
    //     = ss_e || ss_s = ss_e_ecdh || ss_e_kyber768 || ss_s_ecdh || ss_s_kyber768.
    const auto sharedSecret = SensitiveDataGuard(BinaryBuffer::concatenate(
        *mKemResult1.sharedKeys.ecdhSharedSecret,     // ss_e_ecdh
        *mKemResult1.sharedKeys.kyber768SharedSecret, // ss_e_kyber768
        *kemResult2.ecdhSharedSecret,                 // ss_s_ecdh
        *kemResult2.kyber768SharedSecret));           // ss_s_kyber768
    report("ss", *sharedSecret);

    // A_24626 (A_24623): Key derivation of K2 keys for (c2s|s2c)_(key_confirmation|app_data)
    // and of KeyId, 4 keys and 1 KeyId, 256 Bit each = 5 * 32 Bytes = 160 Bytes.
    const auto k2 = hkdf(sharedSecret);
    mK2KeyConfirmation = k2.keyConfirmation;
    mK2ApplicationData = k2.applicationData;
    mKeyId.emplace(k2.keyId);
    report("k2_c2s_key_confirmation", *mK2KeyConfirmation.clientToServer);
    report("k2_s2c_key_confirmation", *mK2KeyConfirmation.serverToClient);
    report("k2_c2s_app_data", *mK2ApplicationData.clientToServer);
    report("k2_c2s_app_data", *mK2ApplicationData.serverToClient);
    report("keyId", mKeyId->toHexString());

    // A_24626: Compute client transscript and its SHA-256 hash value.
    const auto transscriptServerToCheck // transscript_server_to_check
        = BinaryBuffer::concatenate(mTransscript, m3.aeadCipherText);
    const auto teeHashComputed // vau_hash_berechnung
        = Hash::sha256(transscriptServerToCheck.getString());

    // A_24626: Decrypt client transcript hash value.
    const auto transscriptClientHash // transscript_hash_des_clients
        = aeadDecrypt(
            k2.keyConfirmation.clientToServer, // K2_c2s_key_confirmation
            m3.aeadCipherTextKeyConfirmation,  // AEAD_ct_key_confirmation
            "client transscript");

    // A_24626: Verify that that two client hash values are identical. Abort the handshake if not.
    Assert(transscriptClientHash.getString() == teeHashComputed)
        << "M4: client transscript hash"
        << assertion::exceptionType<TeeError>(TeeError::Code::TransscriptError);

    // A_24626: Compute transscript of messages m1, m2 and m3 and its SHA-256 hash.
    mTransscript = BinaryBuffer::concatenate(mTransscript, serializedMessage3);
    const auto serverTransscriptHash = Hash::sha256(mTransscript.getString());

    // A_24626: Encrypt the server transscript hash with K2_s2c_key_confirmation.
    auto keyConfirmation =
        aeadEncrypt(k2.keyConfirmation.serverToClient, BinaryBuffer{serverTransscriptHash});
    report("AEAD_ct_key_confirmation", keyConfirmation);

    // A_24626: Create message m4 and return its CBOR serialization.
    const auto m4 = TeeMessage4{
        .messageType = Tee3Protocol::message4Name,
        .aeadCipherTextKeyConfirmation = std::move(keyConfirmation)};

    return CborSerializer::serialize(m4);
}


Tee3Context ServerTeeHandshake::context() const
{
    Assert(mKeyId.has_value()) << "KeyId has not yet been initialized";
    return Tee3Context(mK2ApplicationData, mKeyId.value(), mVauCid, mChannelId, mIsPU);
}


void ServerTeeHandshake::setVauCid(Tee3Protocol::VauCid&& vauCid)
{
    Assert(mVauCid.empty());
    mVauCid = std::move(vauCid);
}


const Tee3Protocol::VauCid& ServerTeeHandshake::vauCid() const
{
    return mVauCid;
}


void ServerTeeHandshake::setChannelId(std::string&& channelId)
{
    Assert(mChannelId.empty());
    mChannelId = std::move(channelId);
}


const std::string& ServerTeeHandshake::channelId() const
{
    return mChannelId;
}

} // namespace epa
