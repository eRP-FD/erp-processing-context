/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#ifndef EPA_LIBRARY_COMMON_CONTENTTYPE_HXX
#define EPA_LIBRARY_COMMON_CONTENTTYPE_HXX


#include <string>
#include <unordered_map>


/**
 * Represent the value a Content-Type header field in way that makes access to its individual parts easy.
 *
 * Note that type and keys are stored and returned in lower-case, as RFC 7231, section 3.1.1.1 specifies that
 * they are to be used case insensitive. With that, only one side in a string compare has to be modified (to lower-case).
 * However, the same section states, that case sensitivity of parameter values depends on the keys. Therefore
 * they are stored as-is. The caller of getParameterValue() has to handle case explicitly.
 */
class ContentType
{
public:
    static ContentType fromString (const std::string& contentTypeValue);

    /**
     * Return the first part of the Content-Type, the actual type and sub type.
     */
    const std::string& getType (void) const;

    /**
     * Return true if the specified parameter was explicitly defined in the Content-Type.
     */
    bool hasParameter (const std::string& key) const;

    /**
     * Return the value of the specified parameter, or the empty string, when the parameter is not part of the
     * Content-Type.
     */
    const std::string& getParameterValue (const std::string& key) const;

private:
    const std::string mType;
    const std::unordered_map<std::string,std::string> mParameters;

    ContentType (
        std::string type,
        std::unordered_map<std::string,std::string> parameters);
};


#endif
