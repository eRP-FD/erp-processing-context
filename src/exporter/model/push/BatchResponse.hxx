/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2026
 * (C) Copyright IBM Corp. 2021, 2026
 *
 * non-exclusively licensed to gematik GmbH
 */

#pragma once
#include <optional>
#include <string>
#include <vector>

namespace model
{

class BatchResponse
{
public:
    enum class Status
    {
        success,
        failed,
        partial
    };
    struct Result {
        std::string id;
        Status status;
        std::vector<std::string> rejected;
        std::optional<std::string> error;
    };

    static BatchResponse parseResponse(const std::string& responseBody);

    [[nodiscard]] const std::vector<Result>& getResults() const;
    [[nodiscard]] int getTotal() const;
    [[nodiscard]] int getSuccessful() const;
    [[nodiscard]] int getFailed() const;
    [[nodiscard]] int getPartial() const;

private:
    BatchResponse(const std::vector<Result>& results, int total, int successful, int failed, int partial);

    std::vector<Result> mResults;
    int mTotal;
    int mSuccessful;
    int mFailed;
    int mPartial;
};

}// model
