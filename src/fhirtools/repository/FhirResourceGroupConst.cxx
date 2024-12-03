#include "FhirResourceGroupConst.hxx"
#include "shared/util/TLog.hxx"
#include "fhirtools/FPExpect.hxx"

#include <unordered_map>

namespace fhirtools
{

namespace
{
class ConstGroup final : public FhirResourceGroup, public std::enable_shared_from_this<ConstGroup>
{
public:
    ConstGroup(std::string groupName)
        : mId{std::move(groupName)}
    {
    }
    std::string_view id() const override
    {
        return mId;
    }
    std::pair<std::optional<FhirVersion>, std::shared_ptr<const FhirResourceGroup>>
    findVersion(const std::string& url) const override
    {
        std::pair<std::optional<FhirVersion>, std::shared_ptr<const FhirResourceGroup>> result;
        result.first = findVersionLocal(url);
        result.second = result.first ? shared_from_this() : nullptr;
        return result;
    }

    std::optional<FhirVersion> findVersionLocal(const std::string& url) const override
    {
        if (auto ver = mVersions.find(url); ver != mVersions.end())
        {
            return ver->second;
        }
        return std::nullopt;
    }

    const std::string mId;
    std::unordered_map<std::string, FhirVersion> mVersions;
};

}

FhirResourceGroupConst::FhirResourceGroupConst(std::string groupName)
    : mGroup{std::make_shared<ConstGroup>(std::move(groupName))}
{
}

std::map<std::string, std::shared_ptr<const FhirResourceGroup>> fhirtools::FhirResourceGroupConst::allGroups() const
{
    return {{std::string{mGroup->id()}, mGroup}};
}

FhirResourceGroupConst::~FhirResourceGroupConst() = default;


std::shared_ptr<const FhirResourceGroup>
fhirtools::FhirResourceGroupConst::findGroup(const std::string& url, const FhirVersion& version,
                                             const std::filesystem::path& sourceFile [[maybe_unused]]) const
{
    //NOLINTNEXTLINE(cppcoreguidelines-pro-type-static-cast-downcast)
    if (const auto original = static_cast<ConstGroup*>(mGroup.get())->mVersions.try_emplace(url, version);
        original.second)
    {
        TVLOG(3) << "Added " << url << "|" << version;
    }
    else
    {
        FPExpect3(original.first->second == version,
                  "Ambiguous Versions " + to_string(original.first->second) + " and " + to_string(version) +
                      " for url: " + url,
                  std::logic_error);
    }
    return mGroup;
}

std::shared_ptr<const FhirResourceGroup> fhirtools::FhirResourceGroupConst::findGroupById(const std::string& id) const
{
    return mGroup->id() == id ? mGroup : nullptr;
}

}// namespace fhirtools
