/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2023
 * (C) Copyright IBM Corp. 2021, 2023
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "erp/model/CommunicationPayload.hxx"
#include "erp/model/ResourceNames.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/util/Expect.hxx"

#include <rapidjson/pointer.h>


using namespace model;
using namespace model::resource;
namespace rj = rapidjson;


CommunicationPayload::CommunicationPayload(const rj::Value* payloadValue)
{
    if (payloadValue != nullptr)
    {
        ModelExpect(payloadValue->IsArray(), "Element 'payload' must be array.");

        const auto& array = payloadValue->GetArray();

        // see https://dth01.ibmgcloud.net/jira/browse/ERP-5092
        ModelExpect(array.Size() == 1, "Must be exactly one payload item.");

        const auto* object = array.Begin();
        const auto* itemValue = rj::Pointer(ElementName::path("contentString")).Get(*object);

        ModelExpect(itemValue != nullptr, "Payload type must be 'contentString'.");

        mLength = NumberAsStringParserDocument::getStringValueFromValue(itemValue).length();
    }
}

void CommunicationPayload::verifyLength() const
{
    ModelExpect(mLength <= maxPayloadSize, "Payload must not exceed " + std::to_string(maxPayloadSize/1000) + " KB.");
}
