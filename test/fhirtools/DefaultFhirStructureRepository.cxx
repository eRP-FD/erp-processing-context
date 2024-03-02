// (C) Copyright IBM Deutschland GmbH 2021, 2024
// (C) Copyright IBM Corp. 2021, 2024
//
// non-exclusively licensed to gematik GmbH

#include "test/fhirtools/DefaultFhirStructureRepository.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
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


const FhirStructureRepository& DefaultFhirStructureRepository::get()
{
    static std::unique_ptr<const fhirtools::FhirStructureRepository> repo = create(defaultProfileFiles());
    return *repo;
}

const fhirtools::FhirStructureRepository& DefaultFhirStructureRepository::getWithTest()
{
    auto getProfileList = [] {
        auto profileList = defaultProfileFiles();
        profileList.emplace_back(ResourceManager::getAbsoluteFilename("test/fhir-path/structure-definition.xml"));
        return profileList;
    };
    static std::unique_ptr<const fhirtools::FhirStructureRepository> repo = create(getProfileList());
    return *repo;
}

std::unique_ptr<const fhirtools::FhirStructureRepository>
DefaultFhirStructureRepository::create(const std::list<std::filesystem::path>& files)
{
    auto repo = std::make_unique<FhirStructureRepository>();
    repo->load(files);
    return repo;
}
