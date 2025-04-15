/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef FHIRTOOLS_MOCKFHIRRESOURCEGROUP_HXX
#define FHIRTOOLS_MOCKFHIRRESOURCEGROUP_HXX

#include "fhirtools/repository/FhirResourceGroup.hxx"

#include <gmock/gmock.h>


class MockResourceGroup : public fhirtools::FhirResourceGroup
{
public:
    MOCK_METHOD(std::string_view, id, (), (const, override));
    MOCK_METHOD(std::optional<fhirtools::FhirVersion>, findVersionLocal, (const std::string& url), (const override));
    MOCK_METHOD((std::pair<std::optional<fhirtools::FhirVersion>, std::shared_ptr<const FhirResourceGroup>>),
                findVersion, (const std::string& url), (const, override));
};

class MockResourceGroupResolver : public fhirtools::FhirResourceGroupResolver
{
public:
    MOCK_METHOD((std::map<std::string, std::shared_ptr<const fhirtools::FhirResourceGroup>>), allGroups, (),
                (const override));
    MOCK_METHOD((std::shared_ptr<const fhirtools::FhirResourceGroup>), findGroup,
                (const std::string&, const fhirtools::FhirVersion&, const std::filesystem::path&), (const override));
    MOCK_METHOD((std::shared_ptr<const fhirtools::FhirResourceGroup>), findGroupById, (const std::string&),
                (const override));
};


#endif// FHIRTOOLS_MOCKFHIRRESOURCEGROUP_HXX
