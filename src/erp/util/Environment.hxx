#ifndef ERP_PROCESSING_CONTEXT_UTIL_ENVIRONMENT_HXX
#define ERP_PROCESSING_CONTEXT_UTIL_ENVIRONMENT_HXX

#include <optional>
#include <string>

class Environment
{
public:
    /**
     * A low level function that uses secure_getenv when available on the platform and std::getenv otherwise.
     */
    static std::optional<std::string> get(const std::string& variableName);

    /**
     * Do not use this variant unless you have to.
     * Intended for use with SafeString to avoid the excursion via std::string.
     */
    static const char* getCharacterPointer (const std::string& variableName);
    /**
     * A low level function that uses either setenv (linux, mac os) or _putenv_s (windows).
     */
    static void set(const std::string& variableName, const std::string& value);

    /**
     * A low level function that uses either unsetenv (linux, mac os) or _putenv_s (windows) with an empty value string.
     */
    static void unset (const std::string& variableName);

    static std::string getString(const char* envConfigName, const std::string_view& defaultValue);
    static int getInt(const char* envConfigName, const int defaultValue);
    static bool getBool(const char* envConfigName, const bool defaultValue);
};

#endif
