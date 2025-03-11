// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
// non-exclusively licensed to gematik GmbH

#ifndef ERP_PROCESSING_CONTEXT_MODEL_EVDGAHEALTHAPPREQUEST_HXX
#define ERP_PROCESSING_CONTEXT_MODEL_EVDGAHEALTHAPPREQUEST_HXX

#include "Pzn.hxx"
#include "shared/model/Resource.hxx"

namespace model
{

class EvdgaHealthAppRequest : public Resource<EvdgaHealthAppRequest>
{
public:
    static constexpr auto profileType = ProfileType::KBV_PR_EVDGA_HealthAppRequest;
    static constexpr auto resourceTypeName = "DeviceRequest";

    [[nodiscard]] Pzn getPzn() const;
    [[nodiscard]] Timestamp authoredOn() const;

private:
    friend Resource<EvdgaHealthAppRequest>;
    explicit EvdgaHealthAppRequest(NumberAsStringParserDocument&& document);
};

// NOLINTNEXTLINE(bugprone-exception-escape)
extern template class Resource<EvdgaHealthAppRequest>;
}// model

#endif//EVDGAHEALTHAPPREQUEST_HXX
