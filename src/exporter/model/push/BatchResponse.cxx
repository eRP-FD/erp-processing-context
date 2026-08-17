/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "exporter/model/push/BatchResponse.hxx"
#include "exporter/model/EpaErrorType.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"

#include <rapidjson/rapidjson.h>

namespace model
{

BatchResponse BatchResponse::parseResponse(const std::string& responseBody)
{
    std::vector<Result> results;
    rapidjson::Document doc;
    doc.Parse(responseBody.c_str());
    const auto& resultArray = doc["results"].GetArray();
    for (const auto& result : resultArray)
    {
        results.emplace_back(Result{.id = result["id"].GetString(),
                                    .status = value(magic_enum::enum_cast<Status>(result["status"].GetString())),
                                    .rejected = {},
                                    .error = {}});
        if (result.HasMember("rejected"))
        {
            const auto& rejectedField = result["rejected"];
            if (rejectedField.IsArray())
            {
                const auto& rejectedArray = rejectedField.GetArray();
                for (const auto& rejected : rejectedArray)
                {
                    results.back().rejected.emplace_back(rejected.GetString());
                }
            }
        }
        if (result.HasMember("error"))
        {
            const auto& err = result["error"];
            if (err.IsString())
            {
                results.back().error = result["error"].GetString();
            }
        }
    }
    const auto& summary = doc["summary"].GetObject();
    return BatchResponse{results, summary["total"].GetInt(), summary["successful"].GetInt(), summary["failed"].GetInt(),
                         summary["partial"].GetInt()};
}

BatchResponse::BatchResponse(const std::vector<Result>& results, int total, int successful, int failed, int partial)
    : mResults(results)
    , mTotal(total)
    , mSuccessful(successful)
    , mFailed(failed)
    , mPartial(partial)
{
}

const std::vector<BatchResponse::Result>& BatchResponse::getResults() const
{
    return mResults;
}
int BatchResponse::getTotal() const
{
    return mTotal;
}
int BatchResponse::getSuccessful() const
{
    return mSuccessful;
}
int BatchResponse::getFailed() const
{
    return mFailed;
}
int BatchResponse::getPartial() const
{
    return mPartial;
}

}
