#include "shared/model/MedicationDispenseOperationParameters.hxx"
#include "shared/model/GemErpPrMedication.hxx"
#include "shared/model/MedicationDispense.hxx"
#include "shared/ErpRequirements.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/ResourceNames.hxx"
#include "shared/util/Expect.hxx"


namespace model
{
namespace
{
std::string_view metaProfileFor(const rapidjson::Value& rxDispensation, const std::string_view partName)
{
    using namespace model::resource;
    static const rapidjson::Pointer metaProfilePointer{ElementName::path(elements::meta, elements::profile)};
    using namespace std::string_literals;
    const auto* part = ParameterBase::findPart(rxDispensation, partName);
    ModelExpect(part != nullptr,
                "part "s.append(partName).append(
                    " not found in paremeter rxDispensation of MedicationDispenseOperationParameters."));
    const auto* partResource = ParameterBase::getResource(*part);
    ModelExpect(partResource != nullptr,
                "part "s.append(partName).append(" in paremeter rxDispensation of "
                                                 "MedicationDispenseOperationParameters is not a resource."));
    const auto* partProfileArrayValue = metaProfilePointer.Get(*partResource);
    ModelExpect(
        partProfileArrayValue != nullptr && NumberAsStringParserDocument::valueIsArray(*partProfileArrayValue),
        "missing or invalid meta.profile in part "s.append(partName).append(" in paremeter rxDispensation of "
                                                                            "MedicationDispenseOperationParameters"));
    const auto& partProfileArray = partProfileArrayValue->GetArray();
    ModelExpect(
        partProfileArray.Size() == 1,
        "meta.profile in part "s.append(partName).append(
            " in paremeter rxDispensation of MedicationDispenseOperationParameters must have exactly 1 entry."));
    const auto& profile = partProfileArray[0];
    ModelExpect(NumberAsStringParserDocument::valueIsString(profile),
                "meta.profile in part "s.append(partName).append(
                    " in paremeter rxDispensation of MedicationDispenseOperationParameters must be string."));
    return NumberAsStringParserDocument::valueAsString(profile);
}
}

model::ProfileType model::MedicationDispenseOperationParameters::profileType() const
{
    using namespace std::string_literals;
    const auto& profileName = getProfileName();
    ModelExpect(profileName.has_value(), "missing meta.profile");
    fhirtools::DefinitionKey myKey{*profileName};
    if (myKey.url == model::resource::structure_definition::closeOperationInput)
    {
        return ProfileType::GEM_ERP_PR_PAR_CloseOperation_Input;
    }
    else if (myKey.url == model::resource::structure_definition::dispenseOperationInput)
    {
        return ProfileType::GEM_ERP_PR_PAR_DispenseOperation_Input;
    }
    ModelFail("Invalid profile for MedicationDispenseOperationParameters: "s.append(*profileName));
}

gsl::not_null<std::shared_ptr<const fhirtools::FhirStructureRepository>>
MedicationDispenseOperationParameters::getValidationView() const
{
    // find a view that contains all of Parameter profile, MedicationDispense profile and Medication profile
    const auto& profileName = getProfileName();
    ModelExpect(profileName.has_value(), "missing meta.profile");
    fhirtools::DefinitionKey myKey{*profileName};
    std::string profiles{*profileName};
    ModelExpect(myKey.version.has_value(), "missing profile version in meta.profile");
    const auto& fhirInstance = Fhir::instance();
    const auto& backend = fhirInstance.backend();
    auto views = fhirInstance.structureRepository(model::Timestamp::now());
    views = views.matchAll(backend, myKey.url, *myKey.version);
    // we cannot use `findParameter` because `rxDispensation` might appear more than once
    const auto* parameterArrayValue = parameterArrayPointer.Get(jsonDocument());
    if (parameterArrayValue)
    {
        ModelExpect(parameterArrayValue->IsArray(), "Parameters.parameter must be array.");
        const auto parameterArray = parameterArrayValue->GetArray();
        ModelExpect(! parameterArray.Empty(), "Parameters.parameter array must not be empty.");
        const auto& rxDispensation = parameterArray[0];
        const auto medicationDispenseProfile =
            metaProfileFor(rxDispensation, resource::parameter::part::medicationDispense);
        fhirtools::DefinitionKey dispenseKey{medicationDispenseProfile};
        ModelExpect(dispenseKey.version.has_value(), "Missing version for medicationDispense profile");
        views = views.matchAll(backend, dispenseKey.url, *dispenseKey.version);
        profiles.append(", ").append(medicationDispenseProfile);
        if (nullptr != findPart(rxDispensation, resource::parameter::part::medication))
        {
            const auto medicationProfile = metaProfileFor(rxDispensation, resource::parameter::part::medication);
            fhirtools::DefinitionKey medicationKey{medicationProfile};
            ModelExpect(medicationKey.version.has_value(), "Missing version for medication profile");
            views = views.matchAll(backend, medicationKey.url, *medicationKey.version);
            profiles.append(", ").append(medicationProfile);
        }
    }
    ModelExpect(! views.empty(), "no applicable view to validate resource containing profiles: " + profiles);
    return views.latest().view(&backend);
}


std::optional<model::Timestamp> model::MedicationDispenseOperationParameters::getValidationReferenceTimestamp() const
{
    Fail2("getValidationReferenceTimestamp not applicable for MedicationDispenseOperationParameters", std::logic_error);
}

std::list<MedicationDispense> MedicationDispenseOperationParameters::medicationDispensesDiga() const
{
    std::list<MedicationDispense> result;
    for (auto&& [medicationDispense, medication] : collectMedicationDispenses())
    {
        A_26003.start("Task schließen - Flowtype 162 - Profilprüfung MedicationDispense");
        ErpExpect(medicationDispense.profileType() == ProfileType::GEM_ERP_PR_MedicationDispense_DiGA,
                  HttpStatus::BadRequest,
                  "Unzulässige Abgabeinformationen: Für diesen Workflow sind nur Abgabeinformationen für digitale "
                  "Gesundheitsanwendungen zulässig.");
        A_26003.finish();
        ModelExpect(! medication.has_value(), "unexpected medication part in Parameters.parameter");
        result.emplace_back(std::move(medicationDispense));
    }
    return result;
}

std::list<std::pair<MedicationDispense, GemErpPrMedication>>
model::MedicationDispenseOperationParameters::medicationDispenses() const
{
    std::list<std::pair<MedicationDispense, GemErpPrMedication>> result;
    for (auto&& [medicationDispense, medication] : collectMedicationDispenses())
    {
        A_26002.start("Task schließen - Flowtype 160/169/200/209 - Profilprüfung MedicationDispense");
        ErpExpect(medicationDispense.profileType() == ProfileType::GEM_ERP_PR_MedicationDispense,
                  HttpStatus::BadRequest,
                  "Unzulässige Abgabeinformationen: Für diesen Workflow sind nur Abgabeinformationen für "
                  "Arzneimittel zulässig.");
        A_26002.finish();
        ModelExpect(medication.has_value(), "missing medication part in Parameters.parameter");
        result.emplace_back(std::move(medicationDispense), std::move(*medication));
    }
    return result;
}

std::list<std::pair<MedicationDispense, std::optional<GemErpPrMedication>>>
MedicationDispenseOperationParameters::collectMedicationDispenses() const
{
    const auto* parameterArrayValue = parameterArrayPointer.Get(jsonDocument());
    if (! parameterArrayValue)
    {
        return {};
    }
    ModelExpect(parameterArrayValue->IsArray(), "Parameters.parameter must be array.");
    std::list<std::pair<MedicationDispense, std::optional<GemErpPrMedication>>> result;
    for (auto parameterArray = parameterArrayValue->GetArray(); const auto& parameter : parameterArray)
    {
        const auto* rxDispensation = namePointer.Get(parameter);
        ModelExpect(NumberAsStringParserDocument::getStringValueFromValue(rxDispensation) ==
                        resource::parameter::rxDispensation,
                    "Parameter.parameter.name must be `rxDispensation`.");
        std::optional<MedicationDispense> medicationDispense;
        std::optional<GemErpPrMedication> medication;
        const auto* partArrayValue = partArrayPointer.Get(parameter);
        ModelExpect(partArrayValue != nullptr, "missing part in Parameters.parameter");
        ModelExpect(partArrayValue->IsArray(), "Parameters.parameter.part must be array.");
        for (auto partArray = partArrayValue->GetArray(); const auto& part : partArray)
        {
            auto partName = NumberAsStringParserDocument::getStringValueFromValue(namePointer.Get(part));
            if (partName == resource::parameter::part::medication)
            {
                ModelExpect(! medication.has_value(), "Duplicate medication in Parameters.parameter");
                medication.emplace(GemErpPrMedication::fromJson(getResourceDoc(part)));
            }
            else if (partName == resource::parameter::part::medicationDispense)
            {
                ModelExpect(! medicationDispense.has_value(), "Duplicate medicationDispense in Parameters.parameter");
                medicationDispense.emplace(MedicationDispense::fromJson(getResourceDoc(part)));
            }
            else
            {
                ModelFail("invalid part name: " + std::string{partName});
            }
        }
        ModelExpect(medicationDispense.has_value(), "missing medicationDispense part in Parameters.parameter");
        result.emplace_back(std::move(medicationDispense.value()), std::move(medication));
    }
    return result;
}

// NOLINTBEGIN(bugprone-exception-escape)
template class Resource<MedicationDispenseOperationParameters>;
template class Parameters<MedicationDispenseOperationParameters>;
// NOLINTEND(bugprone-exception-escape)

}
