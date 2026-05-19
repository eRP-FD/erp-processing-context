// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#pragma once

#include "shared/model/Resource.hxx"

namespace model
{
class TelematikId;

class ErpTPrescriptionOrganization : public Resource<ErpTPrescriptionOrganization>
{
public:
    ErpTPrescriptionOrganization();
    static constexpr auto resourceTypeName = "Organization";
    static constexpr auto profileType = ProfileType::ERP_TPrescription_Organization;

    void setTelematikId(const rapidjson::Value& telematikIdItem);
    void setTelematikId(const TelematikId& telematikId);
    void setName(std::string_view name);
    void setTelecom(const rapidjson::Value& telecom);
    void setAddress(const rapidjson::Value& address);

private:
    using Resource::Resource;
    friend Resource;
};

extern template class Resource<ErpTPrescriptionOrganization>;

}// model

