add_executable(fhirtools-test
        fhirtools-test.cxx

        ClosedSliceExtensionsTest.cxx
        CodeCachingViewTest.cxx
        CodeSystemTest.cxx
        CodeValidationTest.cxx
        CollectionTest.cxx
        ConverterTest.cxx
        DateTimeTest.cxx
        DefaultFhirStructureRepository.cxx
        ElementTest.cxx
        ExpressionTest.cxx
        FhirPathParserTest.cxx
        FhirPathValidatorTest.cxx
        FhirResourceViewConfigurationTest.cxx
        FhirResourceGroupConfigurationTest.cxx
        FhirResourceGroupConstTest.cxx
        FhirResourceGroupTest.cxx
        FhirResourceViewGroupSetTest.cxx
        FhirResourceViewVerifierTest.cxx
        FhirStructureDefinitionParserTest.cxx
        FhirStructureRepositoryFixerTest.cxx
        FhirStructureRepositoryTest.cxx
        FhirValueSetTest.cxx
        KbvSchluesseltabellenConfigurationTest.cxx
        MandatoryMetaProfileTest.cxx
        ReferenceTest.cxx
        PrimitiveTypeTest.cxx
        SlicingValidationTest.cxx
        UriValidationTest.cxx
        ValueMatchingTest.cxx
        VersionMappingViewTest.cxx
        issues/ERP-12159-RefereceTypeCheck.cxx
        issues/ERP-33046_profile_in_placeholder_element.cxx
        )

target_link_libraries(fhirtools-test fhirtools gtest::gtest erp-processing-context-files erp-test-util-lib
        )
add_dependencies(test fhirtools-test)
