/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/hsm/HsmIdentity.hxx"

#include "shared/util/Configuration.hxx"
#include "shared/util/Expect.hxx"


/**
 * During a hopefully short transition phase support the old environment/json names.
 * That concerns the ERP_WORK username and password and the ERP_SETUP password (but not its username).
 */


HsmIdentity HsmIdentity::getWorkIdentity (void)
{
    const auto& configuration = Configuration::instance();

    auto username = configuration.getOptionalStringValue(ConfigurationKey::HSM_WORK_USERNAME);
    if (! username)
    {
        username = configuration.getStringValue(ConfigurationKey::DEPRECATED_HSM_USERNAME);
    }
    auto password = configuration.getOptionalSafeStringValue(ConfigurationKey::HSM_WORK_PASSWORD);
    if (! password)
    {
        password = configuration.getSafeStringValue(ConfigurationKey::DEPRECATED_HSM_PASSWORD);
    }

    return HsmIdentity{.identityName = "work user",
                       .identity = Identity::Work,
                       .username = std::move(username.value()),
                       .password = std::move(password.value()),
                       .keyspec = configuration.getOptionalSafeStringValue(ConfigurationKey::HSM_WORK_KEYSPEC)};
}


HsmIdentity HsmIdentity::getSetupIdentity (void)
{
    const auto& configuration = Configuration::instance();
    auto username = configuration.getStringValue(ConfigurationKey::HSM_SETUP_USERNAME);
    auto password = configuration.getOptionalSafeStringValue(ConfigurationKey::HSM_SETUP_PASSWORD);
    if (! password)
    {
        password = configuration.getSafeStringValue(ConfigurationKey::DEPRECATED_HSM_PASSWORD);
    }
    return HsmIdentity{.identityName = "setup user",
                       .identity = Identity::Setup,
                       .username = std::move(username),
                       .password = std::move(password.value()),
                       .keyspec = configuration.getOptionalSafeStringValue(ConfigurationKey::HSM_SETUP_KEYSPEC)};
}


HsmIdentity HsmIdentity::getIdentity (Identity identity)
{
    switch(identity)
    {
        case Identity::Work:
            return getWorkIdentity();
        case Identity::Setup:
            return getSetupIdentity();
        default:
            Fail("unknown HSM identity");
    }
}


std::string HsmIdentity::displayName (void) const
{
    return identityName + " " + username;
}
