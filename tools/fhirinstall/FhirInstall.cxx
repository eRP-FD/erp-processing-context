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
    : mArguments{args}
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
    if (mArguments.tree)
    {
        return printTree();

    }
    return install();
}

bool FhirInstall::printTree()
{
    for (const auto& p: mArguments.packages)
    {
        auto packages = p->dependencies(mArguments.cacheFolder, mArguments.substitutions, true);
        p->printTree(packages);
    }
    return true;
}

bool FhirInstall::install()
{
    using namespace fhirtools::version_literal;
    // do not install fhir core:
    static const FhirPackage::Id fhirCore("hl7.fhir.r4.core", "4.0.1"_ver);
    FhirPackage::PtrSet installedPackages;
    while (! mArguments.packages.empty())
    {
        auto pack = mArguments.packages.extract(mArguments.packages.begin());
        if (! installedPackages.contains(pack.value()) && pack.value()->id() != fhirCore)
        {
            const bool ok = pack.value()->install(*mView, mArguments);
            mHadError |= !ok;
            auto deps = pack.value()->dependencies();
            mArguments.packages.merge(std::move(deps));
            installedPackages.insert(std::move(pack));
        }
    }
    if (mArguments.configFolder)
    {
        std::filesystem::create_directories(*mArguments.configFolder);
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
    auto configPath = value(mArguments.configFolder) / fileName;
    if (! exists(configPath))
    {
        VLOG(1) << "Writing config: " << configPath;
        std::ofstream configFile{configPath};
        package.writeConfigAfterInstall(configFile, mArguments.outputFolder);
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
