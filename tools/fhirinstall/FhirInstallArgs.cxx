/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "tools/fhirinstall/FhirInstallArgs.hxx"
#include "fhirtools/FPExpect.hxx"

FhirInstallArgs::FhirInstallArgs(const std::span<const char*>& args)
{
    // skip appname:
    auto argPtr = std::ranges::next(args.begin(), 1, args.end());
    std::optional<std::filesystem::path> cacheFolderOption;
    std::optional<std::filesystem::path> outputFolderOption;
    for (; argPtr != args.end(); ++argPtr)
    {
        const std::string_view arg{*argPtr};
        if (arg == "-p")
        {
            ++argPtr;
            Expect3(argPtr != args.end(), "missing package_cache_folder argument to `-p`", ShowHelp);
            Expect3(! cacheFolderOption.has_value(), "`-p` supported only once", ShowHelp);
            cacheFolderOption.emplace(*argPtr);
        }
        else if (arg == "-o")
        {
            ++argPtr;
            Expect3(argPtr != args.end(), "missing output_folder argument to `-o`", ShowHelp);
            Expect3(! outputFolderOption.has_value(), "`-o` supported only once", ShowHelp);
            Expect3(! tree, "`--tree` and `-o` are mutually exclusive", ShowHelp);
            outputFolderOption.emplace(*argPtr);
        }
        else if (arg == "-c")
        {
            ++argPtr;
            Expect3(argPtr != args.end(), "missing config_folder argument to `-c`", ShowHelp);
            Expect3(! configFolder.has_value(), "`-c` supported only once", ShowHelp);
            configFolder.emplace(*argPtr);
        }
        else if (arg == "-x")
        {
            ++argPtr;
            Expect3(argPtr != args.end(), "missing excludefile_folder argument to `-x`", ShowHelp);
            Expect3(! excludeFileFolder.has_value(), "`-x` supported only once", ShowHelp);
            excludeFileFolder.emplace(*argPtr);
        }
        else if (arg == "--tree")
        {
            Expect3(! outputFolderOption.has_value(), "`--tree` and `-o` are mutually exclusive", ShowHelp);
            tree = true;
        }
        else if (arg == "-s")
        {
            ++argPtr;
            Expect3(argPtr != args.end(), "missing substitution argument to `-s`", ShowHelp);
            addSubstitution(*argPtr);
        }
        else if (arg == "--")
        {
            ++argPtr;
            break;
        }
        else if (! arg.starts_with('-'))
        {
            packages.emplace(FhirPackage::get(FhirPackage::Id{arg}));
        }
    }
    for (; argPtr != args.end(); ++argPtr)
    {
        const std::string_view arg{*argPtr};
        packages.emplace(FhirPackage::get(FhirPackage::Id{arg}));
    }

    Expect3(cacheFolderOption.has_value(), "missing package cache folder `-p` argument", ShowHelp);
    cacheFolder = std::move(*cacheFolderOption);

    Expect3(outputFolderOption.has_value() || tree, "missing output folder `-o` or `--tree` argument", ShowHelp);
    if (outputFolderOption)
    {
        outputFolder = std::move(*outputFolderOption);
    }
    Expect3(! packages.empty(), "no pacakges selected", ShowHelp);
}

void FhirInstallArgs::addSubstitution(std::string_view substitution)
{
    using namespace std::string_literals;
    auto equalSign = substitution.find('=');
    Expect3(equalSign != std::string_view::npos, "missing equal sign in substitution: "s.append(substitution), ShowHelp);
    const auto source = substitution.substr(0, equalSign);
    Expect3(!source.empty(), "missing package ref before equal sign in substitution: "s.append(substitution), ShowHelp);
    const auto sourcePackage = source.substr(0, source.find('@'));
    Expect3(!sourcePackage.empty(), "package name before equal sign in substitution: "s.append(substitution), ShowHelp);

    const auto target = substitution.substr(equalSign + 1);
    Expect3(!target.empty(), "missing package ref after equal sign in substitution: "s.append(substitution), ShowHelp);
    const auto targetAtSign = target.find('@');
    Expect3(targetAtSign != std::string_view::npos,
            "missing At-Sign (@) in package ref after equal sign in substitution: "s.append(substitution), ShowHelp);
    std::string targetPackage{target.substr(0, targetAtSign)};
    if (targetPackage.empty())
    {
        targetPackage = sourcePackage;
    }
    std::string targetVersion{target.substr(targetAtSign + 1)};
    Expect3(!targetVersion.empty(), "missing package version after equal sign in substitution: "s.append(substitution), ShowHelp);

    auto subst = substitutions.emplace(
        std::piecewise_construct, std::forward_as_tuple(source),
        std::forward_as_tuple(std::move(targetPackage), fhirtools::FhirVersion{std::move(targetVersion)}));
    Expect3(subst.second, "ambiguous substitution: "s.append(substitution), ShowHelp);
}
