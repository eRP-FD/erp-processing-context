/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */
#ifndef FHIRTOOLS_MOCKFHIRRESOURCEVIEW_HXX_INCLUDED
#define FHIRTOOLS_MOCKFHIRRESOURCEVIEW_HXX_INCLUDED

#include "fhirtools/repository/DefinitionKey.hxx"
#include "fhirtools/repository/views/FhirStructureRepositoryView.hxx"
#include "fhirtools/typemodel/ProfiledElementTypeInfo.hxx"

#include <gmock/gmock.h>

namespace fhirtools::test
{

class MockFhirResourceView : public FhirStructureRepositoryView
{
public:
    static std::shared_ptr<MockFhirResourceView> create();
    static std::shared_ptr<MockFhirResourceView> createNice();

    MOCK_METHOD(std::string_view, id, (), (const, override));
    MOCK_METHOD(const FhirStructureDefinition*, findStructure, (const DefinitionKey& key), (const, override));
    MOCK_METHOD(const FhirStructureDefinition*, findTypeById, (const std::string& typeName), (const, override));
    MOCK_METHOD(const FhirValueSet*, findValueSet, (const DefinitionKey& key), (const, override));
    MOCK_METHOD(std::shared_ptr<const FhirValueSetCodes>, findValueSetCodes, (const DefinitionKey& key),
                (const override));
    MOCK_METHOD(const FhirCodeSystem*, findCodeSystem, (const DefinitionKey& key), (const, override));
    MOCK_METHOD(std::shared_ptr<const FhirCodeSystemCodes>, findCodeSystemCodes, (const DefinitionKey& key),
                (const override));
    MOCK_METHOD(std::set<const FhirCodeSystem*>, findSupplementers,
                (const std::string& url, const FhirVersion& version), (const, override));
    MOCK_METHOD(std::optional<DefinitionKey>, findRenderVersion, (const DefinitionKey& key), (const override));
    MOCK_METHOD(const FhirStructureDefinition*, systemTypeBoolean, (), (const, override));
    MOCK_METHOD(const FhirStructureDefinition*, systemTypeString, (), (const, override));
    MOCK_METHOD(const FhirStructureDefinition*, systemTypeDate, (), (const, override));
    MOCK_METHOD(const FhirStructureDefinition*, systemTypeTime, (), (const, override));
    MOCK_METHOD(const FhirStructureDefinition*, systemTypeDateTime, (), (const, override));
    MOCK_METHOD(const FhirStructureDefinition*, systemTypeDecimal, (), (const, override));
    MOCK_METHOD(const FhirStructureDefinition*, systemTypeInteger, (), (const, override));
    MOCK_METHOD(ContentReferenceResolution, resolveContentReference,
                (const FhirResourceGroup& group, const FhirElement& element), (const, override));
    MOCK_METHOD((std::tuple<ProfiledElementTypeInfo, std::size_t>), resolveBaseContentReference,
                (std::string_view elementId), (const, override));

    ~MockFhirResourceView() override = default;
private:
    MockFhirResourceView() = default;
};


inline std::shared_ptr<MockFhirResourceView> MockFhirResourceView::create()
{
    struct Construct : MockFhirResourceView {
        Construct() = default;
    };
    return std::make_shared<Construct>();
}

inline std::shared_ptr<MockFhirResourceView> MockFhirResourceView::createNice()
{
    struct Construct : MockFhirResourceView {
        Construct() = default;
    };
    return std::make_shared<testing::NiceMock<Construct>>();
}

}

#endif// FHIRTOOLS_MOCKFHIRRESOURCEVIEW_HXX_INCLUDED
