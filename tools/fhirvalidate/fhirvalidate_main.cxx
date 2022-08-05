#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/ResourceNames.hxx"

#include <erp/fhir/Fhir.hxx>
#include <erp/util/FileHelper.hxx>
#include <filesystem>
#include <memory>
#include <ranges>

#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/ValidationResult.hxx"

int main(int argc, char* argv[])
{
    using namespace std::string_view_literals;
    GLogConfiguration::init_logging(argv[0]);
    Environment::set("ERP_SERVER_HOST", "none");
    try
    {
        auto owd = std::filesystem::current_path();
        auto here = std::filesystem::path(argv[0]).remove_filename().native();
        chdir(here.c_str());
        const auto& fhir = Fhir::instance();
        Expect(argc > 1, "Missing input filename.");
        for (int i = 1; i < argc; ++i)
        {
            std::filesystem::path origPath{argv[i]};
            auto inFilePath = origPath;
            if (inFilePath.is_relative())
            {
                inFilePath = owd / inFilePath;
            }
            auto fileContent = FileHelper::readFileAsString(inFilePath);

            auto startPos = fileContent.find_first_of("<{");
            Expect(startPos != std::string::npos, "File type detection failed");
            std::optional<model::NumberAsStringParserDocument> doc;
            if (fileContent[startPos] == '<')
            {
                doc.emplace(Fhir::instance().converter().xmlStringToJson(fileContent));
            }
            else
            {
                doc.emplace(model::NumberAsStringParserDocument::fromJson(fileContent));
            }
            rapidjson::Pointer resourceTypePtr{
                model::resource::ElementName::path(model::resource::elements::resourceType)};
            auto resourceType = model::NumberAsStringParserDocument::getOptionalStringValue(*doc, resourceTypePtr);
            Expect(resourceType.has_value(), "Could not detect resourceType.");
            auto rootElement =
                std::make_shared<ErpElement>(&fhir.structureRepository(), std::weak_ptr<const ErpElement>{},
                                             std::string{*resourceType}, &doc.value());
            auto validationResult =
                fhirtools::FhirPathValidator::validate({rootElement}, std::string{*resourceType},
                                                       {.reportUnknownExtensions = true});

            validationResult.dumpToLog();
        }
    }
    catch (const std::exception& e)
    {
        TLOG(ERROR) << "ERROR: " << e.what() << std::endl;
    }
    return EXIT_FAILURE;
}
