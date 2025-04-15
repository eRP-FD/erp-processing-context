/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_TOOLS_FHIRINSTALL_FHIRINSTALL_HXX
#define ERP_TOOLS_FHIRINSTALL_FHIRINSTALL_HXX

#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/FhirResourceGroupConst.hxx"
#include "tools/fhirinstall/FhirPackage.hxx"

#include <filesystem>
#include <optional>
#include <set>
#include <span>
#include <string_view>


class FhirInstall
{
public:
    static bool run(const std::span<const char*>& args);

    const std::filesystem::path& cacheFolder() const;
    const std::optional<std::filesystem::path>& configFolder() const;
    const std::filesystem::path& outputFolder() const;
    const fhirtools::FhirStructureRepository& view() const;

private:
    explicit FhirInstall(const std::span<const char*>& args);
    bool run();
    void installConfig(const FhirPackage& package);

    void readArgs(const std::span<const char*>& args);

    std::optional<std::filesystem::path> mCacheFolder;
    std::optional<std::filesystem::path> mOutputFolder;
    std::optional<std::filesystem::path> mConfigFolder;
    FhirPackage::PtrSet mPackages;
    fhirtools::FhirResourceGroupConst mBaseResourceGroup{"base"};
    fhirtools::FhirStructureRepositoryBackend mBackend;
    std::shared_ptr<const fhirtools::FhirStructureRepository> mView;
    bool mHadError = false;
};


#endif// ERP_TOOLS_FHIRINSTALL_FHIRINSTALL_HXX
