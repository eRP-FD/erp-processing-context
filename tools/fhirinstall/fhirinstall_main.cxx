/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#include "shared/util/Environment.hxx"
#include "shared/util/GLogConfiguration.hxx"
#include "tools/fhirinstall/ShowHelp.hxx"
#include "tools/fhirinstall/FhirInstall.hxx"


#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <span>
#include <exception>
#include <string_view>

namespace
{

void usage(std::string_view app, std::ostream& out)
{
    out << "Usage: " << app << " -p <package_cache_folder> -o <output_folder> [-c <config_folder>] [--] <packages>...\n";
    out << "\n";
    out << "installs packages from fhirly terminal cache into output folder\n";
    out << "files are converted to xml if their source is json\n";
    out << "\n";
    out << "-p <package_cache_folder>\n";
    out << "        path to fhirly terminal package cache\n";
    out << "-o <output_folder>\n";
    out << "        path to package output folder\n";
    out << "-c <config_folder>\n";
    out << "        target folder for configuration file templates\n";
    out << "        Even though these files can be loaded, they probably need editing.\n";
    out << "        Be careful providing any folder that is read automatically. \n";
    out << "-x <excludefile_folder>\n";
    out << "        Folder containing files with name <package_name>-<package_version>.exclude.txt\n";
    out << "        The file contains one filename per line. Matching files will be excluded from installation.\n";
    out << "        Lines starting with `#` are ignored.\n";
    out << "-s <package_substitution>\n";
    out << "        Replace one package by a different package: \n";
    out << "        <package1>@<version1>=<package1>@<version2> : instead of <package1>@<version1> use <package2>@<version2>\n";
    out << "        <package>@<version1>=@<version2> : for <package> use version2 instead of version1\n";
    out << "        <package>=@<version> : for <package> always use <version>\n";
    out << "\n";
    out << "Example:\n";
    out << app << " -p ~/.fhir/packages -o resources/fhir/profiles -c resources -- kbv.ita.erp@1.1.1\n\n";
}

}

int main(int argc, const char* argv[])
{
    Environment::set("ERP_SERVER_HOST", "none");
    const std::span<const char*> args(argv, static_cast<size_t>(argc));
    if (std::ranges::any_of(args, [](auto arg){
        return arg == "-h" || arg == "--help";
    }))
    {
        usage(args[0], std::cout);
        return EXIT_SUCCESS;
    }
    GLogConfiguration::initLogging(args[0]);
    try
    {
        if (FhirInstall::run(args))
        {
            return EXIT_SUCCESS;
        }
        LOG(ERROR) << "operation completed with errors";
    }
    catch (const ShowHelp& ex)
    {
        usage(args[0], std::cerr);
        LOG(ERROR) << ex.what();
    }
    catch (const std::exception& ex)
    {
        LOG(ERROR) << ex.what();
    }
    return EXIT_FAILURE;
}
