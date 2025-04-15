// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
//
// non-exclusively licensed to gematik GmbH

#include "fhirtools/repository/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/FhirResourceViewGroupSet.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "test/fhirtools/DefaultFhirStructureRepository.hxx"
#include "test/util/ResourceManager.hxx"

using fhirtools::FhirStructureRepository;

std::list<std::filesystem::path> DefaultFhirStructureRepository::defaultProfileFiles()
{
    return {
        // clang-format off
        "resources/fhir/hl7.org/profiles-types.xml",
        "resources/fhir/hl7.org/profiles-resources.xml",
        "resources/fhir/hl7.org/extension-definitions.xml",
        "resources/fhir/hl7.org/valuesets.xml",
        "resources/fhir/hl7.org/v3-codesystems.xml",
        // clang-format on
    };
}


const std::shared_ptr<const fhirtools::FhirStructureRepository>& DefaultFhirStructureRepository::get()
{
    static const fhirtools::FhirResourceGroupConst resourceGroupResolver{"test"};
    static auto repo = create(defaultProfileFiles(), resourceGroupResolver);
    static std::shared_ptr<const fhirtools::FhirStructureRepository> view =
        std::make_shared<const fhirtools::FhirResourceViewGroupSet>("test", resourceGroupResolver.findGroupById("test"),
                                                                    repo.get());
    return view;
}

const std::shared_ptr<const fhirtools::FhirStructureRepository>& DefaultFhirStructureRepository::getWithTest()
{
    static const fhirtools::FhirResourceGroupConst resourceGroupResolver{"test"};
    auto getProfileList = [] {
        auto profileList = defaultProfileFiles();
        profileList.emplace_back(ResourceManager::getAbsoluteFilename("test/fhir-path/structure-definition.xml"));
        return profileList;
    };
    static auto repo = create(getProfileList(), resourceGroupResolver);
    static std::shared_ptr<const fhirtools::FhirStructureRepository> view =
        std::make_shared<const fhirtools::FhirResourceViewGroupSet>("test", resourceGroupResolver.findGroupById("test"),
                                                                    repo.get());
    return view;
}

std::unique_ptr<const fhirtools::FhirStructureRepositoryBackend>
DefaultFhirStructureRepository::create(const std::list<std::filesystem::path>& files,
                                       const fhirtools::FhirResourceGroupResolver& resolver)
{
    auto repo = std::make_unique<fhirtools::FhirStructureRepositoryBackend>();
    repo->load(files, resolver);
    return repo;
}
