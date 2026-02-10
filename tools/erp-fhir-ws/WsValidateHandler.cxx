/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "WsValidateHandler.hxx"

#include "ErpFhirWS.hxx"
#include "WSViewService.hxx"

#include "fhirtools/converter/FhirConverter.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/util/SaxHandler.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/util/UrlHelper.hxx"
#include "test/util/StaticData.hxx"
#include "test/util/TestUtils.hxx"


namespace erp_fhir_ws
{

WsValidateHandler::WsValidateHandler() = default;


model::NumberAsStringParserDocument WsValidateHandler::handleRequest(boost::asio::execution_context& context,
                                                                     const std::string& query, const std::string& body)
{
    WsValidateHandler handler{};
    handler.handleRequestInternal(use_service<WSViewService>(context), query, body);
    if (! handler.mResult)
    {
        handler.mResult.emplace(model::OperationOutcome::Issue{
            .severity = model::OperationOutcome::Issue::Severity::information,
            .code = model::OperationOutcome::Issue::Type::informational,
            .detailsText = "FHIR-Validation successul.",
            .detailsCodings = {},
            .diagnostics = "",
            .expression = {},
        });
    }
    return std::move(*handler.mResult).jsonDocument();
}

void WsValidateHandler::recordIssue(model::OperationOutcome::Issue issue)
{
    if (mResult)
    {
        mResult->addIssue(std::move(issue));
    }
    else
    {
        mResult.emplace(std::move(issue));
    }
}

void WsValidateHandler::recordResults(const fhirtools::ValidationResults& validationResults)
{
    for (const auto& err : validationResults.results())
    {
        model::OperationOutcome::Issue issue{};
        switch (err.severity())
        {
            case fhirtools::Severity::debug:
                continue;
            case fhirtools::Severity::info:
                issue.severity = model::OperationOutcome::Issue::Severity::information;
                break;
            case fhirtools::Severity::warning:
                issue.severity = model::OperationOutcome::Issue::Severity::warning;
                break;
            case fhirtools::Severity::error:
                issue.severity = model::OperationOutcome::Issue::Severity::error;
                break;
        }
        if (const auto* constraint = std::get_if<fhirtools::FhirConstraint>(&err.reason);
            constraint && constraint->getKey() == "dom-6" && constraint->getExpression() == "text.`div`.exists()" &&
            constraint->getSeverity() == fhirtools::Severity::warning)
        {
            issue.severity = model::OperationOutcome::Issue::Severity::information;
        }
        issue.detailsText = err.text();
        if (err.profile)
        {
            issue.diagnostics = "from profile " + to_string(err.profile->key());
        }
        issue.expression.push_back(err.fieldName);
        recordIssue(std::move(issue));
    }
}


std::chrono::days WsValidateHandler::getValidationTimeOffset(const Parameters& queryParams)
{
    auto dateParam = std::ranges::find(queryParams, "date", &Parameters::value_type::first);
    auto now = model::Timestamp::now();
    auto validationDate = now;
    if (dateParam != queryParams.end())
    {
        try
        {
            validationDate = model::Timestamp::fromGermanDate(dateParam->second);
        }
        catch (const std::exception& ex)
        {
            Fail2(std::string{ex.what()} + ": " + dateParam->second, ErpFhirWsException);
        }
    }
    return floor<std::chrono::days>(now - validationDate);
}


model::NumberAsStringParserDocument erp_fhir_ws::WsValidateHandler::parseBody(const std::string& body)
{
    const auto& fhirInstance = Fhir::instance();
    auto startPos = body.find_first_not_of(" \t\n\v");
    ModelExpect(startPos != std::string::npos, "Document start not found");
    if (body[startPos] == '<')
    {
        auto fhirSchemaValidationContext = StaticData::getXmlValidator()->getSchemaValidationContext(SchemaType::fhir);
        Expect3(fhirSchemaValidationContext != nullptr, "Failed to get validation context for XML", std::logic_error);
        fhirtools::SaxHandler{}.validateStringView(body, *fhirSchemaValidationContext);
        return fhirInstance.converter().xmlStringToJson(body);
    }
    if (body[startPos] == '{')
    {
        auto result = model::NumberAsStringParserDocument::fromJson(body);
        StaticData::getJsonValidator()->validate(
            model::NumberAsStringParserDocumentConverter::copyToOriginalFormat(result), SchemaType::fhir);
        return result;
    }
    ModelFail("Document type must be JSON or XML.");
}


std::unique_ptr<model::FhirResourceBase>
erp_fhir_ws::WsValidateHandler::tryCreateResource(const model::NumberAsStringParserDocument& doc)
{
    try
    {
        model::NumberAsStringParserDocument tmp;
        tmp.CopyFrom(doc, tmp.GetAllocator());
        return testutils::createResourceNoValidation(std::move(tmp));
    }
    catch (const testutils::UnsupportedResourceTypeException& ex)
    {
        recordIssue({
            .severity = model::OperationOutcome::Issue::Severity::information,
            .code = model::OperationOutcome::Issue::Type::not_supported,
            .detailsText = "additional validation not supported",
            .detailsCodings = {},
            .diagnostics = ex.what(),
            .expression = {},
        });
    }
    return nullptr;
}


void erp_fhir_ws::WsValidateHandler::validateResource(const WSViewService::View& view,
                                                      const model::FhirResourceBase& fhirResource, std::chrono::days offset)
{
    auto refTime = fhirResource.getValidationReferenceTimestamp();
    if (refTime)
    {
        if ((view.start && refTime < (*view.start + offset)) || (view.end && refTime > (*view.end + offset)))
        {
            std::ostringstream diag;
            diag << "view " << view.id << " is valid";
            if (view.start)
            {
                diag << " from " << (*view.start + offset).toGermanDate();
            }
            if (view.end)
            {
                diag << " until " <<(*view.end + offset).toGermanDate();
            }
            diag << ", but reference timestamp is " << refTime->toGermanDate();
            recordIssue({
                .severity = model::OperationOutcome::Issue::Severity::error,
                .code = model::OperationOutcome::Issue::Type::invalid,
                .detailsText = "seleted view not valid",
                .detailsCodings = {},
                .diagnostics = std::move(diag).str(),
                .expression = {},
            });
        }
    }
    fhirResource.additionalValidation();
}

model::ProfileType WsValidateHandler::getProfileType(const ErpElement& erpElement)
{
    for (const auto& profileUrl : erpElement.profiles())
    {
        if (const auto type = model::findProfileType(profileUrl); type && type != model::ProfileType::fhir)
        {
            return *type;
        }
    }
    return model::ProfileType::fhir;
}

void WsValidateHandler::handleRequestInternal(WSViewService& viewService, const std::string& query,
                                              const std::string& body)
{
    const rapidjson::Pointer resourceTypePtr{
        model::resource::ElementName::path(model::resource::elements::resourceType)};
    std::optional<model::OperationOutcome> result;
    try
    {
        auto queryParams = UrlHelper::splitQuery(query);
        const auto& fhirInstance = Fhir::instance();
        auto offset = getValidationTimeOffset(queryParams);
        const auto now = model::Timestamp::now();
        TVLOG(1) << "Validation Time Offset: " << offset;
        auto viewParam = std::ranges::find(queryParams, "view", &Parameters::value_type::first);
        Expect3(viewParam != queryParams.end(), "Missing query parameter `view`.", ErpFhirWsException);
        const auto& view = viewService.getView(UrlHelper::unescapeUrl(viewParam->second));
        testutils::ShiftFhirResourceViewsGuard g{offset};
        model::NumberAsStringParserDocument doc = parseBody(body);
        const std::string resourceType{doc.getStringValueFromPointer(resourceTypePtr)};
        auto erpElement = std::make_shared<ErpElement>(std::addressof(fhirInstance.backend()),
                                                       std::weak_ptr<ErpElement>{}, resourceType, &doc);
        auto profileType = getProfileType(*erpElement);
        auto resource = tryCreateResource(doc);
        auto refTime = resource ? resource->getValidationReferenceTimestamp().value_or(now) : now;
        recordResults(fhirtools::FhirPathValidator::validate(
            view.view, erpElement, resourceType, fhirInstance.defaultValidatorOptions(profileType, refTime)));
        if (resource)
        {
            validateResource(view, *resource, offset);
        }
    }
    catch (const model::ModelException& ex)
    {

        recordIssue({
            .severity = model::OperationOutcome::Issue::Severity::fatal,
            .code = model::OperationOutcome::Issue::Type::invalid,
            .detailsText = ex.what(),
            .detailsCodings = {},
            .diagnostics = std::nullopt,
            .expression = {},
        });
    }
    catch (const ErpException& ex)
    {
        recordIssue(model::OperationOutcome::Issue{
            .severity = model::OperationOutcome::Issue::Severity::error,
            .code = model::OperationOutcome::Issue::Type::invalid,
            .detailsText = ex.what(),
            .detailsCodings = {},
            .diagnostics = ex.diagnostics(),
            .expression = {},
        });
    }
}

}
