/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/hsm/HsmIdentity.hxx"

#include "erp/util/Configuration.hxx"


/**
 * During a hopefully short transition phase support the old environment/json names.
 * That concerns the ERP_WORK username and password and the ERP_SETUP password (but not its username).
 */


HsmIdentity HsmIdentity::getWorkIdentity (void)
{
    const auto& configuration = Configuration::instance();

    return HsmIdentity{
        "work user",
        Identity::Work,
        configuration.getOptionalStringValue(
            ConfigurationKey::HSM_WORK_USERNAME,
            configuration.getOptionalStringValue(
                ConfigurationKey::DEPRECATED_HSM_USERNAME,
                "ERP_WORK")),
        configuration.getOptionalSafeStringValue(
            ConfigurationKey::HSM_WORK_PASSWORD,
            configuration.getOptionalSafeStringValue(
                ConfigurationKey::DEPRECATED_HSM_PASSWORD,
                SafeString("password"))),
        configuration.getOptionalSafeStringValue(ConfigurationKey::HSM_WORK_KEYSPEC, {})};
}


HsmIdentity HsmIdentity::getSetupIdentity (void)
{
    const auto& configuration = Configuration::instance();
    return HsmIdentity{
        "setup user",
        Identity::Setup,
        configuration.getOptionalStringValue(ConfigurationKey::HSM_SETUP_USERNAME, "ERP_SETUP"),
        configuration.getOptionalSafeStringValue(
            ConfigurationKey::HSM_SETUP_PASSWORD,
            configuration.getOptionalSafeStringValue(
                ConfigurationKey::DEPRECATED_HSM_PASSWORD,
                SafeString("password"))),
        configuration.getOptionalSafeStringValue(ConfigurationKey::HSM_SETUP_KEYSPEC, {})};
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
