/*
 * (C) Copyright IBM Deutschland GmbH 2021, 2025
 * (C) Copyright IBM Corp. 2021, 2025
 *
 * non-exclusively licensed to gematik GmbH
 */

#include "fhirtools/converter/internal/FhirJsonToXmlConverter.hxx"
#include "fhirtools/model/NumberAsStringParserDocument.hxx"
#include "fhirtools/repository/FhirStructureRepository.hxx"
#include "fhirtools/repository/groups/FhirResourceGroupConst.hxx"
#include "shared/util/FileHelper.hxx"
#include "test/util/ResourceManager.hxx"

#include <gtest/gtest.h>
#include <fstream>

TEST(FhirConverter, resourceArray)
{
    const auto& input = FileHelper::readFileAsString(
        ResourceManager::getAbsoluteFilename("test/fhir-path/samples/conversion/resourceArray.json"));
    const auto& expected = FileHelper::readFileAsString(
        ResourceManager::getAbsoluteFilename("test/fhir-path/samples/conversion/resourceArray.xml"));
    fhirtools::FhirStructureRepositoryBackend backend;
    fhirtools::FhirResourceGroupConst resolver{"FhirConverter-resourceArray)"};
    backend.load({ResourceManager::getAbsoluteFilename("test/fhir-path/profiles/minifhirtypes.xml")}, resolver);
    const auto view = backend.defaultView();
    const auto jsonDom = model::NumberAsStringParserDocument::fromJson(input);
    const auto xmlDom = FhirJsonToXmlConverter::jsonToXml(*view, jsonDom);
    std::unique_ptr<xmlChar[], void (*)(void*)> buffer{nullptr, xmlFree};//NOLINT(cppcoreguidelines-avoid-c-arrays)
    int size = 0;
    xmlChar* rawBuffer = nullptr;
    xmlDocDumpFormatMemoryEnc(xmlDom.get(), &rawBuffer, &size, "utf-8", 1);
    buffer.reset(rawBuffer);
    ASSERT_GT(size, 0);
    ASSERT_NE(rawBuffer, nullptr);
    std::string actual{reinterpret_cast<const char*>(buffer.get()), gsl::narrow<size_t>(size)};
    EXPECT_EQ(actual, expected);
}
