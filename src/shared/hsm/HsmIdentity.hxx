/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_PROCESSING_CONTEXT_HSMIDENTITY_HXX
#define ERP_PROCESSING_CONTEXT_HSMIDENTITY_HXX

#include "shared/util/SafeString.hxx"

#include <optional>

class HsmIdentity
{
public:
    enum class Identity
    {
        Work,
        Setup
    };

    const std::string identityName;
    const Identity identity;
    const std::string username;
    const SafeString password;
    const std::optional<SafeString> keyspec;

    static HsmIdentity getWorkIdentity (void);
    static HsmIdentity getSetupIdentity (void);
    static HsmIdentity getIdentity (Identity identity);

    std::string displayName (void) const;
};


#endif
