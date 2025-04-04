#ifndef ERP_PROCESSING_CONTEXT_UTIL_VERSION_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_VERSION_HXX


#include <list>
#include <string>

/**
 * @brief Supports comparing strings in version order
 *
 * This class recognises '.' and '-' as delimiters and also automatically delimits between numbers and other characters.
 *
 * @note this class makes a best-effort comparison
 *       suffixes like alpha, beta and rc have no sepecial treatment, but are compared alphabetically,
 *       which is actually the correct order.
 */
class Version : public std::string
{
public:
    Version();
    explicit Version(std::string str)
        : std::string{std::move(str)}
    {
    }

    bool operator==(const Version& lhs) const = default;

private:
    class VersionElement;
    std::list<std::list<std::list<VersionElement>>> split() const;

    friend std::strong_ordering operator<=>(const Version& lhs, const Version& rhs);
};

namespace version_literal
{
inline Version operator""_ver(const char* ver, size_t)
{
    return Version{ver};
}
}

template<>
struct std::hash<Version> {
    static constexpr std::hash<std::string> h{};
    std::size_t operator()(const Version& ver) const
    {
        return h(ver);
    }
};

#endif// ERP_PROCESSING_CONTEXT_UTIL_VERSION_HXX
