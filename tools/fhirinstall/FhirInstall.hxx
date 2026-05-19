/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#ifndef ERP_TOOLS_FHIRINSTALL_FHIRINSTALL_HXX
#define ERP_TOOLS_FHIRINSTALL_FHIRINSTALL_HXX

#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "tools/fhirinstall/FhirInstallArgs.hxx"
#include "tools/fhirinstall/FhirPackage.hxx"

#include <filesystem>
#include <optional>
#include <set>
#include <span>
#include <string_view>


class FhirInstall
{
public:
    static bool run(const std::span<const char*>& args);

    const fhirtools::FhirStructureRepositoryView& view() const;

private:
    explicit FhirInstall(const std::span<const char*>& args);
    bool run();
    bool printTree();
    bool install();
    void installConfig(const FhirPackage& package);

    void readArgs(const std::span<const char*>& args);

    FhirInstallArgs mArguments;
    fhirtools::FhirResourceGroupConst mBaseResourceGroup{"base"};
    fhirtools::FhirStructureRepositoryBackend mBackend;
    std::shared_ptr<const fhirtools::FhirStructureRepositoryView> mView;
    bool mHadError = false;
};


#endif// ERP_TOOLS_FHIRINSTALL_FHIRINSTALL_HXX
