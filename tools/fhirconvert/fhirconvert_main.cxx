/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "shared/model/Resource.hxx"
#include "fhirtools/converter/FhirConverter.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/model/NumberAsStringParserWriter.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/util/Expect.hxx"
#include "shared/util/FileHelper.hxx"

#include <boost/algorithm/string.hpp>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>

std::ostream& usage(std::ostream& out)
{
    out << "Usage: fhirconvert <input_file...>\n\n"
           "Convert files with FHIR resources from XML to JSON or vice versa.\n"
           "Converted resource is written to a file with appropriate extension replaced or appended.\n\n"
           "<input_file>  file containing FHIR resource; XML or JSON is auto detected.\n\n";
    return out;
}

std::filesystem::path makeOutname(std::filesystem::path path, std::string_view expect, std::string_view set)
{
    using namespace std::string_view_literals;
    if (boost::iequals(path.extension().string(), expect, std::locale::classic()))
    {
        path.replace_extension(set);
    }
    else
    {
        std::clog << "'"sv << path.extension().string() << "' != '"sv << expect << "'"sv;
        path.replace_extension(path.extension().string().append(set));
    }
    return path;
}

int main(int argc, char* argv[])
{
    using namespace std::string_view_literals;
    auto args = std::span(argv, size_t(argc));
    GLogConfiguration::initLogging(args[0]);
    Environment::set("ERP_SERVER_HOST", "none");
    try
    {
        auto owd = std::filesystem::current_path();
        auto here = std::filesystem::path(args[0]).remove_filename().native();
        if(int res = chdir(here.c_str()) != EXIT_SUCCESS)
        {
            return res;
        }
        Fhir::init<ConfigurationBase::ERP>();
        const auto& fhir = Fhir::instance();
        Expect(argc > 1, "Missing input filename.");
        for (std::filesystem::path origPath : args.subspan(1))
        {
            auto inFilePath = origPath;
            if (inFilePath.is_relative())
            {
                inFilePath = owd / inFilePath;
            }
            auto fileContent = FileHelper::readFileAsString(inFilePath);
            auto typeIndicator = fileContent.find_first_of("{<");
            Expect(typeIndicator != std::string::npos, "Unknown file type.");
            if (fileContent[typeIndicator] == '<')
            {
                auto outname = makeOutname(inFilePath, ".xml"sv, ".json"sv);
                std::clog << origPath << " -> " << outname << std::endl;
                rapidjson::StringBuffer buffer;
                model::NumberAsStringParserWriter<rapidjson::StringBuffer> writer{buffer};
                fhir.converter().xmlStringToJson(fileContent).Accept(writer);
                std::ofstream{outname} << buffer.GetString();
            }
            else
            {
                auto outname = makeOutname(inFilePath, ".json"sv, ".xml"sv);
                std::clog << origPath << " -> " << outname << std::endl;
                std::ofstream{outname} << fhir.converter().jsonToXmlString(
                    model::NumberAsStringParserDocument::fromJson(fileContent), true);
            }
        }
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << usage << "ERROR: " << e.what() << std::endl;
    }
    return EXIT_FAILURE;
}
