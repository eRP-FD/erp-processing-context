/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2024
 * (C) Copyright IBM Corp. 2021, 2024
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/CommunicationPayload.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/util/Expect.hxx"
#include "erp/util/UrlHelper.hxx"
#include "erp/validation/JsonValidator.hxx"

#include <boost/url.hpp>
#include <rapidjson/pointer.h>


using namespace model;
using namespace model::resource;
namespace rj = rapidjson;

namespace
{
const rj::Pointer pickUpCodeHRPointer("/pickUpCodeHR");
const rj::Pointer pickUpCodeDMCPointer("/pickUpCodeDMC");
const rj::Pointer supplyOptionsTypePointer("/supplyOptionsType");
const rj::Pointer urlPointer("/url");
}// namespace


CommunicationPayload::CommunicationPayload(const rj::Value* payloadValue)
{
    if (payloadValue != nullptr)
    {
        ModelExpect(payloadValue->IsArray(), "Element 'payload' must be array.");

        const auto& array = payloadValue->GetArray();

        // see https://dth01.ibmgcloud.net/jira/browse/ERP-5092
        ModelExpect(array.Size() == 1, "Must be exactly one payload item.");

        const auto* object = array.Begin();
        mPayloadValue = rj::Pointer(ElementName::path("contentString")).Get(*object);

        ModelExpect(mPayloadValue != nullptr, "Payload type must be 'contentString'.");
        mLength = NumberAsStringParserDocument::getStringValueFromValue(mPayloadValue).length();
    }
}

void CommunicationPayload::verifyLength() const
{
    ModelExpect(mLength <= maxPayloadSize, "Payload must not exceed " + std::to_string(maxPayloadSize/1000) + " KB.");
}

void CommunicationPayload::validateJsonSchema(const JsonValidator& validator, SchemaType schemaType) const
{
    const auto payload = NumberAsStringParserDocument::getStringValueFromValue(mPayloadValue);
    rj::Document payloadDoc;
    payloadDoc.Parse(payload.data(), payload.size());
    ModelExpect(! payloadDoc.HasParseError(), "Invalid JSON in payload.contentString");
    try
    {
        validator.validate(payloadDoc, schemaType);
    }
    catch (const ErpException& ex)
    {
        if (ex.diagnostics())
        {
            ErpFailWithDiagnostics(
                ex.status(),
                std::string{"Invalid payload: does not conform to expected JSON schema: "}.append(ex.what()),
                ex.diagnostics());
        }
        ErpFail(ex.status(),
                std::string{"Invalid payload: does not conform to expected JSON schema: "}.append(ex.what()));
    }

    if (schemaType == SchemaType::CommunicationReplyPayload)
    {
        if (pickUpCodeHRPointer.Get(payloadDoc) || pickUpCodeDMCPointer.Get(payloadDoc))
        {
            std::string supplyOptionsTypeValue = supplyOptionsTypePointer.Get(payloadDoc)->GetString();
            ModelExpect(supplyOptionsTypeValue == "onPremise", "Invalid payload: Value of 'supplyOptionsType' must be 'onPremise'");
        }
        auto* urlValue = urlPointer.Get(payloadDoc);
        if (urlValue)
        {
            try
            {
                auto urlParts = UrlHelper::parseUrl(urlValue->GetString());
                ModelExpect(!urlParts.mProtocol.empty() && !urlParts.mHost.empty(), "Invalid payload: URL not valid.");
            } catch (const std::runtime_error& e)
            {
                ModelFail("Invalid payload: URL not valid.");
            }
        }
    }
}
