
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/validator/FhirPathValidator.hxx"
#include "fhirtools/validator/ValidationResult.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/FileHelper.hxx"
#include "shared/model/ResourceFactory.hxx"
#include "test/util/StaticData.hxx"

#include <filesystem>
#include <memory>
#include <span>

void usage(std::string_view command, std::ostream& out)
{
    out << command << " <view_id> <filename>\n\n";
    out << " <view_id> id of one of the configured views\n";
    try {
        const auto& config = Configuration::instance();
        out << "           available views:\n";
        const auto& erpViewConfig = config.fhirResourceViewConfiguration<Configuration::ERP>();
        for (const auto& v : erpViewConfig.allViews())
        {
            out << "               " << v->mId << "\n";
        }
        const auto& meViewConfig = config.fhirResourceViewConfiguration<Configuration::MedicationExporter>();
        for (const auto& v : meViewConfig.allViews())
        {
            out << "               " << v->mId << "\n";
        }
    }
    catch (std::runtime_error&) // NOLINT(bugprone-empty-catch)
    {
    }
    catch (std::logic_error&) // NOLINT(bugprone-empty-catch)
    {
    }
}


template <config::ProcessType processType>
std::shared_ptr<const fhirtools::FhirStructureRepository> getView(const Configuration& config, std::string_view viewId)
{
    const auto viewList = config.fhirResourceViewConfiguration<processType>().allViews();
    auto viewCfg = std::ranges::find(viewList, viewId, &fhirtools::FhirResourceViewConfiguration::ViewConfig::mId);
    if (viewCfg != viewList.end())
    {
        Fhir::init<processType>();
        return (*viewCfg)->view(std::addressof(Fhir::instance().backend()));
    }
    return nullptr;
}

std::shared_ptr<const fhirtools::FhirStructureRepository> getView(std::string_view viewId)
{
    const auto& config = Configuration::instance();
    auto result = getView<Configuration::ERP>(config, viewId);
    if (result)
    {
        return result;
    }
    return getView<Configuration::MedicationExporter>(config, viewId);
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
        chdir(here.c_str());
        Expect(argc > 2, "Missing argument");
        auto view = getView(args[1]);
        Expect(view != nullptr, "no such view: " + std::string{args[1]});
        for (std::filesystem::path origPath : args.subspan(2))
        {
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
            (void) std::move(*factory).getValidated(model::ProfileType::fhir, view);
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
        usage(args[0], std::cerr);
    }
    return EXIT_FAILURE;
}
