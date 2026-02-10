/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#pragma once

#include "WSViewService.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "shared/model/OperationOutcome.hxx"
#include "shared/model/Timestamp.hxx"

#include <concepts>
#include <memory>
#include <optional>
#include <set>
#include <vector>
#include <boost/asio/execution_context.hpp>

class ErpElement;

namespace fhirtools
{
class FhirStructureRepositoryView;
}

namespace erp_fhir_ws
{
class WsValidateHandler
{
public:
    static model::NumberAsStringParserDocument handleRequest(boost::asio::execution_context& context,
                                                             const std::string& query, const std::string& body);

private:
    using Parameters = std::vector<std::pair<std::string, std::string>>;

    WsValidateHandler();

    void handleRequestInternal(WSViewService& viewService, const std::string& query, const std::string& body);

    static std::chrono::days getValidationTimeOffset(const Parameters& queryParams);

    std::unique_ptr<model::FhirResourceBase> tryCreateResource(const model::NumberAsStringParserDocument& doc);
    void validateResource(const WSViewService::View& view, const model::FhirResourceBase& fhirResource, std::chrono::days offset);
    void recordResults(const fhirtools::ValidationResults& validationResults);
    void recordIssue(model::OperationOutcome::Issue issue);
    static model::NumberAsStringParserDocument parseBody(const std::string& body);
    static model::ProfileType getProfileType(const ErpElement& erpElement);



    std::optional<model::OperationOutcome> mResult;
};

}
