/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "FhirInstall.hxx"

#include "shared/util/Expect.hxx"
#include "test/util/ResourceManager.hxx"
#include "tools/fhirinstall/ShowHelp.hxx"

#include <boost/format.hpp>
#include <fstream>
#include <utility>

bool FhirInstall::run(const std::span<const char*>& args)
{
    return FhirInstall{args}.run();
}


FhirInstall::FhirInstall(const std::span<const char*>& args)
    : mArgumemts{args}
{
    mBackend.load(
        {
            ResourceManager::getAbsoluteFilename("fhir/hl7.org/profiles-types.xml"),
            ResourceManager::getAbsoluteFilename("fhir/hl7.org/profiles-resources.xml"),
            ResourceManager::getAbsoluteFilename("fhir/hl7.org/extension-definitions.xml"),
            ResourceManager::getAbsoluteFilename("fhir/hl7.org/valuesets.xml"),
            ResourceManager::getAbsoluteFilename("fhir/hl7.org/v3-codesystems.xml"),
        },
        mBaseResourceGroup);
    mView = mBackend.defaultView();
}

bool FhirInstall::run()
{
    using namespace fhirtools::version_literal;
    // do not install fhir core:
    static const FhirPackage::Id fhirCore("hl7.fhir.r4.core", "4.0.1"_ver);
    FhirPackage::PtrSet installedPackages;
    while (! mArgumemts.packages.empty())
    {
        auto pack = mArgumemts.packages.extract(mArgumemts.packages.begin());
        if (! installedPackages.contains(pack.value()) && pack.value()->id() != fhirCore)
        {
            const bool ok = pack.value()->install(*mView, mArgumemts);
            mHadError |= !ok;
            auto deps = pack.value()->dependencies();
            mArgumemts.packages.merge(std::move(deps));
            installedPackages.insert(std::move(pack));
        }
    }
    if (mArgumemts.configFolder)
    {
        std::filesystem::create_directories(*mArgumemts.configFolder);
        for (const auto& package: installedPackages)
        {
            installConfig(*package);
        }
    }
    return ! mHadError;
}

void FhirInstall::installConfig(const FhirPackage& package)
{
    const size_t depth = package.depth();
    const auto& packId = package.id();
    std::string fileName = (boost::format("%02d_%s") % (depth + 2U) % packId.name).str();
    if (packId.version != fhirtools::FhirVersion::notVersioned)
    {
        fileName += '-';
        fileName += to_string(packId.version);
    }
    fileName += ".config.json";
    auto configPath = value(mArgumemts.configFolder) / fileName;
    if (! exists(configPath))
    {
        VLOG(1) << "Writing config: " << configPath;
        std::ofstream configFile{configPath};
        package.writeConfigAfterInstall(configFile, mArgumemts.outputFolder);
    }
    else
    {
        VLOG(1) << "skipping existing config: " << configPath;
    }
}

const fhirtools::FhirStructureRepositoryView& FhirInstall::view() const
{
    return *mView;
}
