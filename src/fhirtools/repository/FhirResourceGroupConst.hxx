#ifndef FHIRTOOLS_RESOURCEGROUPCONST_HXX
#define FHIRTOOLS_RESOURCEGROUPCONST_HXX

#include "FhirResourceGroup.hxx"

namespace fhirtools
{

class FhirResourceGroupConst : public FhirResourceGroupResolver
{
public:
    FhirResourceGroupConst(std::string groupName);

    std::map<std::string, std::shared_ptr<const FhirResourceGroup>> allGroups() const override;

    std::shared_ptr<const FhirResourceGroup> findGroup(const std::string& url, const FhirVersion& version,
                                                       const std::filesystem::path& sourceFile) const override;

    std::shared_ptr<const FhirResourceGroup> findGroupById(const std::string& id) const override;

    ~FhirResourceGroupConst() override;

private:
    std::shared_ptr<FhirResourceGroup> mGroup;
};

}// namespace fhirtools


#endif// FHIRTOOLS_RESOURCEGROUPCONST_HXX
