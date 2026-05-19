/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef ERP_TOOLS_FHIRINSTALL_FHIRINSTALLERCONFIG_HXX
#define ERP_TOOLS_FHIRINSTALL_FHIRINSTALLERCONFIG_HXX


#include "tools/fhirinstall/FhirPackage.hxx"

#include <filesystem>
#include <optional>
#include <map>
#include <span>
#include <string>
#include <string_view>

struct FhirInstallArgs {
    FhirInstallArgs(const std::span<const char*>& args);

    // see app help for a description of the substitution format
    void addSubstitution(std::string_view substitution);

    bool tree = false;
    std::filesystem::path cacheFolder;
    std::filesystem::path outputFolder;
    std::optional<std::filesystem::path> configFolder;
    std::optional<std::filesystem::path> excludeFileFolder;
    FhirPackage::PtrSet packages;
    std::map<std::string, FhirPackage::Id> substitutions;

};

#endif// ERP_TOOLS_FHIRINSTALL_FHIRINSTALLERCONFIG_HXX
