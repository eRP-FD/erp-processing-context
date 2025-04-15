/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
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
    if (const auto* errorCodeValue = errorCodePointer.Get(document))
    {
        mErrorCode = errorCodeValue->GetString();
    }
    if (const auto* errorDetailValue = errorDetailPointer.Get(document))
    {
        mErrorDetail = errorDetailValue->GetString();
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
