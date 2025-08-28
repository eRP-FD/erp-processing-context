add_executable(fhirtools-test
        fhirtools-test.cxx

        ClosedSliceExtensionsTest.cxx
        CodeCachingViewTest.cxx
        CodeSystemTest.cxx
        CollectionTest.cxx
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
        issues/ERP-12159-RefereceTypeCheck.cxx
        MandatoryMetaProfileTest.cxx
        ReferenceTest.cxx
        SlicingValidationTest.cxx
        UriValidationTest.cxx
        ValueMatchingTest.cxx
        VersionMappingViewTest.cxx
        )

target_link_libraries(fhirtools-test fhirtools gtest::gtest erp-processing-context-files erp-test-util-lib
        )
add_dependencies(test fhirtools-test)
