// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "erp/model/EvdgaHealthAppRequest.hxx"
#include "shared/model/ResourceNames.hxx"

namespace model
{

Pzn EvdgaHealthAppRequest::getPzn() const
{
    static const rapidjson::Pointer pznPointer{resource::ElementName::path(
        resource::elements::codeCodeableConcept, resource::elements::coding, 0, resource::elements::code)};
    const auto pznOpt = getOptionalStringValue(pznPointer);
    ModelExpect(pznOpt, "PZN number not found in KBV_PR_EVDGA_HealthAppRequest");
    return {*pznOpt};
}

Timestamp EvdgaHealthAppRequest::authoredOn() const
{
    static const rapidjson::Pointer authoredOnPointer(resource::ElementName::path("authoredOn"));
    const auto authoredOnStr = getStringValue(authoredOnPointer);
    return Timestamp::fromGermanDate(std::string{authoredOnStr});
}

EvdgaHealthAppRequest::EvdgaHealthAppRequest(NumberAsStringParserDocument&& document)
    : Resource(std::move(document))
{
}

}
