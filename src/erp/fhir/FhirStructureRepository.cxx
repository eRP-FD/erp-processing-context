/*
 * (C) Copyright IBM Deutschland GmbH 2021
 * (C) Copyright IBM Corp. 2021
 */

#include "FhirStructureRepository.hxx"

#include "erp/fhir/Fhir.hxx"
#include "erp/fhir/internal/FhirStructureDefinitionParser.hxx"
#include "erp/util/Expect.hxx"

#include <boost/algorithm/string.hpp>
#include <set>

class FhirStructureRepository::Verifier
{
public:
    explicit Verifier(const FhirStructureRepository& repo)
        : mRepo{repo}
    {}

    void verify()
    {
        for (const auto& def: mRepo.mDefinitionsByUrl)
        {
            verifyStructure(*def.second);
        }
        Expect3(unresolvedBase.empty(),
                R"(Could not resolve FHIR baseDefinitions: [")" + boost::join(unresolvedBase, R"(", ")") + R"("])",
                std::logic_error);
        Expect3(missingType.empty(),
                R"(Could not find FHIR types: [")" + boost::join(missingType, R"(", ")") + R"("])", std::logic_error);
        Expect3(elementsWithUnresolvedType.empty(),
                R"(Could not resolve FHIR types for: [")" + boost::join(elementsWithUnresolvedType, R"(", ")") + R"("])",
                std::logic_error);
    }
private:
    void verifyStructure(const FhirStructureDefinition& def)
    {
        const auto& baseDefinition = def.baseDefinition();
        if (!baseDefinition.empty())
        {
            const auto* baseType = mRepo.findDefinitionByUrl(baseDefinition);
            if (!baseType)
            {
                unresolvedBase.insert(baseDefinition);
            }
        }
        for (const auto& element: def.elements())
        {
            Expect3(element != nullptr, "ElementDefinition must not be null.", std::logic_error);
            verifyElement(def, *element);
        }
    }

    void verifyElement(const FhirStructureDefinition& def, const FhirElement& element)
    {
        using boost::starts_with;
        if (element.name() != def.typeId())
        {
            const auto& elementType = element.typeId();
            if (elementType.empty())
            {
                verifyContentReference(def, element);
            }
            else if (not starts_with(elementType, "http://hl7.org/fhirpath/System."))
            {
                const auto* typeDefinition = mRepo.findTypeById(elementType);
                if (!typeDefinition)
                {
                    missingType.insert(elementType);
                }
            }
        }
    }

    void verifyContentReference(const FhirStructureDefinition& def, const FhirElement& element)
    {
        if (element.contentReference().empty())
        {
            elementsWithUnresolvedType.emplace(def.url() + "@" + element.name());
        }
        else
        {
            try
            {
                (void) mRepo.resolveContentReference(element);
            }
            catch (const std::logic_error&)
            {
                elementsWithUnresolvedType.emplace(def.url() + "@" + element.name());
            }
        }
    }

    const FhirStructureRepository& mRepo;
    std::set<std::string> unresolvedBase;
    std::set<std::string> missingType;
    std::set<std::string> elementsWithUnresolvedType;

};

FhirStructureRepository::FhirStructureRepository()
{
    addSystemTypes();
}

void FhirStructureRepository::load(const std::list<std::filesystem::path>& files)
{
    TVLOG(1) << "Loading FHIR structure definitions.";
    for (const auto& f: files)
    {
        auto definitions = FhirStructureDefinitionParser::parse(f);
        for (auto&& def : definitions)
        {
            addDefinition(std::make_unique<FhirStructureDefinition>(std::move(def)));
        }
    }
    verify();
}

const FhirStructureDefinition*
FhirStructureRepository::findDefinitionByUrl(const std::string& url) const
{
    const auto& item = mDefinitionsByUrl.find(url);
    return (item != mDefinitionsByUrl.end()) ? (item->second.get()) : (nullptr);
}

const FhirStructureDefinition*
FhirStructureRepository::findTypeById(const std::string& typeName) const
{
    auto item = mDefinitionsByTypeId.find(typeName);
    if (item == mDefinitionsByTypeId.end() && !typeName.empty())
    {
        const auto& ctype = std::use_facet<std::ctype<char>>(std::locale::classic());
        std::string typeNameLower = typeName;
        typeNameLower[0] = ctype.tolower(typeNameLower[0]);
        item = mDefinitionsByTypeId.find(typeNameLower);
    }
    return (item != mDefinitionsByTypeId.end()) ? (item->second) : (nullptr);
}

FhirStructureRepository::ContentReferenceResolution
FhirStructureRepository::resolveContentReference(const FhirElement& element) const
{
    using namespace std::string_literals;
    const auto& contentReference = element.contentReference();
    auto dot = contentReference.find('.');
    Expect3(dot != std::string::npos, "Missing dot in contentReference: "s.append(contentReference) , std::logic_error);
    const auto& elementName = contentReference.substr(1); // remove prefix '#'
    const std::string baseTypeId{elementName.substr(0, dot - 1)};
    const auto* baseType = findTypeById(baseTypeId);
    Expect3(baseType != nullptr,
            "Failed to find referenced type (" + baseTypeId + ") for: "s.append(contentReference),
            std::logic_error);
    const auto& baseElements = baseType->elements();
    const auto& refElement = find_if(baseElements.begin(), baseElements.end(),
                [&](const std::shared_ptr<const FhirElement>& element) {
                    return element->name() == elementName;
                });
    Expect3(refElement != baseElements.end() && *refElement != nullptr,
            "referenced Element not found: "s.append(contentReference),
            std::logic_error);
    const auto* refElementType = findTypeById((*refElement)->typeId());
    Expect3(refElementType != nullptr, "Failed to resolve type of referenced element: " + (*refElement)->name(),
            std::logic_error);
    auto elementIndex = gsl::narrow<size_t>(refElement - baseElements.begin());
    auto newRefElement = FhirElement::Builder{**refElement}.isArray(element.isArray()).getAndReset();
    return {*baseType, *refElementType, elementIndex, std::move(newRefElement)};
}


void FhirStructureRepository::addSystemTypes()
{
    using K = FhirStructureDefinition::Kind;
    // clang-format off
    mSystemTypeBoolean  = addSystemType(K::systemBoolean, Fhir::systemTypeBoolean);
    mSystemTypeString   = addSystemType(K::systemString , Fhir::systemTypeString);
    mSystemTypeDate     = addSystemType(K::systemString , Fhir::systemTypeDate);
    mSystemTypeTime     = addSystemType(K::systemString , Fhir::systemTypeTime);
    mSystemTypeDateTime = addSystemType(K::systemString , Fhir::systemTypeDateTime);
    mSystemTypeDecimal  = addSystemType(K::systemDouble , Fhir::systemTypeDecimal);
    mSystemTypeInteger  = addSystemType(K::systemInteger, Fhir::systemTypeInteger);
    // clang-format on
}

const FhirStructureDefinition* FhirStructureRepository::addSystemType(FhirStructureDefinition::Kind kind, const std::string_view& url)
{
    std::string urlString{url};
    using namespace std::string_literals;
    FhirStructureDefinition::Builder builder;
    builder.kind(kind)
           .typeId(urlString)
           .url(urlString)
           .addElement(FhirElement::Builder{}.name(urlString).isArray(true).getAndReset())
           .addElement(FhirElement::Builder{}.name(urlString + ".value"s).typeId(urlString).getAndReset());
    return addDefinition(std::make_unique<FhirStructureDefinition>(builder.getAndReset()));
}

const FhirStructureDefinition* FhirStructureRepository::addDefinition(std::unique_ptr<FhirStructureDefinition>&& definition)
{
    if (definition->derivation() != FhirStructureDefinition::Derivation::constraint)
    {
        auto [iter, inserted] = mDefinitionsByTypeId.try_emplace(definition->typeId(), definition.get());
        Expect3(inserted, "Duplicate type: " + definition->typeId(), std::logic_error);
    }
    { // scope
        auto [iter, inserted] = mDefinitionsByUrl.try_emplace(definition->url(), std::move(definition));
        if (!inserted)
        {
            Fail("Duplicate url: " + definition->url());// NOLINT[bugprone-use-after-move,hicpp-invalid-access-moved]
        }
        return iter->second.get();
    }
}

void FhirStructureRepository::verify()
{
    Verifier{*this}.verify();
}
