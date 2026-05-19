/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "FhirPackage.hxx"
#include "fhirtools/converter/internal/FhirJsonToXmlConverter.hxx"
#include "fhirtools/converter/internal/FhirSAXHandler.hxx"
#include "fhirtools/repository/FhirVersion.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/FileHelper.hxx"
#include "tools/fhirinstall/ShowHelp.hxx"
#include "tools/fhirinstall/FhirInstallArgs.hxx"

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <functional>
#include <iostream>
#include <system_error>
#include <utility>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <libxml2/libxml/tree.h>
#include <rapidjson/rapidjson.h>
#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

static inline const rapidjson::Pointer resourceTypePtr{"/resourceType"};
using Json = model::NumberAsStringParserDocument;

std::shared_ptr<FhirPackage> FhirPackage::get(const Id& id)
{
    static PtrSet instances;
    const auto pkg = instances.lower_bound(id);
    if (pkg != instances.end() && (*pkg)->mId == id)
    {
        return *pkg;
    }
    return *instances.insert(pkg, Ptr{new FhirPackage(id)});
}

FhirPackage::Id::Id(std::string_view pkgSpec)
{
    using namespace std::string_literals;
    auto atPos = pkgSpec.rfind('@');
    Expect3(atPos != std::string_view::npos,
            "missing `@` in package: "s.append(pkgSpec).append(" - format is <name>@<version>"), ShowHelp);
    name.assign(pkgSpec.substr(0, atPos));
    version = fhirtools::FhirVersion{std::string{pkgSpec.substr(atPos + 1)}};
}

FhirPackage::Id::Id(std::string initName, fhirtools::FhirVersion initVersion)
    : name{std::move(initName)}
    , version{std::move(initVersion)}
{
}


FhirPackage::FhirPackage(Id id)
    : mId{std::move(id)}
{
}
//NOLINTNEXTLINE(misc-no-recursion)
void FhirPackage::printTree(const PtrSet& packages, size_t indent)
{
    const auto samePackageName =[&](const Ptr& ptr) {
        return ptr->id().name == mId.name;
    };
    Expect3(mPackageInfoLoaded, "Package info not loaded.", std::logic_error);
    std::set<fhirtools::FhirVersion> conflicts;
    for (auto it = packages.lower_bound(Id{mId.name, fhirtools::FhirVersion::notVersioned});
         it != packages.end() && samePackageName(*it); ++it)
    {
        const auto& version = (*it)->id().version;
        if (version != mId.version)
        {
            conflicts.emplace(version);
        }
    }
    if (!conflicts.empty())
    {
        fmt::print(fmt::fg(fmt::color::red), "{:{}} {}@{} conflicts: {}\n", "", indent, mId.name, mId.version, fmt::join(conflicts, ", "));
    }
    else
    {
        fmt::println("{:{}} {}@{}", "", indent, mId.name, mId.version);
    }
    for (const auto& d: dependencies())
    {
        d->printTree(packages, indent + 4);
    }
}

//NOLINTNEXTLINE(misc-no-recursion)
FhirPackage::PtrSet FhirPackage::dependencies(const std::filesystem::path& cacheFolder,
                                        const std::map<std::string, Id>& substitutions, bool recursive)
{
    const std::filesystem::path packageFolder{cacheFolder / (mId.name + '#' + to_string(mId.version))};
    std::filesystem::directory_entry packageFile{packageFolder  / "package" / "package.json"};
    Expect(packageFile.exists(), "packagefile not found: " + packageFile.path().native());
    processPackageInfo(packageFile, substitutions, true);
    auto result = dependencies();
    if (recursive)
    {
        for (const auto& d: dependencies())
        {
            result.merge(d->dependencies(cacheFolder, substitutions, true));
        }
    }
    return result;
}


bool FhirPackage::install(const fhirtools::FhirStructureRepositoryView& view, const FhirInstallArgs& arguments)
{
    const std::filesystem::path packageFolder{arguments.cacheFolder / (mId.name + '#' + to_string(mId.version))};
    const std::filesystem::path excludeFileName{mId.name + '-' + to_string(mId.version) + ".exclude.txt"};
    const std::filesystem::directory_entry src{packageFolder};
    auto excludedFiles = arguments.excludeFileFolder ? readExcludeFile(*arguments.excludeFileFolder / excludeFileName, src)
                                           : std::set<std::filesystem::path>{};
    if (! exists(src))
    {
        LOG(ERROR) << "source package not in cache: " << mId;
        return false;
    }
    installDir(view, src, arguments.outputFolder / (mId.name + '-' + to_string(mId.version)), excludedFiles,
               arguments.substitutions);
    return ! mHadError;
}

//NOLINTNEXTLINE(misc-no-recursion)
size_t FhirPackage::depth() const
{
    if (mDependencies.empty())
    {
        return 0;
    }
    std::size_t depth = 0;
    for (const auto& dep : mDependencies)
    {
        depth = std::max(depth, dep->depth());
    }
    return depth + 1;
}

void FhirPackage::writeConfigAfterInstall(std::ostream& out, const std::filesystem::path& outputFolder) const
{
    auto packageFolder = outputFolder / (mId.name + '-' + to_string(mId.version)) / "package";
    model::NumberAsStringParserDocument doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();
    rapidjson::Value erp{rapidjson::kObjectType};
    rapidjson::Value fhir{rapidjson::kObjectType};
    {
        rapidjson::Value structureFiles{rapidjson::kArrayType};
        structureFiles.PushBack(doc.makeString(packageFolder.string()), alloc);
        fhir.AddMember("structure-files", std::move(structureFiles), alloc);
    }
    rapidjson::Value groupArray{rapidjson::kArrayType};
    rapidjson::Value groupObject{rapidjson::kObjectType};
    groupObject.AddMember("id", doc.makeString(mId.name + "-" + to_string(mId.version)), alloc);
    {
        rapidjson::Value include{rapidjson::kArrayType};
        for (const auto& dep : mDependencies)
        {
            include.PushBack(doc.makeString(dep->mId.name + '-' + to_string(dep->mId.version)), alloc);
        }
        groupObject.AddMember("include", std::move(include), alloc);
    }
    {
        rapidjson::Value matchArray{rapidjson::kArrayType};
        for (const auto& res : mResources)
        {
            auto matchUrl = res.url;
            boost::algorithm::replace_all(matchUrl, ".", R"(\.)");
            boost::algorithm::replace_all(matchUrl, "?", R"(\?)");
            rapidjson::Value matchObject{rapidjson::kObjectType};
            matchObject.AddMember("url", doc.makeString(matchUrl), alloc);
            if (res.version && res.version != fhirtools::FhirVersion::notVersioned)
            {
                matchObject.AddMember("version", doc.makeString(to_string(*res.version)), alloc);
            }
            else
            {
                matchObject.AddMember("version", rapidjson::kNullType, alloc);
            }
            matchArray.PushBack(std::move(matchObject), alloc);
        }
        groupObject.AddMember("match", std::move(matchArray), alloc);
    }
    groupArray.PushBack(std::move(groupObject), alloc);
    fhir.AddMember("resource-groups", groupArray, alloc);
    erp.AddMember("fhir", std::move(fhir), alloc);
    doc.AddMember("erp", std::move(erp), alloc);
    out << doc.serializeToJsonString();
}

FhirPackage::Id FhirPackage::id() const
{
    return mId;
}

const FhirPackage::PtrSet& FhirPackage::dependencies() const
{
    return mDependencies;
}

bool FhirPackage::processEntry(const std::filesystem::directory_entry& src, const rapidjson::Value& entry)
{
    static const std::set<std::string_view> supportedResources{"CodeSystem", "ValueSet", "StructureDefinition"};
    static const rapidjson::Pointer urlPtr{"/url"};
    static const rapidjson::Pointer versionPtr{"/version"};
    static const rapidjson::Pointer kindPtr{"/kind"};

    const auto resourcesType = Json::getOptionalStringValue(entry, resourceTypePtr);
    if (! resourcesType || ! supportedResources.contains(*resourcesType))
    {
        VLOG(1) << "ignoring type: " << resourcesType.value_or("unknown");
        return false;
    }
    auto url = Json::getOptionalStringValue(entry, urlPtr);
    auto version = Json::getOptionalStringValue(entry, versionPtr);
    if (! url)
    {
        LOG(ERROR) << "missing url in: " << src;
        mHadError = true;
        return false;
    }
    if (resourcesType == "StructureDefinition")
    {
        auto kind = Json::getOptionalStringValue(entry, kindPtr);
        if (! kind || kind.value() == "logical")
        {
            VLOG(1) << "ignoring kind: " << kind.value_or("unknown") << " in " << *url << "|"
                    << version.value_or("<none>");
            return false;
        }
    }
    mResources.emplace(std::string{*url},
                       version ? fhirtools::FhirVersion{std::string{*version}} : fhirtools::FhirVersion::notVersioned);
    return true;
}

bool FhirPackage::process(const std::filesystem::directory_entry& src, const model::NumberAsStringParserDocument& doc)
{
    const auto resourcesType = Json::getOptionalStringValue(doc, resourceTypePtr);
    if (resourcesType == "Bundle")
    {
        static const rapidjson::Pointer entryPtr{"/entry"};
        const auto* entries = entryPtr.Get(doc);
        return (entries != nullptr) && entries->IsArray() &&
               std::ranges::any_of(entries->GetArray(), std::bind_front(&FhirPackage::processEntry, this, src));
    }
    return processEntry(src, doc);
}

//NOLINTNEXTLINE(misc-no-recursion)
void FhirPackage::installDir(const fhirtools::FhirStructureRepositoryView& view,
                             const std::filesystem::directory_entry& src, const std::filesystem::path& destFolder,
                             const std::set<std::filesystem::path>& excludedFiles,
                             const std::map<std::string, FhirPackage::Id>& substitutions)
{
    for (const auto& entry : std::filesystem::directory_iterator{src})
    {
        install(view, entry, destFolder, excludedFiles, substitutions);
    }
}

void FhirPackage::convert(const fhirtools::FhirStructureRepositoryView& view,
                          const std::filesystem::directory_entry& src, const std::filesystem::path& destFolder)
{
    model::NumberAsStringParserDocument asJsonDoc;
    try {
        asJsonDoc = Json::fromJson(FileHelper::readFileAsString(src));
    }
    catch (const std::exception& ex)
    {
        LOG(ERROR) << ex.what() << " reading: " << src;
        mHadError = true;
        return;
    }
    if (! process(src, asJsonDoc))
    {
        VLOG(1) << "skipping unneeded: " << src;
        return;
    }
    const std::filesystem::path outname{destFolder / (src.path().stem().string() + ".xml")};
    if (exists(outname))
    {
        VLOG(1) << "skipping existing: " << src;
        return;
    }
    VLOG(0) << "installing converted: " << outname;
    auto asXmlDoc = FhirJsonToXmlConverter::jsonToXml(view, asJsonDoc);
    const auto& nativeOutName = outname.native();
    errno = 0;
    std::error_code ec;
    create_directories(destFolder, ec);
    if (ec)
    {
        LOG(ERROR) << "failed creating target folder: " << destFolder;
        mHadError = true;
        return;
    }
    auto size = xmlSaveFormatFileEnc(nativeOutName.c_str(), asXmlDoc.get(), "utf-8", 1);
    if (size < 0)
    {
        auto err = std::system_category().default_error_condition(errno);
        if (err)
        {
            LOG(ERROR) << err.message() << ": " << outname;
        }
        else
        {
            LOG(ERROR) << "XML document serialization failed." << ": " << outname;
        }
        mHadError = true;
    }
}

//NOLINTNEXTLINE(misc-no-recursion)
void FhirPackage::install(const fhirtools::FhirStructureRepositoryView& view,
                          const std::filesystem::directory_entry& src, const std::filesystem::path& destFolder,
                          const std::set<std::filesystem::path>& excludedFiles,
                          const std::map<std::string, FhirPackage::Id>& substitutions)
{
    if (is_directory(src))
    {
        installDir(view, src, destFolder / src.path().filename(), excludedFiles, substitutions);
        return;
    }
    const auto& srcFilePath = src.path();
    if (excludedFiles.contains(srcFilePath))
    {
        VLOG(1) << "skipping excluded file: " << src;
        return;
    }
    const auto& srcFileName = srcFilePath.filename();
    if (srcFileName == "package.json")
    {
        processPackageInfo(src, substitutions);
        return;
    }
    if (srcFileName == "fhirpkg.lock.json")
    {
        VLOG(1) << "skipping package.lock: " << src;
        return;
    }
    if (srcFileName.string().starts_with('.'))
    {
        VLOG(1) << "skipping hidden file: " << src;
        return;
    }
    if (! is_regular_file(src))
    {
        LOG(ERROR) << "not a regular file: " << src;
        mHadError = true;
        return;
    }
    if (srcFilePath.extension() == ".json")
    {
        convert(view, src, destFolder);
        return;
    }
    if (srcFilePath.extension() != ".xml")
    {
        VLOG(1) << "skipping unknown type: " << src;
        return;
    }

    auto json = FhirSaxHandler::parseXMLintoJSON(view, FileHelper::readFileAsString(src), nullptr);
    if (! process(src, json))
    {
        VLOG(1) << "skipping unneeded: " << src;
        return;
    }
    std::error_code ec;
    create_directories(destFolder, ec);
    if (ec)
    {
        LOG(ERROR) << "failed creating target folder: " << destFolder;
        mHadError = true;
        return;
    }
    copy(src.path(), destFolder, ec);
    if (ec == std::errc::file_exists)
    {
        VLOG(1) << "skipping existing: " << src;
        return;
    }
    if (ec)
    {
        LOG(ERROR) << "copy failed: " << src;
        mHadError = true;
        return;
    }
    VLOG(0) << "copied: " << destFolder / src.path().native();
}

void FhirPackage::processPackageInfo(const std::filesystem::directory_entry& src,
                                     const std::map<std::string, FhirPackage::Id>& substitutions, bool quiet)
{
    if (mPackageInfoLoaded)
    {
        return;
    }
    if (!quiet)
    {
        LOG(INFO) << "reading package info: " << src;
    }
    auto pkgInfo = Json::fromJson(FileHelper::readFileAsString(src));
    static const rapidjson::Pointer dependenciesPtr{"/dependencies"};
    const auto* const dependencies = dependenciesPtr.Get(pkgInfo);
    if ((dependencies == nullptr) || ! dependencies->IsObject())
    {
        LOG(ERROR) << "dependencies not found or not an array: " << src;
        mHadError = true;
        return;
    }
    for (const auto& member : dependencies->GetObject())
    {
        std::string depName{member.name.GetString()};
        if (! member.value.IsString())
        {
            LOG(ERROR) << "dependency version of " << depName << " is not a string: " << src;
            mHadError = true;
            return;
        }
        fhirtools::FhirVersion depVersion{std::string{Json::valueAsString(member.value)}};
        if (depName == "hl7.fhir.r4.core" || depName == "kbv.all.st")
        {
            // don't install fhir core or KBV-Schlüsseltabellen
            continue;
        }
        Id depId = applySubstitutions(substitutions, Id{std::move(depName), std::move(depVersion)}, quiet);
        const auto& depPkg = mDependencies.emplace(get(depId));
        if (!quiet)
        {
            LOG(INFO) << mId << " depends on: " << (*depPkg.first)->id();
        }
    }
    mPackageInfoLoaded = true;
}

FhirPackage::Id FhirPackage::applySubstitutions(const std::map<std::string, FhirPackage::Id>& substitutions, const Id& id, bool quiet)
{
    auto subst = substitutions.find(to_string(id));
    if (subst == substitutions.end())
    {
        subst = substitutions.find(id.name);
    }
    if (subst != substitutions.end())
    {
        if (!quiet)
        {
            LOG(INFO) << mId << ": substituting dependency " << id << " with " << subst->second;
        }
        return subst->second;
    }
    return id;
}

std::set<std::filesystem::path> FhirPackage::readExcludeFile(const std::filesystem::path& excludeFile,
                                                             const std::filesystem::path& srcFolder)
{
    std::ifstream excludes;
    excludes.open(excludeFile);
    if (excludes.fail())
    {
        std::error_code ec(errno, std::system_category());
        if (ec == std::errc::no_such_file_or_directory)
        {
            LOG(INFO) << "no exclude file for package: " << mId;
            VLOG(0) << "    exclude file name is: " << excludeFile;
            return {};
        }
        mHadError = true;
        LOG(ERROR) << ec.message() << ": " << excludeFile;
        return {};
    }
    std::set<std::filesystem::path> result;
    std::string line;
    while (getline(excludes, line))
    {
        boost::algorithm::trim(line);
        if (line.empty() || line.starts_with('#'))
        {
            continue;
        }
        std::filesystem::path path{std::move(line)};
        if (path.is_absolute())
        {
            result.emplace(std::move(path));
        }
        else
        {
            result.emplace(srcFolder / path);
        }
    }
    if (excludes.bad())
    {
        std::error_code ec(errno, std::system_category());
        mHadError = true;
        LOG(ERROR) << ec.message() << ": " << excludeFile;
        return {};
    }
    return result;
}


std::ostream& operator<<(std::ostream& out, const FhirPackage::Id& id)
{
    std::ostream xout{out.rdbuf()};
    xout << id.name << '@' << id.version;
    return out;
}

std::string to_string(const FhirPackage::Id& id)
{
    std::string result;
    const auto& versionStr = to_string(id.version);
    result.reserve(id.name.size() + versionStr.size() + 1);
    result.append(id.name).append(1, '@').append(versionStr);
    return result;
}
