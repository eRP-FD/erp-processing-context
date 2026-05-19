// (C) Copyright IBM Deutschland GmbH 2021, 2025
// (C) Copyright IBM Corp. 2021, 2025
// non-exclusively licensed to gematik GmbH

#include "exporter/TransformerBase.hxx"
#include "ExporterRequirements.hxx"
#include "fhirtools/model/erp/ErpElement.hxx"
#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/views/FhirResourceViewList.hxx"
#include "fhirtools/transformer/ResourceProfileTransformer.hxx"
#include "model/DataAbsentReason.hxx"
#include "shared/fhir/Fhir.hxx"
#include "shared/model/Resource.hxx"
#include "shared/model/Timestamp.hxx"

model::NumberAsStringParserDocument TransformerBase::transformResource(
    const fhirtools::DefinitionKey& targetProfileKey, const model::FhirResourceBase& sourceResource,
    const std::vector<fhirtools::ValueMapping>& valueMappings, const std::vector<FixedValue>& fixedValues)
{
    const auto* backend = std::addressof(Fhir::instance().backend());
    auto repoView = getRepo(targetProfileKey);
    const auto* targetProfile = repoView->findStructure(targetProfileKey);
    Expect(targetProfile, "profile not found: " + to_string(targetProfileKey));

    model::NumberAsStringParserDocument targetResource;
    targetResource.CopyFrom(sourceResource.jsonDocument(), targetResource.GetAllocator());

    for (const auto& fixedValue : fixedValues)
    {
        targetResource.setValue(fixedValue.ptr, fixedValue.value);
    }

    const fhirtools::ProfiledElementTypeInfo baseTargetProfile{*targetProfile->baseType()};

    auto erpElement = std::make_shared<ErpElement>(backend, std::weak_ptr<const fhirtools::Element>{},
                                                   baseTargetProfile, &targetResource, &targetResource, nullptr);

    fhirtools::ResourceProfileTransformer::Options options;
    options.removeUnknownExtensionsFromOpenSlicing = true;
    fhirtools::ResourceProfileTransformer resourceProfileTransformer(repoView.get(), nullptr, targetProfile, options);
    resourceProfileTransformer.map(valueMappings);
    resourceProfileTransformer.map(
        {.from = to_string(sourceResource.getDefinitionKeyFromMeta()), .to = to_string(targetProfileKey)});

    auto result = resourceProfileTransformer.applyMappings(*erpElement);
    Expect(result.success, result.summary());
    erpElement = std::make_shared<ErpElement>(backend, std::weak_ptr<const fhirtools::Element>{}, baseTargetProfile,
                                              &targetResource, &targetResource, nullptr);

    A_25948.start("generic mapping of fhir resources");
    A_25949.start("generic extension takeover");
    result = resourceProfileTransformer.transform(repoView, *erpElement, targetProfileKey);
    // we possibly need to do two rounds, because transformation can unmask other validation errors.
    // e.g. adding data-absent-reason to numerator unmasks the constraint violation rat-1 (missing denominator)
    erpElement = std::make_shared<ErpElement>(backend, std::weak_ptr<const fhirtools::Element>{}, baseTargetProfile,
                                              &targetResource, &targetResource, nullptr);
    result.merge(resourceProfileTransformer.transform(repoView, *erpElement, targetProfileKey));
    A_25949.finish();
    A_25948.finish();

    Expect(result.success, result.summary());
    return targetResource;
}
void TransformerBase::checkSetAbsentReason(model::NumberAsStringParserDocument& document, const std::string& memberPath,
                                           const std::string_view valueCode)
{
    if (rapidjson::Pointer(memberPath).Get(document) == nullptr)
    {
        document.setValue(rapidjson::Pointer(memberPath + "/extension/0"),
                          model::DataAbsentReason(valueCode).jsonDocument());
    }
}

std::shared_ptr<const fhirtools::FhirStructureRepositoryView>
TransformerBase::getRepo(const fhirtools::DefinitionKey& profileKey)
{
    auto viewList = Fhir::instance().structureRepository(model::Timestamp::now());
    auto repoView = viewList.match(profileKey);
    Expect(repoView, "No repository view for " + to_string(profileKey));
    return repoView;
}
void TransformerBase::remove(rapidjson::Value& from, const rapidjson::Pointer& toBeRemoved)
{
    toBeRemoved.Erase(from);
}
