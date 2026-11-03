/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/EpaErrorType.hxx"

namespace
{
const rapidjson::Pointer errorCodePointer{"/errorCode"};
const rapidjson::Pointer errorDetailPointer{"/errorDetail"};
}

namespace model
{

EpaErrorType::EpaErrorType(const NumberAsStringParserDocument& document)
{
    // GetString() would return the internal string prefix ('$') as well
    if (const auto* errorCodeValue = errorCodePointer.Get(document))
    {
        mErrorCode = NumberAsStringParserDocument::getStringValueFromValue(errorCodeValue);
    }
    if (const auto* errorDetailValue = errorDetailPointer.Get(document))
    {
        mErrorDetail = NumberAsStringParserDocument::getStringValueFromValue(errorDetailValue);
    }
}
const std::string& EpaErrorType::getErrorCode() const
{
    return mErrorCode;
}

const std::string& EpaErrorType::getErrorDetail() const
{
    return mErrorDetail;
}

}
