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
{
    readArgs(args);
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
    while (!mPackages.empty())
    {
        auto pack = mPackages.extract(mPackages.begin());
        if (! installedPackages.contains(pack.value()) && pack.value()->id() != fhirCore)
        {
            const bool ok = pack.value()->install(*mView, cacheFolder(), outputFolder());
            mHadError |= !ok;
            auto deps = pack.value()->dependencies();
            mPackages.merge(std::move(deps));
            installedPackages.insert(std::move(pack));
        }
    }
    if (mConfigFolder)
    {
        std::filesystem::create_directories(*mConfigFolder);
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
    auto configPath = value(mConfigFolder) / fileName;
    if (! exists(configPath))
    {
        VLOG(1) << "Writing config: " << configPath;
        std::ofstream configFile{configPath};
        package.writeConfigAfterInstall(configFile, value(mOutputFolder));
    }
    else
    {
        VLOG(1) << "skipping existing config: " << configPath;
    }
}


const std::filesystem::path& FhirInstall::cacheFolder() const
{
    return mCacheFolder.value();
}

const std::optional<std::filesystem::path>& FhirInstall::configFolder() const
{
    return mConfigFolder;
}

const std::filesystem::path& FhirInstall::outputFolder() const
{
    return mOutputFolder.value();
}

const fhirtools::FhirStructureRepository& FhirInstall::view() const
{
    return *mView;
}

void FhirInstall::readArgs(const std::span<const char*>& args)
{
    // skip appname:
    auto argPtr = std::ranges::next(args.begin(), 1, args.end());

    for (; argPtr != args.end(); ++argPtr)
    {
        const std::string_view arg{*argPtr};
        if (arg == "-p")
        {
            ++argPtr;
            Expect3(argPtr != args.end(), "missing package_cache_folder argument to `-p`", ShowHelp);
            Expect3(! mCacheFolder.has_value(), "`-p` supported only once", ShowHelp);
            mCacheFolder.emplace(*argPtr);
        }
        else if (arg == "-o")
        {
            ++argPtr;
            Expect3(argPtr != args.end(), "missing output_folder argument to `-o`", ShowHelp);
            Expect3(! mOutputFolder.has_value(), "`-o` supported only once", ShowHelp);
            mOutputFolder.emplace(*argPtr);
        }
        else if (arg == "-c")
        {
            ++argPtr;
            Expect3(argPtr != args.end(), "missing config_folder argument to `-c`", ShowHelp);
            Expect3(! mConfigFolder.has_value(), "`-c` supported only once", ShowHelp);
            mConfigFolder.emplace(*argPtr);
        }
        else if (arg == "--")
        {
            ++argPtr;
            break;
        }
        else if (! arg.starts_with('-'))
        {
            break;
        }
    }
    for (; argPtr != args.end(); ++argPtr)
    {
        const std::string_view arg{*argPtr};
        mPackages.emplace(FhirPackage::get(FhirPackage::Id{arg}));
    }

    Expect3(mCacheFolder.has_value(), "missing package cache folder `-p` argument", ShowHelp);
    Expect3(mOutputFolder.has_value(), "missing output folder `-o` argument", ShowHelp);
    Expect3(! mPackages.empty(), "no pacakges provided", ShowHelp);
}
