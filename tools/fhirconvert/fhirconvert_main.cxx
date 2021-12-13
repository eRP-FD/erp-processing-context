/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "erp/fhir/FhirConverter.hxx"
#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/NumberAsStringParserWriter.hxx"
#include "erp/util/FileHelper.hxx"
#include "erp/util/Expect.hxx"


#include <iostream>


std::ostream& usage(std::ostream& out)
{
    out << "Usage: fhirconvert <input_file>\n\n"
           "Convert files with FHIR resources from XML to JSON or vice versa.\n"
           "Converted resource is printed to stdout.\n\n"
           "<input_file>  file containing FHIR resource; XML or JSON is auto detected.\n\n";
    return out;
}

int main(int argc, char* argv[])
{
    Environment::set("ERP_SERVER_HOST", "none");
    try {
        GLogConfiguration::init_logging(argv[0]);
        Expect(argc == 2, "Missing input filename.");
        auto fileContent = FileHelper::readFileAsString(argv[1]);
        auto typeIndicator = fileContent.find_first_of("{<");
        Expect(typeIndicator != std::string::npos, "Unknown file type.");
        if (fileContent[typeIndicator] == '<')
        {
            rapidjson::StringBuffer buffer;
            model::NumberAsStringParserWriter<rapidjson::StringBuffer> writer{buffer};
            FhirConverter{}.xmlStringToJson(fileContent).Accept(writer);
            std::cout << buffer.GetString();
        }
        else
        {
            std::cout << FhirConverter{}.jsonToXmlString(model::NumberAsStringParserDocument::fromJson(fileContent));
        }
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << usage << "ERROR: " << e.what() << std::endl;
    }
    return EXIT_FAILURE;
}
