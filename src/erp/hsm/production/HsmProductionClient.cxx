/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/production/HsmProductionClient.hxx"
#include "erp/hsm/HsmIdentity.hxx"
#include "erp/hsm/HsmSessionExpiredException.hxx"
#include "erp/hsm/production/HsmRawSession.hxx"
#include "erp/util/Base64.hxx"
#include "erp/util/ByteHelper.hxx"
#include "erp/util/Configuration.hxx"
#include "erp/util/DurationConsumer.hxx"
#include "erp/util/String.hxx"

#include "erp/util/TLog.hxx"

namespace
{
    template<int L>
    void setInput (unsigned char destination[L], const ErpArray<L>& source)
    {
        std::copy(source.begin(), source.end(), destination);
    }

    void setInput (hsmclient::ERPBlob_s& destination, ErpBlob& source)
    {
        destination.BlobGeneration = gsl::narrow<unsigned int>(source.generation);
        destination.BlobLength = source.data.size();
        const char* p = source.data;
        std::copy(p, p+source.data.size(), destination.BlobData);
    }

    void setInput (size_t& destination, const ErpVector& source)
    {
        destination = source.size();
    }

    void setInput (unsigned char destination[MaxBuffer], const ErpVector& source)
    {
        std::copy(source.begin(), source.end(), destination);
    }

    void setInput (unsigned int& destination, const std::optional<bool>& source)
    {
        Expect(source.has_value(), "source bool missing");
        destination = source.value() ? 1 : 0;
    }


    ErpBlob convertErpBlob (const hsmclient::ERPBlob_s& blob)
    {
        Expect(blob.BlobLength <= MaxBuffer, "invalid hsm blob");
        return ErpBlob(
            std::vector<uint8_t>(blob.BlobData, blob.BlobData+blob.BlobLength),
            gsl::narrow<uint32_t>(blob.BlobGeneration));
    }

    ErpVector createErpVector (const size_t length, const uint8_t buffer[])
    {
        ErpVector data;
        data.resize(length);
        std::copy(buffer, buffer+length, data.begin());
        return data;
    }

    template<size_t L>
    ErpArray<L> createErpArray (const uint8_t data[L])
    {
        ErpArray<L> array;
        std::copy(data, data+L, array.data());
        return array;
    }


    /**
     * When status is  ERP_UTIMACO_SESSION_EXPIRED then throw a HsmSessionExpiredException to allow the caller to reconnect
     * and try again.
     * An expired session should happen seldodmly enough so that using an exception to notify it to the caller is
     * an acceptable overhead.
     */
    void handleExpiredSession(const uint32_t status)
    {
        switch(status)
        {
            case ERP_UTIMACO_SESSION_EXPIRED:
            case ERP_UTIMACO_SESSION_CLOSED:
            case ERP_UTIMACO_MAX_AUTH_FAILURES:
            case ERP_UTIMACO_MAX_LOGINS:
            case ERP_UTIMACO_INVALID_MESSAGING_ID:
            case ERP_UTIMACO_MESSAGING_FAILED:
            case ERP_UTIMACO_INVALID_USER_NAME:
            case ERP_UTIMACO_UNKNOWN_USER:
                throw ExceptionWrapper<HsmSessionExpiredException>::create({__FILE__, __LINE__},
                    "HSM session expired: " + HsmProductionClient::hsmErrorDetails(status),
                    status);

            default:
                // Other error conditions are handled elsewhere.
                break;
        }
    }


    DeriveKeyOutput derivePersistenceKey (
        const HsmRawSession& session,
        DeriveKeyInput&& arguments,
        const std::string& keyName,
        hsmclient::DeriveKeyOutput (*deriveKey) (hsmclient::HSMSession, hsmclient::DeriveKeyInput))
    {
        auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryHsm, keyName);

        hsmclient::DeriveKeyInput input;
        setInput<TpmObjectNameLength>(input.AKName, arguments.akName);
        setInput(input.TEEToken, arguments.teeToken);
        setInput(input.derivationKey, arguments.derivationKey);
        setInput(input.initialDerivation, arguments.initialDerivation);
        setInput(input.derivationDataLength, arguments.derivationData);
        setInput(input.derivationData, arguments.derivationData);

        const hsmclient::DeriveKeyOutput response = (*deriveKey)(
            session.rawSession,
            input);
        handleExpiredSession(response.returnCode);
        HsmExpectSuccess(response, "ERP_DeriveTaskKey failed", timer);
        Expect(response.derivationDataLength<MaxBuffer, "invalid response from ERP_DeriveTaskKey");

        DeriveKeyOutput output;
        const auto derivationData = createErpVector(response.derivationDataLength, response.derivationData);
        output.derivedKey = createErpArray<Aes256Length>(response.derivedKey);

        if (arguments.initialDerivation)
        {
            // Return salt and blob generation.
            ErpExpect(derivationData.startsWith(arguments.derivationData), HttpStatus::InternalServerError, "HSM returned invalid derivation result");
            output.optionalData = OptionalDeriveKeyData{
                derivationData.tail(derivationData.size() - arguments.derivationData.size()),
                arguments.blobId};
        }
        else
        {
            // Verify that salt did not change.
            ErpExpect(derivationData == arguments.derivationData, HttpStatus::InternalServerError, "salt was added on second key derivation call");
        }

        return output;
    }
}

::Nonce HsmProductionClient::getNonce(const ::HsmRawSession& session, uint32_t input)
{
    auto timer = ::DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryHsm, "Hsm:ERP_GenerateNONCE");

    const auto nonceOutput = ::hsmclient::ERP_GenerateNONCE(session.rawSession, ::hsmclient::UIntInput{input});

    HsmExpectSuccess(nonceOutput, "ERP_GenerateNONCE failed", timer);

    return {{nonceOutput.NONCE, nonceOutput.NONCE + NONCE_LEN},
            ::ErpBlob{::std::vector<uint8_t>{nonceOutput.BlobOut.BlobData,
                                             nonceOutput.BlobOut.BlobData + nonceOutput.BlobOut.BlobLength},
                      nonceOutput.BlobOut.BlobGeneration}};
}

ErpBlob HsmProductionClient::generatePseudonameKey(const ::HsmRawSession& session, uint32_t input)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryHsm, "Hsm:ERP_GeneratePseudonameKey");
    const auto result = ::hsmclient::ERP_GeneratePseudonameKey(session.rawSession, ::hsmclient::UIntInput{input});
    HsmExpectSuccess(result, "ERP_GeneratePseudonameKey failed", timer);
    return ::ErpBlob{::std::vector<uint8_t>{result.BlobOut.BlobData, result.BlobOut.BlobData + result.BlobOut.BlobLength},
                     result.BlobOut.BlobGeneration};
}

ErpArray<Aes256Length> HsmProductionClient::unwrapPseudonameKey(const HsmRawSession& session, UnwrapHashKeyInput&& input)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryHsm, "Hsm:ERP_UnwrapPseudonameKey");
    hsmclient::TwoBlobGetKeyInput requestInput;
    setInput(requestInput.TEEToken, input.teeToken);
    setInput(requestInput.Key, input.key);
    const auto response = hsmclient::ERP_UnwrapPseudonameKey(session.rawSession, requestInput);
    handleExpiredSession(response.returnCode);
    HsmExpectSuccess(response, "ERP_UnwrapPseudonameKey failed", timer);
    return createErpArray<Aes256Length>(reinterpret_cast<const uint8_t*>(response.Key));
}

ErpBlob HsmProductionClient::getTeeToken(
    const HsmRawSession& session,
    TeeTokenRequestInput&& input)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryHsm, "Hsm:ERP_GetTEEToken");

    hsmclient::TEETokenRequestInput requestInput;
    setInput<TpmObjectNameLength>(requestInput.AKName, input.akName);
    setInput(requestInput.KnownAKBlob, input.knownAkBlob);
    setInput(requestInput.KnownQuoteBlob, input.knownQuoteBlob);
    setInput(requestInput.NONCEBlob, input.nonceBlob);
    setInput(requestInput.QuoteDataLength, input.quoteData);
    setInput(requestInput.QuoteData, input.quoteData);
    setInput(requestInput.QuoteSignatureLength, input.quoteSignature);
    setInput(requestInput.QuoteSignature, input.quoteSignature);

    const hsmclient::SingleBlobOutput response = hsmclient::ERP_GetTEEToken(
        session.rawSession,
        requestInput);
        handleExpiredSession(response.returnCode);

    HsmExpectSuccess(response, "ERP_GetTEEToken failed", timer);

    return convertErpBlob(response.BlobOut);
}


DeriveKeyOutput HsmProductionClient::deriveTaskKey(
    const HsmRawSession& session,
    DeriveKeyInput&& input)
{
    return derivePersistenceKey (
        session,
        std::move(input),
        "Hsm:ERP_DeriveTaskKey",
        hsmclient::ERP_DeriveTaskKey);
}


DeriveKeyOutput HsmProductionClient::deriveAuditKey(
    const HsmRawSession& session,
    DeriveKeyInput&& input)
{
    return derivePersistenceKey (
        session,
        std::move(input),
        "Hsm:ERP_DeriveTaskKey(AuditKey)",
        hsmclient::ERP_DeriveTaskKey);
}


DeriveKeyOutput HsmProductionClient::deriveCommsKey(
    const HsmRawSession& session,
    DeriveKeyInput&& input)
{
    return derivePersistenceKey (
        session,
        std::move(input),
        "Hsm:ERP_DeriveCommsKey",
        hsmclient::ERP_DeriveCommsKey);
}

::DeriveKeyOutput HsmProductionClient::deriveChargeItemKey(const ::HsmRawSession& session, ::DeriveKeyInput&& input)
{
    return derivePersistenceKey(session, ::std::move(input), "Hsm:ERP_DeriveChargeItemKey", ::hsmclient::ERP_DeriveChargeItemKey);
}

ErpArray<Aes128Length> HsmProductionClient::doVauEcies128(
    const HsmRawSession& session,
    DoVAUECIESInput&& input)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryHsm, "Hsm:ERP_DoVAUECIES128");

    hsmclient::DoVAUECIESInput requestInput;
    setInput(requestInput.TEEToken, input.teeToken);
    setInput(requestInput.ECIESKeyPair, input.eciesKeyPair);
    setInput(requestInput.clientPublicKeyLength, input.clientPublicKey);
    setInput(requestInput.clientPublicKeyData, input.clientPublicKey);

    const auto response = hsmclient::ERP_DoVAUECIES128(
        session.rawSession,
        requestInput);
    handleExpiredSession(response.returnCode);
    HsmExpectSuccess(response, "ERP_DoVAUECIES128 failed", timer);

    return createErpArray<Aes128Length>(reinterpret_cast<const uint8_t*>(response.AESKey));
}


SafeString HsmProductionClient::getVauSigPrivateKey (
    const HsmRawSession& session,
    GetVauSigPrivateKeyInput&& input)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryHsm, "Hsm:ERP_GetVAUSIGPrivateKey");

    hsmclient::TwoBlobGetKeyInput requestInput;
    setInput(requestInput.TEEToken, input.teeToken);
    setInput(requestInput.Key, input.vauSigPrivateKey);

    auto response = hsmclient::ERP_GetVAUSIGPrivateKey(
        session.rawSession,
        requestInput);
        handleExpiredSession(response.returnCode);

    HsmExpectSuccess(response, "ERP_GetVAUSIGPrivateKey failed", timer);

    return SafeString(gsl::span(response.keyData, response.keyLength));
}


ErpVector HsmProductionClient::getRndBytes(
    const HsmRawSession& session,
    size_t input)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryHsm, "Hsm:ERP_GetRNDBytes");

    const auto response = hsmclient::ERP_GetRNDBytes(
        session.rawSession,
        hsmclient::UIntInput{gsl::narrow<unsigned int>(input)});
        handleExpiredSession(response.returnCode);

    HsmExpectSuccess(response, "ERP_GetRNDBytes failed", timer);

    return createErpVector(response.RNDDataLen, response.RNDData);
}


ErpArray<Aes256Length> HsmProductionClient::unwrapHashKey(
    const HsmRawSession& session,
    UnwrapHashKeyInput&& input)
{
    auto timer = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryHsm, "Hsm:ERP_UnwrapHashKey");

    hsmclient::TwoBlobGetKeyInput requestInput;
    setInput(requestInput.TEEToken, input.teeToken);
    setInput(requestInput.Key, input.key);

    const auto response = hsmclient::ERP_UnwrapHashKey(
        session.rawSession,
        requestInput);
        handleExpiredSession(response.returnCode);

    HsmExpectSuccess(response, "ERP_UnwrapHashKey failed", timer);

    return createErpArray<Aes256Length>(reinterpret_cast<const uint8_t*>(response.Key));
}

::ParsedQuote HsmProductionClient::parseQuote(const ::ErpVector& quote) const
{
    Expect(quote.size() == TPM_QUOTE_LENGTH, "Quote data size is not as expected.");

    auto timer = ::DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryHsm, "Hsm:ERP_ParseTPMQuote");

    ::hsmclient::TPMQuoteInput input;
    ::std::copy(::std::begin(quote), ::std::end(quote), ::std::begin(input.QuoteData));

    const auto parsedQuote = ::hsmclient::ERP_ParseTPMQuote(input);

    HsmExpectSuccess(parsedQuote, "ERP_ParseTPMQuote failed", timer);

    ::ParsedQuote result;
    result.qualifiedSignerName = createErpVector(TPM_NAME_LEN, parsedQuote.qualifiedSignerName);
    result.qualifyingInformation = createErpVector(NONCE_LEN, parsedQuote.qualifyingInformation);
    result.pcrSetFlags = createErpVector(TPM_PCRSET_LENGTH, parsedQuote.PCRSETFlags);
    result.pcrHash = createErpVector(TPM_PCR_DIGESTHASH_LENGTH, parsedQuote.PCRHash);

    return result;
}

void HsmProductionClient::reconnect (HsmRawSession& session)
{
    // Log out.
    disconnect(session);

    // Log back in.
    session = connect(HsmIdentity::getWorkIdentity());
}


HsmRawSession HsmProductionClient::connect (const HsmIdentity& identity)
{

    const std::string hsmDeviceString = Configuration::instance().getStringValue(ConfigurationKey::HSM_DEVICE);
    auto [devices,devicesBuffer] = String::splitIntoNullTerminatedArray(hsmDeviceString, ",");
    const auto connectTimeout = std::chrono::seconds(
        Configuration::instance().getOptionalIntValue(
            ConfigurationKey::HSM_CONNECT_TIMEOUT_SECONDS,
            5));
    const auto readTimeout = std::chrono::seconds(
        Configuration::instance().getOptionalIntValue(
            ConfigurationKey::HSM_READ_TIMEOUT_SECONDS,
            1));
    const auto reconnectInterval = std::chrono::seconds(
        Configuration::instance().getOptionalIntValue(
            ConfigurationKey::HSM_RECONNECT_INTERVAL_SECONDS,
            15));

    TVLOG(1) << "connecting to HSM cluster with " << devices.size()-1 << " devices: " << hsmDeviceString;
    const auto connectedSession =
        hsmclient::ERP_ClusterConnect(
            const_cast<const char**>(devices.data()), // NOLINT(cppcoreguidelines-pro-type-const-cast)
            gsl::narrow_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(connectTimeout).count()),
            gsl::narrow_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(readTimeout).count()),
            gsl::narrow_cast<uint32_t>(std::chrono::duration_cast<std::chrono::seconds>(reconnectInterval).count()));

    if (connectedSession.status != hsmclient::HSMAnonymousOpen)
    {
        Expect(connectedSession.status == hsmclient::HSMAnonymousOpen,
            "could not connect to HSM " + hsmDeviceString
                + " : " + HsmProductionClient::hsmErrorMessage(connectedSession.status, connectedSession.errorCode));
    }
    return logon(connectedSession, identity);
}

HsmRawSession HsmProductionClient::logon(const hsmclient::HSMSession& connectedSession, const HsmIdentity& identity)
{
    HsmRawSession session;
    session.identity = identity.identity;

    if (identity.keyspec.has_value() && identity.keyspec.value().size()>0)
    {
        TVLOG(1) << "hsm keyspec is present, logging in as " << identity.displayName();
        auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryHsm, "Hsm:ERP_LogonKeySpec");

        const auto loggedInSession = ERP_LogonKeySpec(connectedSession, identity.username.c_str(), identity.keyspec.value(), identity.password);
        Expect(loggedInSession.status == hsmclient::HSMSessionStatus::HSMLoggedIn, "login of hsm user " + identity.displayName() + " failed");

        TVLOG(1) << "EnrolmentManagerImplementation: connection to HSM for user " << identity.displayName() << " established";
        session.rawSession = loggedInSession;
    }
    else if (identity.password.size()>0)
    {
        TVLOG(1) << "password is present, logging in as " << identity.displayName();
        auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryHsm, "Hsm:ERP_LogonPassword");

        const auto loggedInSession = ERP_LogonPassword(connectedSession, identity.username.c_str(), identity.password);
        Expect(loggedInSession.status == hsmclient::HSMSessionStatus::HSMLoggedIn, "login of hsm user " + identity.displayName() + " failed");

        TVLOG(1) << "EnrolmentManagerImplementation: connection to HSM for user " << identity.displayName() << " established";
        session.rawSession = loggedInSession;
    }
    else
    {
        Fail("hsm " + identity.displayName() + " has neither password nor keyspec configured");
    }
    return session;
}

void HsmProductionClient::disconnect (HsmRawSession& session)
{
    if (session.rawSession.status == hsmclient::HSMSessionStatus::HSMLoggedIn)
    {
        auto timerKeepAlive = DurationConsumer::getCurrent().getTimer(DurationConsumer::categoryHsm, "Hsm:ERP_Disconnect");
        session.rawSession = ERP_Disconnect(session.rawSession);
    }
}


std::string HsmProductionClient::hsmErrorCodeString (const uint32_t errorCode)
{
    // The error codes have been taken from ERP_Error.h and ERP_MDLError.h
    switch(STRIP_ERR_INDEX(errorCode))
    {
#ifndef __APPLE__
#define caseErrorCode(name) case name: return #name;
        caseErrorCode(ERP_ERR_NOERROR);
        // caseErrorCode(ERP_ERR_SUCCESS); same numerical value (0) as the previous error code.
        caseErrorCode(ERP_ERR_NO_CONNECTION);
        caseErrorCode(ERP_ERR_ALREADY_LOGGED_IN);
        caseErrorCode(ERP_ERR_BAD_CONNECTION);
        caseErrorCode(ERP_ERR_BAD_CREDENTIAL_FORMAT);
        caseErrorCode(ERP_ERR_NOT_LOGGED_IN);
        caseErrorCode(ERP_ERR_RESPONSE_TOO_SHORT);
        caseErrorCode(ERP_ERR_ASN1ENCODING_ERROR);
        caseErrorCode(ERP_ERR_ASN1DECODING_ERROR);
        caseErrorCode(ERP_ERR_RESPONSE_TOO_LONG);
        caseErrorCode(ERP_ERR_CALLOC_ERROR);
        caseErrorCode(ERP_ERR_FREE_ERROR);
        caseErrorCode(ERP_ERR_BAD_RETURN_FORMAT);
        caseErrorCode(ERP_ERR_BUFFER_TOO_SMALL);
        caseErrorCode(ERP_ERR_NOT_IMPLEMENTED);
        caseErrorCode(ERP_ERR_BAD_SESSION_HANDLE);
        caseErrorCode(ERP_ERR_TOO_MANY_SESSIONS);
        caseErrorCode(ERP_ERR_PERMISSION_DENIED);
        caseErrorCode(ERP_ERR_PARAM);
        caseErrorCode(ERP_ERR_PARAM_LEN);
        caseErrorCode(ERP_ERR_MALLOC);
        caseErrorCode(ERP_ERR_MODE);
        caseErrorCode(ERP_ERR_ITEM_NOT_FOUND);
        caseErrorCode(ERP_ERR_MODULE_DEP);
        caseErrorCode(ERP_ERR_FILE_IO);
        caseErrorCode(ERP_ERR_ASN1_PARSE_ERROR);
        caseErrorCode(ERP_ERR_ASN1_CONTENT_ERROR);
        caseErrorCode(ERP_ERR_UNKNOWN_BLOB_GENERATION);
        caseErrorCode(ERP_ERR_NOT_IMPLEMENTED_YET);
        caseErrorCode(ERP_ERR_BAD_BLOB_GENERATION);
        caseErrorCode(ERP_ERR_AES_KEY_ERROR);
        caseErrorCode(ERP_ERR_KEY_USAGE_ERROR);
        caseErrorCode(ERP_ERR_BAD_BLOB_DOMAIN);
        caseErrorCode(ERP_ERR_BAD_BLOB_AD);
        caseErrorCode(ERP_ERR_WRONG_BLOB_TYPE);
        caseErrorCode(ERP_ERR_OBSOLETE_FUNCTION);
        caseErrorCode(ERP_ERR_DEV_FUNCTION_ONLY);
        caseErrorCode(ERP_ERR_MAX_BLOB_GENERATIONS);
        caseErrorCode(ERP_ERR_INTERNAL_BUFFER_ERROR);
        caseErrorCode(ERP_ERR_NO_ECC_DOMAIN_PARAMETERS);
        caseErrorCode(ERP_ERR_FAILED_ECC_KEYPAIR_GENERATION);
        caseErrorCode(ERP_ERR_CERT_BAD_SUBJECT_ALG);
        caseErrorCode(ERP_ERR_CERT_BAD_SIGNATURE_ALG);
        caseErrorCode(ERP_ERR_CERT_BAD_SUBJECT_ENCODING);
        caseErrorCode(ERP_ERR_CERT_UNSUPPORTED_CURVE);
        caseErrorCode(ERP_ERR_CERT_BAD_SIGNATURE_FORMAT);
        caseErrorCode(ERP_ERR_BLOB_EXPIRED);
        caseErrorCode(ERP_ERR_BAD_ANSIX9_62_LENGTH);
        caseErrorCode(ERP_ERR_BAD_TPM_NAME_ALGORITHM);
        caseErrorCode(ERP_ERR_BAD_ANSIX9_62_ENCODING);
        caseErrorCode(ERP_ERR_TPM_NAME_MISMATCH);
        caseErrorCode(ERP_ERR_BAD_TPMT_PUBLIC_LENGTH);
        caseErrorCode(ERP_ERR_BAD_TPMT_PUBLIC_ALGORITHM);
        caseErrorCode(ERP_ERR_BAD_TPMT_PUBLIC_FORMAT);
        caseErrorCode(ERP_ERR_FAIL_AK_CREDENTIAL_MATCH);
        caseErrorCode(ERP_ERR_BAD_BLOB_TIME);
        caseErrorCode(ERP_ERR_TPM_UNSUPPORTED_CURVE);
        caseErrorCode(ERP_ERR_BAD_TPMT_SIGNATURE_LENGTH);
        caseErrorCode(ERP_ERR_BAD_TPMT_SIGNATURE_FORMAT);
        caseErrorCode(ERP_ERR_BAD_TPMT_PUBLIC_ATTRIBUTES);
        caseErrorCode(ERP_ERR_BAD_QUOTE_HEADER);
        caseErrorCode(ERP_ERR_BAD_QUOTE_LENGTH);
        caseErrorCode(ERP_ERR_QUOTE_NONCE_MISMATCH);
        caseErrorCode(ERP_ERR_BAD_QUOTE_FORMAT);
        caseErrorCode(ERP_ERR_BAD_QUOTE_HASH_FORMAT);
        caseErrorCode(ERP_ERR_QUOTE_SIGNER_MISMATCH);
        caseErrorCode(ERP_ERR_QUOTE_PCRSET_MISMATCH);
        caseErrorCode(ERP_ERR_QUOTE_DIGEST_MISMATCH);
        caseErrorCode(ERP_ERR_ECIES_CURVE_MISMATCH);
        caseErrorCode(ERP_ERR_CSR_ADMISSIONS_MISMATCH);
        caseErrorCode(ERP_ERR_BLOB_SEMAPHORE_DEADLOCK);
        caseErrorCode(ERP_ERR_DERIVATION_DATA_LENGTH);
        caseErrorCode(E_ASN1_MEM);
        caseErrorCode(E_ASN1_FLAG);
        caseErrorCode(E_ASN1_TAB_OVL);
        caseErrorCode(E_ASN1_BAD_ZKA);
        caseErrorCode(E_ASN1_DATASIZE);
        caseErrorCode(E_ASN1_TAGSIZE);
        caseErrorCode(E_ASN1_INDEF_LEN);
        caseErrorCode(E_ASN1_LENSIZE);
        caseErrorCode(E_ASN1_STACK_OVL);
        caseErrorCode(E_ASN1_NOT_FOUND);
        caseErrorCode(E_ASN1_BUFF_OVL);
        caseErrorCode(E_ASN1_ITEMCOUNT);
        caseErrorCode(E_ASN1_BADTAG);
        caseErrorCode(E_ASN1_BAD_PKCS1);
        caseErrorCode(E_ASN1_DECODE_ERR);
        caseErrorCode(E_ASN1_SIZE_EXCEEDED);

        caseErrorCode(ERP_ERR_BAD_DEVICE_SPEC);
        caseErrorCode(ERP_ERR_SET_CLUSTER_FALLBACK);
#undef caseErrorCode
#endif
        // More error codes according to Utimaco's CryptoServer_ErrorReference.pdf
        case ERP_UTIMACO_CONNECTION_TERMIINATED:       return "CryptoServer API LINUX: connection terminated by remote host";
        case ERP_UTIMACO_DECRYPT_FAILED:               return "CryptoServer module AES: Tag verification on CCM/GCM decrypt failed";
        case ERP_UTIMACO_FUNCTION_DOESNT_EXIST:        return "CryptoServer module CMDS, Command scheduler: function doesn’t exist";
        case ERP_UTIMACO_INVALID_HANDLE:               return "CryptoServer API core functions: invalid handle";
        case ERP_UTIMACO_INVALID_MESSAGING_ID:         return "CryptoServer module CMDS, Command scheduler: invalid secure messaging ID";
        case ERP_UTIMACO_INVALID_USER_NAME:            return "CryptoServer module CMDS, Command scheduler: invalid user name";
        case ERP_UTIMACO_MAX_AUTH_FAILURES:            return "CryptoServer module CMDS, Command scheduler: file for MaxAuthFailures corrupted";
        case ERP_UTIMACO_MAX_LOGINS:                   return "CryptoServer module CMDS, Command scheduler: maximum of logged in/authenticated users reached";
        case ERP_UTIMACO_MESSAGING_FAILED:             return "CryptoServer module CMDS, Command scheduler: secure messaging failed";
        case ERP_UTIMACO_SESSION_CLOSED:               return "CryptoServer module CMDS, Command scheduler: secure messaging session is closed";
        case ERP_UTIMACO_SESSION_EXPIRED:              return "CryptoServer module CMDS, Command scheduler: secure messaging session expired";
        case ERP_UTIMACO_SIGNATURE_VERIFICATION:       return "CryptoServer module ECDSA: signature verification failed";
        case ERP_UTIMACO_TIMEOUT_OCCURED:              return "CryptoServer API LINUX: timeout occurred";
        case ERP_UTIMACO_UNKNOWN_USER:                 return "CryptoServer module CMDS, Command scheduler: unknown user";

        default:         return "unknown";
    }
}


std::string HsmProductionClient::hsmErrorIndexString (const uint32_t errorCode)
{
    const uint8_t errorIndex = static_cast<uint8_t>((errorCode & ~ERR_INDEX_MASK) >> 8);
    return ByteHelper::toHex(gsl::span(reinterpret_cast<const char*>(&errorIndex), 1));
}


std::string HsmProductionClient::hsmErrorDetails (const uint32_t errorCode)
{
    std::ostringstream s;
    s << std::hex << errorCode
      << " " + hsmErrorCodeString(errorCode);
    if ((errorCode & ~ERR_INDEX_MASK) != 0)
        s << " index " << hsmErrorIndexString(errorCode);
    else
        s << " without index";
    return s.str();
}


std::string HsmProductionClient::hsmErrorMessage (const size_t status, const uint32_t errorCode)
{
    switch(status)
    {
        case hsmclient::HSMUninitialised:
            return "HSMUninitialised";
        case hsmclient::HSMClosed:
            return "HSMClose";
        case hsmclient::HSMAnonymousOpen:
            return "HSMAnonymousOpen";
        case hsmclient::HSMLoggedIn:
            return "HSMLoggedIn";
        case hsmclient::HSMLoginFailed:
            return "HSMLoginFailed";
        case hsmclient::HSMError:
            return "HSMError: " + hsmErrorDetails(errorCode);

        default:
            return "unknown HSM session status";
    }
}
