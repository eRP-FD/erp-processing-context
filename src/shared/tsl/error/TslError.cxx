/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/tsl/error/TslError.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/JsonLog.hxx"

#include <vector>

#define CASE_RETURN_STRING(value) case value: return #value;

namespace
{
    std::vector<TslError::ErrorData> generateErrorStack(TslError::ErrorData errorData, const TslError& previous)
    {
        std::vector<TslError::ErrorData> result;

        result.emplace_back(std::move(errorData));
        result.insert(result.end(), previous.getErrorData().begin(), previous.getErrorData().end());

        return result;
    }


    std::string to_string(const TslMode tslMode)
    {
        switch (tslMode)
        {
            CASE_RETURN_STRING(TSL)
            CASE_RETURN_STRING(BNA)
        }

        Fail("unexpected TslMode " + std::to_string(tslMode));
    }


    std::string to_string(const TslErrorCode errorCode)
    {
        switch (errorCode)
        {
            CASE_RETURN_STRING(UNKNOWN_ERROR)
            CASE_RETURN_STRING(TSL_INIT_ERROR)
            CASE_RETURN_STRING(TSL_CERT_EXTRACTION_ERROR)
            CASE_RETURN_STRING(MULTIPLE_TRUST_ANCHOR)
            CASE_RETURN_STRING(TSL_SIG_CERT_EXTRACTION_ERROR)
            CASE_RETURN_STRING(TSL_DOWNLOAD_ADDRESS_ERROR)
            CASE_RETURN_STRING(TSL_DOWNLOAD_ERROR)
            CASE_RETURN_STRING(TSL_ID_INCORRECT)
            CASE_RETURN_STRING(VALIDITY_WARNING_2)
            CASE_RETURN_STRING(TSL_NOT_WELLFORMED)
            CASE_RETURN_STRING(TSL_SCHEMA_NOT_VALID)
            CASE_RETURN_STRING(XML_SIGNATURE_ERROR)
            CASE_RETURN_STRING(WRONG_KEYUSAGE)
            CASE_RETURN_STRING(WRONG_EXTENDEDKEYUSAGE)
            CASE_RETURN_STRING(CERT_TYPE_MISMATCH)
            CASE_RETURN_STRING(CERT_READ_ERROR)
            CASE_RETURN_STRING(CERTIFICATE_NOT_VALID_TIME)
            CASE_RETURN_STRING(AUTHORITYKEYID_DIFFERENT)
            CASE_RETURN_STRING(CERTIFICATE_NOT_VALID_MATH)
            CASE_RETURN_STRING(SERVICESUPPLYPOINT_MISSING)
            CASE_RETURN_STRING(CA_CERT_MISSING)
            CASE_RETURN_STRING(OCSP_CHECK_REVOCATION_ERROR)
            CASE_RETURN_STRING(OCSP_CERT_MISSING)
            CASE_RETURN_STRING(OCSP_SIGNATURE_ERROR)
            CASE_RETURN_STRING(OCSP_NOT_AVAILABLE)
            CASE_RETURN_STRING(CERT_TYPE_INFO_MISSING)
            CASE_RETURN_STRING(CA_CERTIFICATE_REVOKED_IN_TSL)
            CASE_RETURN_STRING(NO_OCSP_CHECK)
            CASE_RETURN_STRING(CERTHASH_EXTENSION_MISSING)
            CASE_RETURN_STRING(CERTHASH_MISMATCH)
            CASE_RETURN_STRING(TSL_CA_NOT_LOADED)
            CASE_RETURN_STRING(CRL_CHECK_ERROR)
            CASE_RETURN_STRING(CERT_UNKNOWN)
            CASE_RETURN_STRING(CERT_REVOKED)
            CASE_RETURN_STRING(QC_STATEMENT_ERROR)
            CASE_RETURN_STRING(PROVIDED_OCSP_RESPONSE_NOT_VALID)
            CASE_RETURN_STRING(OCSP_NONCE_MISMATCH)
            CASE_RETURN_STRING(ATTR_CERT_MISMATCH)
            CASE_RETURN_STRING(CRL_DOWNLOAD_ERROR)
            CASE_RETURN_STRING(CRL_OUTDATED_ERROR)
            CASE_RETURN_STRING(CRL_SIGNER_CERT_MISSING)
            CASE_RETURN_STRING(CRL_SIGNATURE_ERROR)
            CASE_RETURN_STRING(OCSP_STATUS_ERROR)
            CASE_RETURN_STRING(CA_CERTIFICATE_NOT_QES_QUALIFIED)
            CASE_RETURN_STRING(VL_UPDATE_ERROR)
            CASE_RETURN_STRING(CERT_TYPE_CA_NOT_AUTHORIZED)
            CASE_RETURN_STRING(CA_CERTIFICATE_REVOKED_IN_BNETZA_VL)
            CASE_RETURN_STRING(TSL_CA_UPDATE_WARNING)
        }

        Fail("unexpected TslErrorCode " + std::to_string(errorCode));
    }


    LogId getErrorLogId(const TslErrorCode errorCode)
    {
        switch (errorCode)
        {
            case UNKNOWN_ERROR:
            case TSL_INIT_ERROR:
            case TSL_CERT_EXTRACTION_ERROR:
            case TSL_SIG_CERT_EXTRACTION_ERROR:
            case TSL_DOWNLOAD_ADDRESS_ERROR:
            case TSL_DOWNLOAD_ERROR:
            case TSL_ID_INCORRECT:
            case VALIDITY_WARNING_2:
            case TSL_NOT_WELLFORMED:
            case TSL_SCHEMA_NOT_VALID:
            case XML_SIGNATURE_ERROR:
            case VL_UPDATE_ERROR:
                return LogId::TSL_CRITICAL;

            case MULTIPLE_TRUST_ANCHOR:
            case WRONG_KEYUSAGE:
            case WRONG_EXTENDEDKEYUSAGE:
            case CERT_TYPE_MISMATCH:
            case CERT_READ_ERROR:
            case CERTIFICATE_NOT_VALID_TIME:
            case AUTHORITYKEYID_DIFFERENT:
            case CERTIFICATE_NOT_VALID_MATH:
            case SERVICESUPPLYPOINT_MISSING:
            case CA_CERT_MISSING:
            case OCSP_CHECK_REVOCATION_ERROR:
            case OCSP_CERT_MISSING:
            case OCSP_SIGNATURE_ERROR:
            case OCSP_NOT_AVAILABLE:
            case CERT_TYPE_INFO_MISSING:
            case CA_CERTIFICATE_REVOKED_IN_TSL:
            case NO_OCSP_CHECK:
            case CERTHASH_EXTENSION_MISSING:
            case CERTHASH_MISMATCH:
            case TSL_CA_NOT_LOADED:
            case CRL_CHECK_ERROR:
            case CERT_UNKNOWN:
            case CERT_REVOKED:
            case QC_STATEMENT_ERROR:
            case PROVIDED_OCSP_RESPONSE_NOT_VALID:
            case OCSP_NONCE_MISMATCH:
            case ATTR_CERT_MISMATCH:
            case CRL_DOWNLOAD_ERROR:
            case CRL_OUTDATED_ERROR:
            case CRL_SIGNER_CERT_MISSING:
            case CRL_SIGNATURE_ERROR:
            case OCSP_STATUS_ERROR:
            case CA_CERTIFICATE_NOT_QES_QUALIFIED:
            case CERT_TYPE_CA_NOT_AUTHORIZED:
            case CA_CERTIFICATE_REVOKED_IN_BNETZA_VL:
                return LogId::TSL_ERROR;

            case TSL_CA_UPDATE_WARNING:
                return LogId::TSL_WARNING;
        }

        Fail("unexpected TslErrorCode " + std::to_string(errorCode));
    }

    // GEMREQ-start getErrorDefaultStatus
    HttpStatus getErrorDefaultStatus(const TslErrorCode errorCode)
    {
        switch (errorCode)
        {
            case UNKNOWN_ERROR:
            case TSL_INIT_ERROR:
            case TSL_CERT_EXTRACTION_ERROR:
            case TSL_SIG_CERT_EXTRACTION_ERROR:
            case TSL_DOWNLOAD_ADDRESS_ERROR:
            case TSL_DOWNLOAD_ERROR:
            case MULTIPLE_TRUST_ANCHOR:
            case TSL_ID_INCORRECT:
            case VALIDITY_WARNING_2:
            case TSL_NOT_WELLFORMED:
            case TSL_SCHEMA_NOT_VALID:
            case XML_SIGNATURE_ERROR:
            case VL_UPDATE_ERROR:
            case OCSP_NOT_AVAILABLE:
            case TSL_CA_UPDATE_WARNING:
                return HttpStatus::InternalServerError;

            case WRONG_KEYUSAGE:
            case WRONG_EXTENDEDKEYUSAGE:
            case CERT_TYPE_MISMATCH:
            case CERT_READ_ERROR:
            case CERTIFICATE_NOT_VALID_TIME:
            case AUTHORITYKEYID_DIFFERENT:
            case CERTIFICATE_NOT_VALID_MATH:
            case SERVICESUPPLYPOINT_MISSING:
            case CA_CERT_MISSING:
            case OCSP_CHECK_REVOCATION_ERROR:
            case OCSP_CERT_MISSING:
            case OCSP_SIGNATURE_ERROR:
            case CERT_TYPE_INFO_MISSING:
            case CA_CERTIFICATE_REVOKED_IN_TSL:
            case NO_OCSP_CHECK:
            case CERTHASH_EXTENSION_MISSING:
            case CERTHASH_MISMATCH:
            case TSL_CA_NOT_LOADED:
            case CRL_CHECK_ERROR:
            case CERT_UNKNOWN:
            case CERT_REVOKED:
            case QC_STATEMENT_ERROR:
            case PROVIDED_OCSP_RESPONSE_NOT_VALID:
            case OCSP_NONCE_MISMATCH:
            case ATTR_CERT_MISMATCH:
            case CRL_DOWNLOAD_ERROR:
            case CRL_OUTDATED_ERROR:
            case CRL_SIGNER_CERT_MISSING:
            case CRL_SIGNATURE_ERROR:
            case OCSP_STATUS_ERROR:
            case CA_CERTIFICATE_NOT_QES_QUALIFIED:
            case CERT_TYPE_CA_NOT_AUTHORIZED:
            case CA_CERTIFICATE_REVOKED_IN_BNETZA_VL:
                return HttpStatus::BadRequest;
        }

        Fail("unexpected TslErrorCode " + std::to_string(errorCode));
    }
    // GEMREQ-end getErrorDefaultStatus


    LogId getErrorsLogId(const TslErrorCode errorCode, const std::vector<TslError::ErrorData>& errorData)
    {
        LogId logId = getErrorLogId(errorCode);
        for (const auto& entry : errorData)
        {
            const LogId entryLogId = getErrorLogId(entry.errorCode);
            logId = std::min(logId, entryLogId);
        }

        return logId;
    }


    std::string generateErrorDataLog(const TslErrorCode errorCode, const std::string& message, const bool isLast)
    {
        std::ostringstream log;

        log << R"({"errorCode":")" << to_string(errorCode) << R"(")"
            << R"(,"message":")" << message << R"("})";

        if ( ! isLast)
        {
            log << ",";
        }

        return log.str();
    }


    std::string generateMessage(
        const std::string& message,
        const TslErrorCode errorCode,
        const std::vector<TslError::ErrorData>& errorData = {},
        const std::optional<TslMode>& tslMode = {},
        const std::optional<std::string>& tslId = {},
        const std::optional<std::string>& tslSequenceNumber = {})
    {
        std::ostringstream log;
        log << R"({"id":)" << static_cast<uint32_t>(getErrorsLogId(errorCode, errorData))
            << R"(,"compType":"eRP:PKI")"
            << R"(,"errorData":[)";

        log << generateErrorDataLog(
            errorCode,
            message,
            errorData.empty());
        for (size_t ind = 0; ind < errorData.size(); ind++)
        {
            log << generateErrorDataLog(
                errorData[ind].errorCode,
                errorData[ind].message,
                ind == errorData.size() - 1);
        }
        log << "]";

        if (tslMode.has_value())
        {
            log << R"(,"tslMode": ")" << to_string(*tslMode) << R"(")";
        }

        if (tslId.has_value())
        {
            log << R"(,"tslId": ")" << *tslId << R"(")";
        }

        if (tslSequenceNumber.has_value())
        {
            log << R"(,"tslSequenceNumber": ")" << *tslSequenceNumber << R"(")";
        }

        log << "}";

        return log.str();
    }
}


TslError::TslError(
    std::string message,
    TslErrorCode errorCode)
        : std::runtime_error(generateMessage(message, errorCode))
        , mErrorData({{std::move(message), errorCode}})
        , mTslMode()
        , mTslId()
        , mTslSequenceNumber()
        , mStatus(getErrorDefaultStatus(errorCode))
{
}


TslError::TslError(
    std::string message,
    TslErrorCode errorCode,
    HttpStatus httpStatus)
    : std::runtime_error(generateMessage(message, errorCode))
    , mErrorData({{std::move(message), errorCode}})
    , mTslMode()
    , mTslId()
    , mTslSequenceNumber()
    , mStatus(httpStatus)
{
}


TslError::TslError(
    std::string message,
    TslErrorCode errorCode,
    const TslMode tslMode)
        : std::runtime_error(generateMessage(message, errorCode, {}, tslMode))
        , mErrorData({{std::move(message), errorCode}})
        , mTslMode(tslMode)
        , mTslId()
        , mTslSequenceNumber()
        , mStatus(getErrorDefaultStatus(errorCode))
{
}


TslError::TslError(
    std::string message,
    TslErrorCode errorCode,
    const TslMode tslMode,
    std::optional<std::string> tslId,
    std::string tslSequenceNumber)
        : std::runtime_error(generateMessage(message, errorCode, {}, tslMode, tslId, tslSequenceNumber))
        , mErrorData({{std::move(message), errorCode}})
        , mTslMode(tslMode)
        , mTslId(std::move(tslId))
        , mTslSequenceNumber(std::move(tslSequenceNumber))
        , mStatus(getErrorDefaultStatus(errorCode))
{
}


TslError::TslError(
    std::string message,
    TslErrorCode errorCode,
    const TslError& previous)
        : std::runtime_error(
              generateMessage(
                  message,
                  errorCode,
                  previous.getErrorData(),
                  previous.getTslMode(),
                  previous.getTslId(),
                  previous.getTslSequenceNumber()))
        , mErrorData(generateErrorStack({std::move(message), errorCode}, previous))
        , mTslMode(previous.getTslMode())
        , mTslId(previous.getTslId())
        , mTslSequenceNumber(previous.getTslSequenceNumber())
        , mStatus(getErrorDefaultStatus(errorCode))
{
}


TslError::TslError(const TslError& original, HttpStatus newHttpStatus)
    : std::runtime_error(original.what())
    , mErrorData(original.mErrorData)
    , mTslMode(original.mTslMode)
    , mTslId(original.mTslId)
    , mTslSequenceNumber(original.mTslSequenceNumber)
    , mStatus(newHttpStatus)
{
}


const std::vector<TslError::ErrorData>& TslError::getErrorData() const
{
    return mErrorData;
}


const std::optional<TslMode>& TslError::getTslMode() const
{
    return mTslMode;
}


const std::optional<std::string>& TslError::getTslId() const
{
    return mTslId;
}


const std::optional<std::string>& TslError::getTslSequenceNumber() const
{
    return mTslSequenceNumber;
}


HttpStatus TslError::getHttpStatus() const
{
    return mStatus;
}
