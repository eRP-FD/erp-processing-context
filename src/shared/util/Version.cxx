#include "shared/util/Version.hxx"
#include "shared/util/String.hxx"

#include <charconv>
#include <limits>
#include <tuple>

class Version::VersionElement
{
public:
    int number = std::numeric_limits<int>::max();
    std::string suffix = "\377";
    auto tie() const
    {
        return std::tie(number, suffix);
    }
    auto operator<=>(const VersionElement& rhs) const
    {
        return tie() <=> rhs.tie();
    }
    bool operator==(const VersionElement& rhs) const
    {
        return tie() == rhs.tie();
    }
};

std::strong_ordering operator<=>(const Version& lhs, const Version& rhs)
{
    return lhs.split() <=> rhs.split();
}

Version::Version() = default;

std::list<std::list<std::list<Version::VersionElement>>> Version::split() const
{
    auto isdigit = [](char c) {
        static const auto& classic = std::use_facet<std::ctype<char>>(std::locale::classic());
        return classic.is(std::ctype_base::digit, c);
    };
    std::list<std::list<std::list<VersionElement>>> result;
    for (const auto& dashSplit : String::split(*this, '-'))
    {
        std::list<std::list<VersionElement>> byDot;
        for (const auto& dotSplit : String::split(dashSplit, '.'))
        {
            std::list<VersionElement> elements;
            const char* pos = dotSplit.data();
            const char* const end = pos + dotSplit.size();
            while (pos != end)
            {
                VersionElement elem;
                auto [numEnd, ec] = std::from_chars(pos, end, elem.number);
                pos = std::find_if(numEnd, end, isdigit);
                elem.suffix.assign(numEnd, pos);
                elements.emplace_back(std::move(elem));
            }
            byDot.emplace_back(std::move(elements));
        }
        result.emplace_back(std::move(byDot));
    }
    static const std::list<std::list<VersionElement>> endMark{std::list{VersionElement{}}};
    result.emplace_back(endMark);
    return result;
}
