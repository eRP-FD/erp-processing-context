#include "exporter/model/push/ErrorResponse.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/util/Expect.hxx"

#include <rapidjson/rapidjson.h>
#include <utility>

namespace model
{

ErrorResponse ErrorResponse::parseResponse(const std::string& responseBody)
{
    rapidjson::Document doc;
    doc.Parse(responseBody.c_str());
    const auto& error = doc["error"].GetString();
    return ErrorResponse{error};
}

const std::string& ErrorResponse::getError() const
{
    return mError;
}

ErrorResponse::ErrorResponse(std::string error)
    : mError(std::move(error))
{
}
}