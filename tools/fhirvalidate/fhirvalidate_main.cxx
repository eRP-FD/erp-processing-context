#include "erp/model/NumberAsStringParserDocument.hxx"
#include "erp/model/ResourceNames.hxx"

#include <erp/fhir/Fhir.hxx>
#include <erp/util/FileHelper.hxx>
#include <filesystem>
#include <memory>
#include <ranges>

#include "erp/model/ResourceFactory.hxx"
#include "test/util/StaticData.hxx"
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
            using Factory = model::ResourceFactory<model::UnspecifiedResource>;
            std::optional<Factory> factory;
            if (fileContent[startPos] == '<')
            {
                factory.emplace(Factory::fromXml(fileContent, *StaticData::getXmlValidator()));
            }
            else
            {
                factory.emplace(Factory::fromJson(fileContent, *StaticData::getJsonValidator()));
            }
            rapidjson::Pointer resourceTypePtr{
                model::resource::ElementName::path(model::resource::elements::resourceType)};
            (void) std::move(*factory).getValidated(SchemaType::fhir, model::ResourceVersion::allBundles());
        }
    }
    catch (const ErpException& erpException)
    {
        TLOG(ERROR) << "ERROR: " << erpException.what();
        if (erpException.diagnostics().has_value())
        {
            TLOG(WARNING) << "Diagnostics: " << *erpException.diagnostics();
        }
    }
    catch (const std::exception& e)
    {
        TLOG(ERROR) << "ERROR: " << e.what() << std::endl;
    }
    return EXIT_FAILURE;
}
