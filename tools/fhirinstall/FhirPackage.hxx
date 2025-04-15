/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef ERP_TOOLS_FHIRINSTALL_FHIRPACKAGE_HXX
#define ERP_TOOLS_FHIRINSTALL_FHIRPACKAGE_HXX

#include "tools/fhirinstall/ShowHelp.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/FhirVersion.hxx"

#include <iosfwd>
#include <filesystem>
#include <memory>
#include <rapidjson/fwd.h>
#include <set>
#include <string>

namespace fhirtools
{
class FhirStructureRepository;
}

namespace model
{
class NumberAsStringParserDocument;
}
class FhirInstall;

class FhirPackage
{
public:
    struct Id;
    struct IdCompare;
    using Ptr = std::shared_ptr<FhirPackage>;
    using PtrSet = std::set<Ptr, IdCompare>;

    static Ptr get(const Id& id);

    bool install(const fhirtools::FhirStructureRepository& view, const std::filesystem::path& cacheFolder,
                 const std::filesystem::path& outputFolder);

    size_t depth() const;

    void writeConfigAfterInstall(std::ostream& out, const std::filesystem::path& outputFolder) const;

    Id id() const;
    const PtrSet& dependencies() const;
    const std::set<fhirtools::DefinitionKey>& resources() const;


    struct Id {
        explicit Id(std::string_view pkgSpec);
        explicit Id(std::string initName, fhirtools::FhirVersion initVersion);

        std::string name;
        fhirtools::FhirVersion version = fhirtools::FhirVersion::notVersioned;
        auto operator<=>(const Id&) const = default;
    };

    struct IdCompare {
        using is_transparent = Id;
        bool operator()(const Ptr& lhs, const Ptr& rhs) const
        {
            return (lhs == nullptr && rhs != nullptr) || (rhs && lhs && lhs->mId < rhs->mId);
        }
        bool operator()(const Ptr& lhs, const Id& rhs) const
        {
            return (lhs == nullptr) || (lhs->mId < rhs);
        }
        bool operator()(const Id& lhs, const Ptr& rhs) const
        {
            return (rhs != nullptr) && (lhs < rhs->mId);
        }
    };
    FhirPackage(const FhirPackage&) = delete;
    FhirPackage& operator = (const FhirPackage&) = delete;
    FhirPackage(FhirPackage&&) = delete;
    FhirPackage& operator = (FhirPackage&&) = delete;
    ~FhirPackage() = default;
private:
    explicit FhirPackage(Id id);

    bool processEntry(const std::filesystem::directory_entry& src, const rapidjson::Value& entry);

    bool process(const std::filesystem::directory_entry& src, const model::NumberAsStringParserDocument& doc);

    void installDir(const fhirtools::FhirStructureRepository& view, const std::filesystem::directory_entry& src, const std::filesystem::path& destFolder);

    void convert(const fhirtools::FhirStructureRepository& view, const std::filesystem::directory_entry& src, const std::filesystem::path& destFolder);

    void install(const fhirtools::FhirStructureRepository& view, const std::filesystem::directory_entry& src, const std::filesystem::path& destFolder);

    void processPackageInfo(const std::filesystem::directory_entry& src);

    Id mId;
    PtrSet mDependencies;
    std::set<fhirtools::DefinitionKey> mResources;
    bool mHadError = false;
};


std::ostream& operator<<(std::ostream&, const FhirPackage::Id&);


#endif// ERP_TOOLS_FHIRINSTALL_FHIRPACKAGE_HXX
