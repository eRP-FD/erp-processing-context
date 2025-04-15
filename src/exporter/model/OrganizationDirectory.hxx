/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_ORGANIZATIONDIRECTORY_HXX
#define ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_ORGANIZATIONDIRECTORY_HXX

#include "shared/model/Resource.hxx"

namespace model
{
class TelematikId;
class Coding;

class OrganizationDirectory : public Resource<OrganizationDirectory>
{
public:
    static constexpr auto resourceTypeName = "Organization";
    static constexpr auto profileType = ProfileType::OrganizationDirectory;
    std::optional<Timestamp> getValidationReferenceTimestamp() const override;

    OrganizationDirectory(const TelematikId& telematikId, const Coding& type, const std::string& name,
                          const std::string& professionOid);

private:
    friend Resource<OrganizationDirectory>;
    explicit OrganizationDirectory(NumberAsStringParserDocument&& jsonTree);
};

extern template class Resource<OrganizationDirectory>;

}// model

#endif//ERP_PROCESSING_CONTEXT_SRC_EXPORTER_MODEL_ORGANIZATIONDIRECTORY_HXX
