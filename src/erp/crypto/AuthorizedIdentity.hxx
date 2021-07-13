#ifndef E_LIBRARY_CRYPTO_AUTHORIZEDIDENTITY_HXX
#define E_LIBRARY_CRYPTO_AUTHORIZEDIDENTITY_HXX

#include <string>

struct AuthorizedIdentity
{
    enum Type {
        IDENTITY_SUBJECT_ID,
        IDENTITY_ORGANIZATION_ID
    };

    std::string identity;
    Type type;
};

#endif
